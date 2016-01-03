#include "engine/executor.h"
#include "engine/task_graph.h"
#include "engine/task_function.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations_internal.h"
#include "engine/channel_mixer.h"
#include <algorithm>

c_executor::c_executor() {
	m_state.initialize(k_state_uninitialized);
	m_sample_library.initialize("./");
}

void c_executor::initialize(const s_executor_settings &settings) {
	wl_assert(m_state.get_unsafe() == k_state_uninitialized);

	initialize_internal(settings);

	IF_ASSERTS_ENABLED(int32 old_state = ) m_state.exchange(k_state_initialized);
	wl_assert(old_state == k_state_uninitialized);
}

void c_executor::shutdown() {
	// If we are only initialized, we can shutdown immediately
	int32 old_state = m_state.compare_exchange(k_state_initialized, k_state_uninitialized);
	if (old_state == k_state_uninitialized) {
		// Nothing to do
		return;
	} else if (old_state != k_state_initialized) {
		// If we're not initialized, then we must be running. We must wait for the stream to flush before shutting down.
		wl_assert(old_state == k_state_running);

		IF_ASSERTS_ENABLED(int32 old_state_verify = ) m_state.exchange(k_state_terminating);
		wl_assert(old_state_verify == k_state_running);

		// Now wait until we get a notification from the stream
		m_shutdown_signal.wait();
		wl_assert(m_state.get_unsafe() == k_state_uninitialized);
	}

	shutdown_internal();
}

void c_executor::execute(const s_executor_chunk_context &chunk_context) {
	// Try flipping from initialized to running. Nothing will happen if we're not in the initialized state.
	m_state.compare_exchange(k_state_initialized, k_state_running);

	// Try flipping from terminating to uninitialized
	int32 old_state = m_state.compare_exchange(k_state_terminating, k_state_uninitialized);

	if (old_state == k_state_running) {
		execute_internal(chunk_context);
	} else {
		// Zero buffer if disabled
		zero_output_buffers(chunk_context.frames, chunk_context.output_channels,
			chunk_context.sample_format, chunk_context.output_buffers);

		if (old_state == k_state_terminating) {
			// We successfully terminated, so signal the main thread
			m_shutdown_signal.notify();
		}
	}
}

void c_executor::initialize_internal(const s_executor_settings &settings) {
	m_task_graph = settings.task_graph;
	m_max_buffer_size = settings.max_buffer_size;

	initialize_thread_pool(settings);
	initialize_buffer_allocator(settings);
	initialize_task_memory(settings);
	initialize_tasks(settings);
	initialize_voice_contexts();
	initialize_task_contexts();
	initialize_buffer_contexts();
	initialize_profiler(settings);
}

void c_executor::initialize_thread_pool(const s_executor_settings &settings) {
	s_thread_pool_settings thread_pool_settings;
	thread_pool_settings.thread_count = settings.thread_count;
	thread_pool_settings.max_tasks = std::max(1u, m_task_graph->get_max_task_concurrency());
	thread_pool_settings.start_paused = true;
	m_thread_pool.start(thread_pool_settings);
}

void c_executor::initialize_buffer_allocator(const s_executor_settings &settings) {
	s_buffer_allocator_settings buffer_allocator_settings;
	buffer_allocator_settings.buffer_type = k_buffer_type_real;
	buffer_allocator_settings.buffer_size = settings.max_buffer_size;
	buffer_allocator_settings.buffer_count = m_task_graph->get_max_buffer_concurrency();

	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	// Allocate an additional buffer for each constant output. The task graph doesn't require buffers for these, but
	// at this point we do.
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		if (outputs[output].data.is_constant) {
			buffer_allocator_settings.buffer_count++;
		}
	}

	// Allocate an additional buffer for each "duplicated" output buffer. This is because we need to keep them
	// separate for accumulation.
	{
		std::vector<uint32> buffer_deduplicator;
		buffer_deduplicator.reserve(outputs.get_count());
		uint32 total_buffer_outputs = 0;
		uint32 unique_buffer_outputs = 0;
		for (uint32 output = 0; output < outputs.get_count(); output++) {

			if (!outputs[output].data.is_constant) {
				uint32 output_buffer_index = outputs[output].get_real_buffer_in();
				if (std::find(buffer_deduplicator.begin(), buffer_deduplicator.end(), output_buffer_index) ==
					buffer_deduplicator.end()) {
					unique_buffer_outputs++;
					buffer_deduplicator.push_back(output_buffer_index);
				}

				total_buffer_outputs++;
			}
		}

		uint32 duplicated_buffer_outputs = total_buffer_outputs - unique_buffer_outputs;
		buffer_allocator_settings.buffer_count += duplicated_buffer_outputs;
	}

	// If we support multiple voices, we need buffers to accumulate the final result into
	if (m_task_graph->get_globals().max_voices > 1) {
		buffer_allocator_settings.buffer_count += outputs.get_count();
	}

	// If the output count differs from the channel count, we'll need additional buffers to perform mixing
	if (settings.output_channels != outputs.get_count()) {
		buffer_allocator_settings.buffer_count += settings.output_channels;
	}

	m_buffer_allocator.initialize(buffer_allocator_settings);

	m_voice_output_accumulation_buffers.resize(outputs.get_count());
	m_channel_mix_buffers.resize(settings.output_channels);
}

void c_executor::initialize_task_memory(const s_executor_settings &settings) {
	uint32 max_voices = m_task_graph->get_globals().max_voices;
	size_t total_voice_task_memory_pointers = max_voices * m_task_graph->get_task_count();
	m_voice_task_memory_pointers.clear();
	m_voice_task_memory_pointers.reserve(total_voice_task_memory_pointers);

	// Since we initially store offsets, this value cast to a pointer represents null
	static const size_t k_invalid_memory_pointer = static_cast<size_t>(-1);

	size_t required_memory_per_voice = 0;
	for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
		const s_task_function &task_function =
			c_task_function_registry::get_task_function(m_task_graph->get_task_function_index(task));

		size_t required_memory_for_task = 0;
		if (task_function.memory_query) {
			// The memory query function will be able to read from the constant inputs.
			// We don't (currently) support dynamic task memory usage.

			// Set up the arguments
			s_task_function_argument arguments[k_max_task_function_arguments];
			c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(task);

			for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
				s_task_function_argument &argument = arguments[arg];
				ZERO_STRUCT(&argument);
				IF_ASSERTS_ENABLED(argument.data.type = argument_data[arg].data.type);
				argument.data.is_constant = argument_data[arg].is_constant();

				switch (argument_data[arg].data.type) {
				case k_task_data_type_real_in:
					if (argument.data.is_constant) {
						argument.data.value.real_constant_in = argument_data[arg].get_real_constant_in();
					}
					break;

				case k_task_data_type_real_out:
				case k_task_data_type_real_inout:
					break;

				case k_task_data_type_real_array_in:
					argument.data.value.real_array_in = argument_data[arg].get_real_array_in();
					break;

				case k_task_data_type_bool_in:
					argument.data.value.bool_constant_in = argument_data[arg].get_bool_constant_in();
					break;

				case k_task_data_type_bool_array_in:
					argument.data.value.bool_array_in = argument_data[arg].get_bool_array_in();
					break;

				case k_task_data_type_string_in:
					argument.data.value.string_constant_in = argument_data[arg].get_string_constant_in();
					break;

				case k_task_data_type_string_array_in:
					argument.data.value.string_array_in = argument_data[arg].get_string_array_in();
					break;

				default:
					wl_unreachable();
				}
			}

			s_task_function_context task_function_context;
			ZERO_STRUCT(&task_function_context);
			task_function_context.sample_rate = settings.sample_rate;
			task_function_context.arguments = c_task_function_arguments(arguments, argument_data.get_count());
			// $TODO fill in timing info, etc.

			required_memory_for_task = task_function.memory_query(task_function_context);
		}

		if (required_memory_for_task == 0) {
			m_voice_task_memory_pointers.push_back(reinterpret_cast<void *>(k_invalid_memory_pointer));
		} else {
			m_voice_task_memory_pointers.push_back(reinterpret_cast<void *>(required_memory_per_voice));

			required_memory_per_voice += required_memory_for_task;
			required_memory_per_voice = align_size(required_memory_per_voice, k_lock_free_alignment);
		}
	}

	// Allocate the memory so we can calculate the true pointers
	size_t total_required_memory = max_voices * required_memory_per_voice;
	m_task_memory_allocator.free();

	if (total_required_memory > 0) {
		m_task_memory_allocator.allocate(total_required_memory);
		size_t task_memory_base = reinterpret_cast<size_t>(m_task_memory_allocator.get_array().get_pointer());

		// Compute offset pointers
		for (size_t index = 0; index < m_voice_task_memory_pointers.size(); index++) {
			size_t offset = reinterpret_cast<size_t>(m_voice_task_memory_pointers[index]);
			void *offset_pointer = (offset == k_invalid_memory_pointer) ?
				nullptr :
				offset_pointer = reinterpret_cast<void *>(task_memory_base + offset);
			m_voice_task_memory_pointers[index] = offset_pointer;
		}

		// Now offset for each additional voice
		for (uint32 voice = 1; voice < max_voices; voice++) {
			size_t voice_offset = voice * m_task_graph->get_task_count();
			size_t voice_memory_offset = voice * required_memory_per_voice;
			for (size_t index = 0; index < m_voice_task_memory_pointers.size(); index++) {
				size_t base_pointer = reinterpret_cast<size_t>(m_voice_task_memory_pointers[index]);
				void *offset_pointer = (base_pointer == 0) ?
					nullptr :
					reinterpret_cast<void *>(base_pointer + voice_memory_offset);
				m_voice_task_memory_pointers[voice_offset + index] = offset_pointer;
			}
		}

		// Zero out all the memory - tasks can detect whether it is initialized by providing an "initialized" field
		memset(m_task_memory_allocator.get_array().get_pointer(), 0,
			m_task_memory_allocator.get_array().get_count());
	} else {
#if PREDEFINED(ASSERTS_ENABLED)
		// All pointers should be invalid
		for (size_t index = 0; index < m_voice_task_memory_pointers.size(); index++) {
			wl_assert(m_voice_task_memory_pointers[index] == reinterpret_cast<void *>(k_invalid_memory_pointer));
		}
#endif // PREDEFINED(ASSERTS_ENABLED)

		// All point to null
		m_voice_task_memory_pointers.clear();
		m_voice_task_memory_pointers.resize(total_voice_task_memory_pointers, nullptr);
	}
}

void c_executor::initialize_tasks(const s_executor_settings &settings) {
	m_sample_library.clear_requested_samples();

	for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
		const s_task_function &task_function =
			c_task_function_registry::get_task_function(m_task_graph->get_task_function_index(task));

		if (task_function.initializer) {
			// Set up the arguments
			s_task_function_argument arguments[k_max_task_function_arguments];
			c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(task);

			for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
				s_task_function_argument &argument = arguments[arg];
				ZERO_STRUCT(&argument);
				IF_ASSERTS_ENABLED(argument.data.type = argument_data[arg].data.type);
				argument.data.is_constant = argument_data[arg].is_constant();

				switch (argument_data[arg].data.type) {
				case k_task_data_type_real_in:
					if (argument.data.is_constant) {
						argument.data.value.real_constant_in = argument_data[arg].get_real_constant_in();
					}
					break;

				case k_task_data_type_real_out:
				case k_task_data_type_real_inout:
					break;

				case k_task_data_type_real_array_in:
					argument.data.value.real_array_in = argument_data[arg].get_real_array_in();
					break;

				case k_task_data_type_bool_in:
					argument.data.value.bool_constant_in = argument_data[arg].get_bool_constant_in();
					break;

				case k_task_data_type_bool_array_in:
					argument.data.value.bool_array_in = argument_data[arg].get_bool_array_in();
					break;

				case k_task_data_type_string_in:
					argument.data.value.string_constant_in = argument_data[arg].get_string_constant_in();
					break;

				case k_task_data_type_string_array_in:
					argument.data.value.string_array_in = argument_data[arg].get_string_array_in();
					break;

				default:
					wl_unreachable();
				}
			}

			c_sample_library_requester sample_library_requester(m_sample_library);
			s_task_function_context task_function_context;
			ZERO_STRUCT(&task_function_context);
			task_function_context.sample_requester = &sample_library_requester;
			task_function_context.sample_rate = settings.sample_rate;
			task_function_context.arguments = c_task_function_arguments(arguments, argument_data.get_count());
			// $TODO fill in timing info, etc.

			for (uint32 voice = 0; voice < m_task_graph->get_globals().max_voices; voice++) {
				task_function_context.task_memory =
					m_voice_task_memory_pointers[m_task_graph->get_task_count() * voice + task];
				task_function.initializer(task_function_context);
			}
		}
	}

	m_sample_library.update_loaded_samples();
}

void c_executor::initialize_voice_contexts() {
	m_voice_contexts.resize(m_task_graph->get_globals().max_voices);
	for (size_t voice = 0; voice < m_voice_contexts.size(); voice++) {
		s_voice_context &context = m_voice_contexts[voice];
		context.active = true; // $TODO should be false by default when we have note press events
	}
}

void c_executor::initialize_task_contexts() {
	m_task_contexts.free();
	m_task_contexts.allocate(m_task_graph->get_task_count());
}

void c_executor::initialize_buffer_contexts() {
	m_buffer_contexts.free();
	m_buffer_contexts.allocate(m_task_graph->get_buffer_count());
}

void c_executor::initialize_profiler(const s_executor_settings &settings) {
	m_profiling_enabled = settings.profiling_enabled;
	if (m_profiling_enabled) {
		s_profiler_settings profiler_settings;
		profiler_settings.worker_thread_count = settings.thread_count;
		profiler_settings.task_count = m_task_graph->get_task_count();
		m_profiler.initialize(profiler_settings);
		m_profiler.start();
	}
}

void c_executor::shutdown_internal() {
	IF_ASSERTS_ENABLED(uint32 unexecuted_tasks = ) m_thread_pool.stop();
	wl_assert(unexecuted_tasks == 0);

	if (m_profiling_enabled) {
		m_profiler.stop();
		s_profiler_report report;
		m_profiler.get_report(report);
		output_profiler_report("profiler_report.csv", report);
	}
}

void c_executor::execute_internal(const s_executor_chunk_context &chunk_context) {
	if (m_profiling_enabled) {
		m_profiler.begin_execution();
	}

	wl_assert(chunk_context.frames <= m_max_buffer_size); // $TODO we could do multiple passes to handle this...

	// Clear accumulation buffers
	std::fill(m_voice_output_accumulation_buffers.begin(), m_voice_output_accumulation_buffers.end(),
		k_lock_free_invalid_handle);
	std::fill(m_channel_mix_buffers.begin(), m_channel_mix_buffers.end(),
		k_lock_free_invalid_handle);

	// Process each active voice
	uint32 voices_processed = 0;
	for (uint32 voice = 0; voice < m_voice_contexts.size(); voice++) {
		if (!m_voice_contexts[voice].active) {
			continue;
		}

		process_voice(chunk_context, voice);

		if (voices_processed == 0) {
			// Swap the output buffers directly into the accumulation buffers for the first voice we process
			swap_output_buffers_with_accumulation_buffers(chunk_context);
		} else {
			add_output_buffers_to_accumulation_buffers(chunk_context);
		}

		assert_all_output_buffers_free();

		voices_processed++;
	}

	// If we didn't process any voices, allocate zero signal output buffers
	if (voices_processed == 0) {
		allocate_and_clear_output_buffers(chunk_context);
	}

	if (m_voice_output_accumulation_buffers.size() != chunk_context.output_channels) {
		// Mix the accumulation buffers into the channel buffers and free the accumulation buffers
		mix_accumulation_buffers_to_channel_buffers(chunk_context);
	} else {
		// Simply swap the accumulation buffers with the channel buffers
		std::swap(m_voice_output_accumulation_buffers, m_channel_mix_buffers);
	}

	{
		c_wrapped_array_const<uint32> channel_buffers(
			&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());
		convert_and_interleave_to_output_buffer(
			chunk_context.frames,
			m_buffer_allocator,
			channel_buffers,
			chunk_context.sample_format,
			chunk_context.output_buffers);
	}

	// Free channel buffers
	for (uint32 channel = 0; channel < chunk_context.output_channels; channel++) {
		m_buffer_allocator.free_buffer(m_channel_mix_buffers[channel]);
	}

	if (m_profiling_enabled) {
		m_profiler.end_execution();
	}
}

void c_executor::process_voice(const s_executor_chunk_context &chunk_context, uint32 voice) {
	// Setup each initial task predecessor count
	for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
		m_task_contexts.get_array()[task].predecessors_remaining.initialize(
			cast_integer_verify<int32>(m_task_graph->get_task_predecessor_count(task)));
	}

	// Initially all buffers are unassigned
	for (uint32 buffer = 0; buffer < m_task_graph->get_buffer_count(); buffer++) {
		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		buffer_context.handle = k_lock_free_invalid_handle;
		buffer_context.usages_remaining.initialize(m_task_graph->get_buffer_usages(buffer));
	}

	m_tasks_remaining.initialize(cast_integer_verify<int32>(m_task_graph->get_task_count()));

	// Add the initial tasks
	c_task_graph_task_array initial_tasks = m_task_graph->get_initial_tasks();
	if (initial_tasks.get_count() > 0) {
		for (size_t initial_task = 0; initial_task < initial_tasks.get_count(); initial_task++) {
			add_task(voice, initial_tasks[initial_task], chunk_context.sample_rate, chunk_context.frames);
		}

		if (m_profiling_enabled) {
			m_profiler.begin_execution_tasks();
		}

		// Start the threads processing
		m_thread_pool.resume();
		m_all_tasks_complete_signal.wait();
		m_thread_pool.pause();

		if (m_profiling_enabled) {
			m_profiler.end_execution_tasks();
		}
	}
}

void c_executor::swap_output_buffers_with_accumulation_buffers(const s_executor_chunk_context &chunk_context) {
	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	for (uint32 output = 0; output < outputs.get_count(); output++) {
		wl_assert(m_voice_output_accumulation_buffers[output] == k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() == k_task_data_type_real_in);
		if (outputs[output].is_constant()) {
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
			m_voice_output_accumulation_buffers[output] = buffer_handle;
			s_buffer_operation_assignment::in_out(
				chunk_context.frames,
				c_real_buffer_or_constant_in(outputs[output].get_real_constant_in()),
				m_buffer_allocator.get_buffer(buffer_handle));
		} else {
			// Swap out the buffer
			s_buffer_context &buffer_context =
				m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];

			wl_assert(buffer_context.usages_remaining.get_unsafe() >= 0);
			if (buffer_context.usages_remaining.get_unsafe() > 0) {
				// Set usage count to 0, but don't deallocate, because we're swapping it into the accumulation
				// buffer slot. Don't clear the handle though, because it may be used as another output.
				buffer_context.usages_remaining.initialize(0);
				m_voice_output_accumulation_buffers[output] = buffer_context.handle;
			} else {
				// This case occurs when a single buffer is used for multiple outputs, and the buffer was
				// already swapped to an accumulation slot. In this case, we must actually allocate an
				// additional buffer and copy to it, because we need to keep the data separate both
				// accumulations.
				uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
				m_voice_output_accumulation_buffers[output] = buffer_handle;
				s_buffer_operation_assignment::in_out(
					chunk_context.frames,
					c_real_buffer_or_constant_in(m_buffer_allocator.get_buffer(buffer_context.handle)),
					m_buffer_allocator.get_buffer(buffer_handle));
			}
		}
	}
}

void c_executor::add_output_buffers_to_accumulation_buffers(const s_executor_chunk_context &chunk_context) {
	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	// Add to the accumulation buffers, which should already exist from the first voice
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		uint32 accumulation_buffer_handle = m_voice_output_accumulation_buffers[output];
		wl_assert(accumulation_buffer_handle != k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() == k_task_data_type_real_in);
		if (outputs[output].is_constant()) {
			s_buffer_operation_addition::inout_in(
				chunk_context.frames,
				m_buffer_allocator.get_buffer(accumulation_buffer_handle),
				c_real_buffer_or_constant_in(outputs[output].get_real_constant_in()));
		} else {
			uint32 output_buffer_handle = outputs[output].get_real_buffer_in();
			s_buffer_operation_addition::inout_in(
				chunk_context.frames,
				m_buffer_allocator.get_buffer(accumulation_buffer_handle),
				c_real_buffer_or_constant_in(m_buffer_allocator.get_buffer(output_buffer_handle)));
		}
	}

	// Free the output buffers
	for (size_t index = 0; index < outputs.get_count(); index++) {
		if (!outputs[index].is_constant()) {
			uint32 buffer_index = outputs[index].get_real_buffer_in();
			decrement_buffer_usage(buffer_index);
		}
	}
}

#if PREDEFINED(ASSERTS_ENABLED)
void c_executor::assert_all_output_buffers_free() {
	// All buffers should have been freed
	for (size_t buffer = 0; buffer < m_buffer_contexts.get_array().get_count(); buffer++) {
		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		// We can't assert this because we don't clear handles for output buffers due to the duplication case:
		//wl_assert(buffer_context.handle == k_lock_free_invalid_handle);
		wl_assert(buffer_context.usages_remaining.get_unsafe() == 0);
	}
}
#endif // PREDEFINED(ASSERTS_ENABLED)

void c_executor::allocate_and_clear_output_buffers(const s_executor_chunk_context &chunk_context) {
	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	// Allocate zero'd buffers
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		wl_assert(m_voice_output_accumulation_buffers[output] == k_lock_free_invalid_handle);
		uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
		m_voice_output_accumulation_buffers[output] = buffer_handle;
		s_buffer_operation_assignment::in_out(
			chunk_context.frames,
			c_real_buffer_or_constant_in(0.0f),
			m_buffer_allocator.get_buffer(buffer_handle));
	}
}

void c_executor::mix_accumulation_buffers_to_channel_buffers(const s_executor_chunk_context &chunk_context) {
	// Allocate channel buffers
	for (uint32 channel = 0; channel < chunk_context.output_channels; channel++) {
		wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
		m_channel_mix_buffers[channel] = m_buffer_allocator.allocate_buffer();
	}

	c_wrapped_array_const<uint32> accumulation_buffers(
		&m_voice_output_accumulation_buffers.front(), m_voice_output_accumulation_buffers.size());
	c_wrapped_array_const<uint32> channel_buffers(&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());

	mix_output_buffers_to_channel_buffers(chunk_context.frames, m_buffer_allocator,
		accumulation_buffers, channel_buffers);

	// Free the accumulation buffers
	for (uint32 accum = 0; accum < m_voice_output_accumulation_buffers.size(); accum++) {
		m_buffer_allocator.free_buffer(m_voice_output_accumulation_buffers[accum]);
	}
}

void c_executor::add_task(uint32 voice_index, uint32 task_index, uint32 sample_rate, uint32 frames) {
	wl_assert(m_task_contexts.get_array()[task_index].predecessors_remaining.get() == 0);

	s_thread_pool_task task;
	task.task_entry_point = process_task_wrapper;
	s_task_parameters *task_params = task.parameter_block.get_memory_typed<s_task_parameters>();
	task_params->this_ptr = this;
	task_params->voice_index = voice_index;
	task_params->task_index = task_index;
	task_params->sample_rate = sample_rate;
	task_params->frames = frames;

	m_thread_pool.add_task(task);
}

void c_executor::process_task_wrapper(uint32 thread_index, const s_thread_parameter_block *params) {
	const s_task_parameters *task_params = params->get_memory_typed<s_task_parameters>();
	task_params->this_ptr->process_task(thread_index, task_params);
}

void c_executor::process_task(uint32 thread_index, const s_task_parameters *params) {
	if (m_profiling_enabled) {
		m_profiler.begin_task(thread_index, params->task_index,
			m_task_graph->get_task_function_index(params->task_index));
	}

	const s_task_function &task_function =
		c_task_function_registry::get_task_function(m_task_graph->get_task_function_index(params->task_index));

	allocate_output_buffers(params->task_index);

	{
		// Set up the arguments
		s_task_function_argument arguments[k_max_task_function_arguments];
		c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(params->task_index);

		for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
			s_task_function_argument &argument = arguments[arg];
			IF_ASSERTS_ENABLED(argument.data.type = argument_data[arg].data.type);
			argument.data.is_constant = argument_data[arg].is_constant();

			switch (argument_data[arg].data.type) {
			case k_task_data_type_real_in:
				if (argument.data.is_constant) {
					argument.data.value.real_constant_in = argument_data[arg].get_real_constant_in();
				} else {
					argument.data.value.real_buffer_in = m_buffer_allocator.get_buffer(
						m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_in()].handle);
				}
				break;

			case k_task_data_type_real_out:
				argument.data.value.real_buffer_out = m_buffer_allocator.get_buffer(
					m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_out()].handle);
				break;

			case k_task_data_type_real_inout:
				argument.data.value.real_buffer_inout = m_buffer_allocator.get_buffer(
					m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_inout()].handle);
				break;

			case k_task_data_type_real_array_in:
				argument.data.value.real_array_in = argument_data[arg].get_real_array_in();
				break;

			case k_task_data_type_bool_in:
				argument.data.value.bool_constant_in = argument_data[arg].get_bool_constant_in();
				break;

			case k_task_data_type_bool_array_in:
				argument.data.value.bool_array_in = argument_data[arg].get_bool_array_in();
				break;

			case k_task_data_type_string_in:
				argument.data.value.string_constant_in = argument_data[arg].get_string_constant_in();
				break;

			case k_task_data_type_string_array_in:
				argument.data.value.string_array_in = argument_data[arg].get_string_array_in();
				break;

			default:
				wl_unreachable();
			}
		}

		c_sample_library_accessor sample_library_accessor(m_sample_library);
		s_task_function_context task_function_context;
		task_function_context.sample_accessor = &sample_library_accessor;
		task_function_context.sample_requester = nullptr;
		task_function_context.sample_rate = params->sample_rate;
		task_function_context.buffer_size = params->frames;
		task_function_context.task_memory =
			m_voice_task_memory_pointers[m_task_graph->get_task_count() * params->voice_index + params->task_index];
		task_function_context.arguments = c_task_function_arguments(arguments, argument_data.get_count());
		// $TODO fill in timing info, etc.

		// Needed for array dereference
		task_function_context.executor = this;

		// Call the task function
		wl_assert(task_function.function);

		if (m_profiling_enabled) {
			m_profiler.begin_task_function(thread_index, params->task_index);
		}

		task_function.function(task_function_context);

		if (m_profiling_enabled) {
			m_profiler.end_task_function(thread_index, params->task_index);
		}
	}

	decrement_buffer_usages(params->task_index);

	// Decrement remaining predecessor counts for all successors to this task
	c_task_graph_task_array successors = m_task_graph->get_task_successors(params->task_index);
	for (size_t successor = 0; successor < successors.get_count(); successor++) {
		uint32 successor_index = successors[successor];

		int32 prev_predecessors_remaining =
			m_task_contexts.get_array()[successor_index].predecessors_remaining.decrement();
		wl_assert(prev_predecessors_remaining > 0);
		if (prev_predecessors_remaining == 1) {
			add_task(params->voice_index, successor_index, params->sample_rate, params->frames);
		}
	}

	int32 prev_tasks_remaining = m_tasks_remaining.decrement();
	wl_assert(prev_tasks_remaining > 0);
	if (prev_tasks_remaining == 1) {
		// Notify the calling thread that we are done
		m_all_tasks_complete_signal.notify();
	}

	if (m_profiling_enabled) {
		m_profiler.end_task(thread_index, params->task_index);
	}
}

void c_executor::allocate_output_buffers(uint32 task_index) {
	c_task_graph_data_array arguments = m_task_graph->get_task_arguments(task_index);

	for (size_t index = 0; index < arguments.get_count(); index++) {
		const s_task_graph_data &argument = arguments[index];

		switch (argument.data.type) {
		case k_task_data_type_real_in:
			if (!argument.is_constant()) {
				// Any input buffers should already be allocated
				wl_assert(m_buffer_contexts.get_array()[argument.get_real_buffer_in()].handle !=
					k_lock_free_invalid_handle);
			}
			break;

		case k_task_data_type_real_out:
		{
			// Allocate output buffers
			uint32 buffer_index = argument.get_real_buffer_out();
			wl_assert(m_buffer_contexts.get_array()[buffer_index].handle == k_lock_free_invalid_handle);
			m_buffer_contexts.get_array()[buffer_index].handle = m_buffer_allocator.allocate_buffer();
			break;
		}

		case k_task_data_type_real_inout:
			// Any input buffers should already be allocated
			wl_assert(m_buffer_contexts.get_array()[argument.get_real_buffer_inout()].handle !=
				k_lock_free_invalid_handle);
			break;

		case k_task_data_type_real_array_in:
		case k_task_data_type_bool_in:
		case k_task_data_type_bool_array_in:
		case k_task_data_type_string_in:
		case k_task_data_type_string_array_in:
			// Nothing to do
			break;

		default:
			wl_unreachable();
		}
	}
}

void c_executor::decrement_buffer_usages(uint32 task_index) {
	c_task_graph_data_array arguments = m_task_graph->get_task_arguments(task_index);

	// Decrement usage of each buffer
	for (size_t index = 0; index < arguments.get_count(); index++) {
		const s_task_graph_data &argument = arguments[index];

		switch (argument.data.type) {
		case k_task_data_type_real_in:
			if (!argument.is_constant()) {
				decrement_buffer_usage(argument.get_real_buffer_in());
			}
			break;

		case k_task_data_type_real_out:
			decrement_buffer_usage(argument.get_real_buffer_out());
			break;

		case k_task_data_type_real_inout:
			decrement_buffer_usage(argument.get_real_buffer_inout());
			break;

		case k_task_data_type_real_array_in:
			if (!argument.is_constant())
			{
				c_real_array real_array = argument.get_real_array_in();
				for (size_t index = 0; index < real_array.get_count(); index++) {
					const s_real_array_element &element = real_array[index];
					if (!element.is_constant) {
						decrement_buffer_usage(element.buffer_index_value);
					}
				}
			}
			break;

		case k_task_data_type_bool_in:
		case k_task_data_type_bool_array_in:
		case k_task_data_type_string_in:
		case k_task_data_type_string_array_in:
			// Nothing to do
			break;

		default:
			wl_unreachable();
		}
	}
}


void c_executor::decrement_buffer_usage(uint32 buffer_index) {
	// Free the buffer if usage reaches 0
	s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	int32 prev_usage = buffer_context.usages_remaining.decrement();
	wl_assert(prev_usage > 0);
	if (prev_usage == 1) {
		m_buffer_allocator.free_buffer(buffer_context.handle);
		buffer_context.handle = k_lock_free_invalid_handle;
	}
}

const c_buffer *c_executor::get_buffer_by_index(uint32 buffer_index) const {
	const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	return m_buffer_allocator.get_buffer(buffer_context.handle);
}