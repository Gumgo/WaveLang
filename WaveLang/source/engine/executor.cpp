#include "engine/executor.h"
#include "engine/task_graph.h"
#include "engine/task_functions.h"
#include "engine/buffer_operations/buffer_operations_arithmetic.h"
#include "engine/channel_mixer.h"
#include <algorithm>

c_executor::c_executor() {
	m_state.initialize(k_state_uninitialized);
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

	if (old_state == k_state_terminating) {
		// We successfully terminated, so signal the main thread
		m_shutdown_signal.notify();
	} else if (old_state == k_state_running) {
		execute_internal(chunk_context);
	}
}

void c_executor::initialize_internal(const s_executor_settings &settings) {
	m_task_graph = settings.task_graph;
	m_max_buffer_size = settings.max_buffer_size;

	{ // Initialize thread pool
		s_thread_pool_settings thread_pool_settings;
		thread_pool_settings.thread_count = 1; // $TODO don't hardcode this
		thread_pool_settings.max_tasks = std::max(1u, m_task_graph->get_max_task_concurrency());
		thread_pool_settings.start_paused = true;
		m_thread_pool.start(thread_pool_settings);
	}

	{ // Initialize buffer allocator
		s_buffer_allocator_settings buffer_allocator_settings;
		buffer_allocator_settings.buffer_type = k_buffer_type_real;
		buffer_allocator_settings.buffer_size = settings.max_buffer_size;
		buffer_allocator_settings.buffer_count = m_task_graph->get_max_buffer_concurrency();

		c_task_graph_data_array outputs = m_task_graph->get_outputs();

		// Allocate an additional buffer for each constant output. The task graph doesn't require buffers for these, but
		// at this point we do.
		for (uint32 output = 0; output < outputs.get_count(); output++) {
			if (outputs[output].type == k_task_data_type_real_constant_in) {
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

				if (outputs[output].type == k_task_data_type_real_buffer_in) {
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

	{ // Calculate task memory
		uint32 max_voices = m_task_graph->get_globals().max_voices;
		size_t total_voice_task_memory_pointers = max_voices * m_task_graph->get_task_count();
		m_voice_task_memory_pointers.clear();
		m_voice_task_memory_pointers.reserve(total_voice_task_memory_pointers);

		// Since we initially store offsets, this value cast to a pointer represents null
		static const size_t k_invalid_memory_pointer = static_cast<size_t>(-1);

		size_t required_memory_per_voice = 0;
		for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
			const s_task_function_description &task_function_description =
				c_task_function_registry::get_task_function_description(m_task_graph->get_task_function(task));

			size_t required_memory_for_task = 0;
			if (task_function_description.memory_query) {
				// The memory query function will be able to read from the constant inputs.
				// We don't (currently) support dynamic task memory usage.

				// Set up the arguments
				s_task_function_argument arguments[k_max_task_function_arguments];
				c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(task);

				for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
					s_task_function_argument &argument = arguments[arg];
					argument.type = argument_data[arg].type;

					switch (argument.type) {
					case k_task_data_type_real_buffer_in:
					case k_task_data_type_real_buffer_out:
					case k_task_data_type_real_buffer_inout:
						break;

					case k_task_data_type_real_constant_in:
						argument.data.real_constant_in = argument_data[arg].get_real_constant_in();
						break;

					case k_task_data_type_bool_constant_in:
						argument.data.bool_constant_in = argument_data[arg].get_bool_constant_in();
						break;

					case k_task_data_type_string_constant_in:
						argument.data.string_constant_in = argument_data[arg].get_string_constant_in();
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

				required_memory_for_task = task_function_description.memory_query(task_function_context);
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

	{ // Initialize task memory and load samples
		m_sample_library.clear_requested_samples();

		for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
			const s_task_function_description &task_function_description =
				c_task_function_registry::get_task_function_description(m_task_graph->get_task_function(task));

			if (task_function_description.initializer) {
				// Set up the arguments
				s_task_function_argument arguments[k_max_task_function_arguments];
				c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(task);

				for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
					s_task_function_argument &argument = arguments[arg];
					argument.type = argument_data[arg].type;

					switch (argument.type) {
					case k_task_data_type_real_buffer_in:
					case k_task_data_type_real_buffer_out:
					case k_task_data_type_real_buffer_inout:
						break;

					case k_task_data_type_real_constant_in:
						argument.data.real_constant_in = argument_data[arg].get_real_constant_in();
						break;

					case k_task_data_type_bool_constant_in:
						argument.data.bool_constant_in = argument_data[arg].get_bool_constant_in();
						break;

					case k_task_data_type_string_constant_in:
						argument.data.string_constant_in = argument_data[arg].get_string_constant_in();
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
					task_function_description.initializer(task_function_context);
				}
			}
		}

		m_sample_library.update_loaded_samples();
	}

	{ // Setup voice contexts
		m_voice_contexts.resize(m_task_graph->get_globals().max_voices);
		for (size_t voice = 0; voice < m_voice_contexts.size(); voice++) {
			s_voice_context &context = m_voice_contexts[voice];
			context.active = true; // $TODO should be false by default when we have note press events
		}
	}

	{ // Allocate space for a context for each task
		m_task_contexts.free();
		m_task_contexts.allocate(m_task_graph->get_task_count());
	}

	{ // Allocate space for buffer assignments
		m_buffer_contexts.free();
		m_buffer_contexts.allocate(m_task_graph->get_buffer_count());
	}
}

void c_executor::shutdown_internal() {
	IF_ASSERTS_ENABLED(uint32 unexecuted_tasks = ) m_thread_pool.stop();
	wl_assert(unexecuted_tasks == 0);
}

void c_executor::execute_internal(const s_executor_chunk_context &chunk_context) {
	wl_assert(chunk_context.frames <= m_max_buffer_size); // $TODO we could do multiple passes to handle this...

	// Clear accumulation buffers
	std::fill(m_voice_output_accumulation_buffers.begin(), m_voice_output_accumulation_buffers.end(),
		k_lock_free_invalid_handle);
	std::fill(m_channel_mix_buffers.begin(), m_channel_mix_buffers.end(),
		k_lock_free_invalid_handle);

	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	// Process each active voice
	uint32 voices_processed = 0;
	for (uint32 voice = 0; voice < m_voice_contexts.size(); voice++) {
		if (!m_voice_contexts[voice].active) {
			continue;
		}

		// Setup each initial task predecessor count
		for (uint32 task = 0; task < m_task_graph->get_task_count(); task++) {
			m_task_contexts.get_array()[task].predecessors_remaining.initialize(
				static_cast<int32>(m_task_graph->get_task_predecessor_count(task)));
		}

		// Initially all buffers are unassigned
		for (uint32 buffer = 0; buffer < m_task_graph->get_buffer_count(); buffer++) {
			s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
			buffer_context.handle = k_lock_free_invalid_handle;
			buffer_context.usages_remaining.initialize(m_task_graph->get_buffer_usages(buffer)); 
		}

		m_tasks_remaining.initialize(static_cast<int32>(m_task_graph->get_task_count()));

		// Add the initial tasks
		c_task_graph_task_array initial_tasks = m_task_graph->get_initial_tasks();
		if (initial_tasks.get_count() > 0) {
			for (size_t initial_task = 0; initial_task < initial_tasks.get_count(); initial_task++) {
				add_task(voice, initial_tasks[initial_task], chunk_context.sample_rate, chunk_context.frames);
			}

			// Start the threads processing
			m_thread_pool.resume();
			m_all_tasks_complete_signal.wait();
			m_thread_pool.pause();
		}

		if (voices_processed == 0) {
			// Swap the output buffers into the accumulation buffers
			for (uint32 output = 0; output < outputs.get_count(); output++) {
				wl_assert(m_voice_output_accumulation_buffers[output] == k_lock_free_invalid_handle);
				if (outputs[output].type == k_task_data_type_real_buffer_in) {
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
						s_buffer_operation_assignment::buffer(
							chunk_context.frames,
							m_buffer_allocator.get_buffer(buffer_handle),
							m_buffer_allocator.get_buffer(buffer_context.handle));
					}
				} else {
					wl_assert(outputs[output].type == k_task_data_type_real_constant_in);
					uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
					m_voice_output_accumulation_buffers[output] = buffer_handle;
					s_buffer_operation_assignment::constant(
						chunk_context.frames,
						m_buffer_allocator.get_buffer(buffer_handle),
						outputs[output].get_real_constant_in());
				}
			}
		} else {
			// Add to the accumulation buffers, which should already exist from the first voice
			for (uint32 output = 0; output < outputs.get_count(); output++) {
				uint32 accumulation_buffer_handle = m_voice_output_accumulation_buffers[output];
				wl_assert(accumulation_buffer_handle != k_lock_free_invalid_handle);
				if (outputs[output].type == k_task_data_type_real_buffer_in) {
					uint32 output_buffer_handle = outputs[output].get_real_buffer_in();
					s_buffer_operation_addition::bufferio_buffer(
						chunk_context.frames,
						m_buffer_allocator.get_buffer(accumulation_buffer_handle),
						m_buffer_allocator.get_buffer(output_buffer_handle));
				} else {
					wl_assert(outputs[output].type == k_task_data_type_real_constant_in);
					s_buffer_operation_assignment::constant(
						chunk_context.frames,
						m_buffer_allocator.get_buffer(accumulation_buffer_handle),
						outputs[output].get_real_constant_in());
				}
			}

			// Free the output buffers
			for (size_t index = 0; index < outputs.get_count(); index++) {
				if (outputs[index].type == k_task_data_type_real_buffer_in) {
					uint32 buffer_index = outputs[index].get_real_buffer_in();
					decrement_buffer_usage(buffer_index);
				}
			}
		}

#if PREDEFINED(ASSERTS_ENABLED)
		// All buffers should have been freed
		for (size_t buffer = 0; buffer < m_buffer_contexts.get_array().get_count(); buffer++) {
			s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
			// We can't assert this because we don't clear handles for output buffers due to the duplication case:
			//wl_assert(buffer_context.handle == k_lock_free_invalid_handle);
			wl_assert(buffer_context.usages_remaining.get_unsafe() == 0);
		}
#endif // PREDEFINED(ASSERTS_ENABLED)

		voices_processed++;
	}

	if (voices_processed == 0) {
		// Allocate zero'd buffers
		for (uint32 output = 0; output < outputs.get_count(); output++) {
			wl_assert(m_voice_output_accumulation_buffers[output] == k_lock_free_invalid_handle);
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
			m_voice_output_accumulation_buffers[output] = buffer_handle;
			s_buffer_operation_assignment::constant(
				chunk_context.frames,
				m_buffer_allocator.get_buffer(buffer_handle),
				0.0f);
		}
	}

	if (outputs.get_count() != chunk_context.output_channels) {
		// Allocate channel buffers
		for (uint32 channel = 0; channel < chunk_context.output_channels; channel++) {
			wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
			m_channel_mix_buffers[channel] = m_buffer_allocator.allocate_buffer();
		}

		c_wrapped_array_const<uint32> accumulation_buffers(
			&m_voice_output_accumulation_buffers.front(), m_voice_output_accumulation_buffers.size());
		c_wrapped_array_const<uint32> channel_buffers(
			&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());
		mix_output_buffers_to_channel_buffers(chunk_context.frames, m_buffer_allocator,
			accumulation_buffers, channel_buffers);

		// Free the accumulation buffers
		for (uint32 output = 0; output < outputs.get_count(); output++) {
			m_buffer_allocator.free_buffer(m_voice_output_accumulation_buffers[output]);
		}
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

void c_executor::process_task_wrapper(const s_thread_parameter_block *params) {
	const s_task_parameters *task_params = params->get_memory_typed<s_task_parameters>();
	task_params->this_ptr->process_task(task_params);
}

void c_executor::process_task(const s_task_parameters *params) {
	e_task_function task_function = m_task_graph->get_task_function(params->task_index);
	const s_task_function_description &task_function_description =
		c_task_function_registry::get_task_function_description(task_function);

	allocate_output_buffers(params->task_index);

	{
		// Set up the arguments
		s_task_function_argument arguments[k_max_task_function_arguments];
		c_task_graph_data_array argument_data = m_task_graph->get_task_arguments(params->task_index);

		for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
			s_task_function_argument &argument = arguments[arg];
			argument.type = argument_data[arg].type;

			switch (argument.type) {
			case k_task_data_type_real_buffer_in:
				argument.data.real_buffer_in = m_buffer_allocator.get_buffer(
					m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_in()].handle);
				break;

			case k_task_data_type_real_buffer_out:
				argument.data.real_buffer_out = m_buffer_allocator.get_buffer(
					m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_out()].handle);
				break;

			case k_task_data_type_real_buffer_inout:
				argument.data.real_buffer_inout = m_buffer_allocator.get_buffer(
					m_buffer_contexts.get_array()[argument_data[arg].get_real_buffer_inout()].handle);
				break;

			case k_task_data_type_real_constant_in:
				argument.data.real_constant_in = argument_data[arg].get_real_constant_in();
				break;

			case k_task_data_type_bool_constant_in:
				argument.data.bool_constant_in = argument_data[arg].get_bool_constant_in();
				break;

			case k_task_data_type_string_constant_in:
				argument.data.string_constant_in = argument_data[arg].get_string_constant_in();
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

		// Call the task function
		wl_assert(task_function_description.function);
		task_function_description.function(task_function_context);
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
}

void c_executor::allocate_output_buffers(uint32 task_index) {
	c_task_graph_data_array arguments = m_task_graph->get_task_arguments(task_index);

	for (size_t index = 0; index < arguments.get_count(); index++) {
		const s_task_graph_data &argument = arguments[index];

		switch (argument.type) {
		case k_task_data_type_real_buffer_in:
			// Any input buffers should already be allocated
			wl_assert(m_buffer_contexts.get_array()[argument.get_real_buffer_in()].handle !=
				k_lock_free_invalid_handle);
			break;

		case k_task_data_type_real_buffer_out:
		{
			// Allocate output buffers
			uint32 buffer_index = argument.get_real_buffer_out();
			wl_assert(m_buffer_contexts.get_array()[buffer_index].handle == k_lock_free_invalid_handle);
			m_buffer_contexts.get_array()[buffer_index].handle = m_buffer_allocator.allocate_buffer();
			break;
		}

		case k_task_data_type_real_buffer_inout:
			// Any input buffers should already be allocated
			wl_assert(m_buffer_contexts.get_array()[argument.get_real_buffer_inout()].handle !=
				k_lock_free_invalid_handle);
			break;

		case k_task_data_type_real_constant_in:
		case k_task_data_type_bool_constant_in:
		case k_task_data_type_string_constant_in:
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

		switch (argument.type) {
		case k_task_data_type_real_buffer_in:
			decrement_buffer_usage(argument.get_real_buffer_in());
			break;

		case k_task_data_type_real_buffer_out:
			decrement_buffer_usage(argument.get_real_buffer_out());
			break;

		case k_task_data_type_real_buffer_inout:
			decrement_buffer_usage(argument.get_real_buffer_inout());
			break;

		case k_task_data_type_real_constant_in:
		case k_task_data_type_bool_constant_in:
		case k_task_data_type_string_constant_in:
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