#include "engine/array_dereference_interface/array_dereference_interface.h"
#include "engine/executor/channel_mixer.h"
#include "engine/executor/executor.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"

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
			chunk_context.sample_format, chunk_context.output_buffer);

		if (old_state == k_state_terminating) {
			// We successfully terminated, so signal the main thread
			m_shutdown_signal.notify();
		}
	}
}

void c_executor::initialize_internal(const s_executor_settings &settings) {
	m_settings = settings;
	initialize_events();
	initialize_thread_pool();
	initialize_buffer_manager();
	initialize_task_memory();
	initialize_tasks();
	initialize_voice_allocator();
	initialize_controller_event_manager();
	initialize_task_contexts();
	initialize_profiler();
}

void c_executor::initialize_events() {
	m_event_console.start();

	if (!m_async_event_handler.is_initialized()) {
		s_async_event_handler_settings event_handler_settings;
		event_handler_settings.set_default();
		event_handler_settings.event_handler = handle_event_wrapper;
		event_handler_settings.event_handler_context = this;
		m_async_event_handler.initialize(event_handler_settings);
	}
	m_async_event_handler.begin_event_handling();

	m_event_interface.initialize(&m_async_event_handler);
}

void c_executor::initialize_thread_pool() {
	s_thread_pool_settings thread_pool_settings;
	thread_pool_settings.thread_count = m_settings.thread_count;
	thread_pool_settings.max_tasks = std::max(1u, m_settings.task_graph->get_max_task_concurrency());
	thread_pool_settings.start_paused = true;
	m_thread_pool.start(thread_pool_settings);
}

void c_executor::initialize_buffer_manager() {
	m_buffer_manager.initialize(m_settings.task_graph, m_settings.max_buffer_size, m_settings.output_channels);
}

void c_executor::initialize_task_memory() {
	uint32 max_voices = m_settings.task_graph->get_globals().max_voices;
	size_t total_voice_task_memory_pointers = max_voices * m_settings.task_graph->get_task_count();
	m_voice_task_memory_pointers.clear();
	m_voice_task_memory_pointers.reserve(total_voice_task_memory_pointers);

	// Since we initially store offsets, this value cast to a pointer represents null
	static const size_t k_invalid_memory_pointer = static_cast<size_t>(-1);

	size_t required_memory_per_voice = 0;
	for (uint32 task = 0; task < m_settings.task_graph->get_task_count(); task++) {
		const s_task_function &task_function =
			c_task_function_registry::get_task_function(m_settings.task_graph->get_task_function_index(task));

		size_t required_memory_for_task = 0;
		if (task_function.memory_query) {
			// The memory query function will be able to read from the constant inputs.
			// We don't (currently) support dynamic task memory usage.

			// Set up the arguments
			s_static_array<s_task_function_argument, k_max_task_function_arguments> arguments;
			size_t argument_count = setup_task_arguments(task, false, arguments);

			s_task_function_context task_function_context;
			ZERO_STRUCT(&task_function_context);
			task_function_context.event_interface = &m_event_interface;
			task_function_context.sample_rate = m_settings.sample_rate;
			task_function_context.arguments = c_task_function_arguments(arguments.get_elements(), argument_count);
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
		m_voice_task_memory_pointers.resize(total_voice_task_memory_pointers);
		for (uint32 voice = 1; voice < max_voices; voice++) {
			size_t voice_offset = voice * m_settings.task_graph->get_task_count();
			size_t voice_memory_offset = voice * required_memory_per_voice;
			for (size_t index = 0; index < m_settings.task_graph->get_task_count(); index++) {
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
#if IS_TRUE(ASSERTS_ENABLED)
		// All pointers should be invalid
		for (size_t index = 0; index < m_voice_task_memory_pointers.size(); index++) {
			wl_assert(m_voice_task_memory_pointers[index] == reinterpret_cast<void *>(k_invalid_memory_pointer));
		}
#endif // IS_TRUE(ASSERTS_ENABLED)

		// All point to null
		m_voice_task_memory_pointers.clear();
		m_voice_task_memory_pointers.resize(total_voice_task_memory_pointers, nullptr);
	}
}

void c_executor::initialize_tasks() {
	m_sample_library.clear_requested_samples();

	for (uint32 task = 0; task < m_settings.task_graph->get_task_count(); task++) {
		const s_task_function &task_function =
			c_task_function_registry::get_task_function(m_settings.task_graph->get_task_function_index(task));

		if (task_function.initializer) {
			// Set up the arguments
			s_static_array<s_task_function_argument, k_max_task_function_arguments> arguments;
			size_t argument_count = setup_task_arguments(task, false, arguments);

			c_sample_library_requester sample_library_requester(m_sample_library);
			s_task_function_context task_function_context;
			ZERO_STRUCT(&task_function_context);
			task_function_context.event_interface = &m_event_interface;
			task_function_context.sample_requester = &sample_library_requester;
			task_function_context.sample_rate = m_settings.sample_rate;
			task_function_context.arguments = c_task_function_arguments(arguments.get_elements(), argument_count);
			// $TODO fill in timing info, etc.

			for (uint32 voice = 0; voice < m_settings.task_graph->get_globals().max_voices; voice++) {
				task_function_context.task_memory =
					m_voice_task_memory_pointers[m_settings.task_graph->get_task_count() * voice + task];
				task_function.initializer(task_function_context);
			}
		}
	}

	m_sample_library.update_loaded_samples();
}

void c_executor::initialize_voice_allocator() {
	m_voice_allocator.initialize(m_settings.task_graph->get_globals().max_voices);
}

void c_executor::initialize_controller_event_manager() {
	m_controller_event_manager.initialize(m_settings.controller_event_queue_size);
}

void c_executor::initialize_task_contexts() {
	m_task_contexts.free();
	m_task_contexts.allocate(m_settings.task_graph->get_task_count());
}

void c_executor::initialize_profiler() {
	if (m_settings.profiling_enabled) {
		s_profiler_settings profiler_settings;
		profiler_settings.worker_thread_count = m_settings.thread_count;
		profiler_settings.task_count = m_settings.task_graph->get_task_count();
		m_profiler.initialize(profiler_settings);
		m_profiler.start();
	}
}

void c_executor::shutdown_internal() {
	IF_ASSERTS_ENABLED(uint32 unexecuted_tasks = ) m_thread_pool.stop();
	wl_assert(unexecuted_tasks == 0);

	m_async_event_handler.end_event_handling();
	m_event_console.stop();

	if (m_settings.profiling_enabled) {
		m_profiler.stop();
		s_profiler_report report;
		m_profiler.get_report(report);
		output_profiler_report("profiler_report.csv", report);
	}
}

void c_executor::execute_internal(const s_executor_chunk_context &chunk_context) {
	if (m_settings.profiling_enabled) {
		m_profiler.begin_execution();
	}

	wl_assert(chunk_context.frames <= m_settings.max_buffer_size); // $TODO we could do multiple passes to handle this

	// Clear accumulation buffers
	m_buffer_manager.begin_chunk(chunk_context.frames);

	size_t controller_event_count = m_settings.process_controller_events(
		m_settings.process_controller_events_context,
		m_controller_event_manager.get_writable_controller_events(),
		chunk_context.frames * k_nanoseconds_per_second / chunk_context.sample_rate);
	m_controller_event_manager.process_controller_events(controller_event_count);

	m_voice_allocator.allocate_voices_for_chunk(
		m_controller_event_manager.get_note_events(), chunk_context.sample_rate, chunk_context.frames);

	// Process each active voice
	for (uint32 voice_index = 0; voice_index < m_voice_allocator.get_voice_count(); voice_index++) {
		const c_voice_allocator::s_voice &voice = m_voice_allocator.get_voice(voice_index);

		if (!voice.active) {
			continue;
		}

		process_voice(chunk_context, voice_index);
		m_buffer_manager.accumulate_voice_output(voice.chunk_offset_samples);
	}

	m_buffer_manager.mix_voice_accumulation_buffers_to_output_buffer(
		chunk_context.sample_format, chunk_context.output_buffer);

	if (m_settings.profiling_enabled) {
		m_profiler.end_execution();
	}
}

void c_executor::process_voice(const s_executor_chunk_context &chunk_context, uint32 voice_index) {
	const c_voice_allocator::s_voice &voice = m_voice_allocator.get_voice(voice_index);
	wl_assert(voice.active);

	if (voice.activated_this_chunk) {
		for (uint32 task = 0; task < m_settings.task_graph->get_task_count(); task++) {
			const s_task_function &task_function =
				c_task_function_registry::get_task_function(m_settings.task_graph->get_task_function_index(task));

			if (task_function.voice_initializer) {
				// Set up the arguments
				s_static_array<s_task_function_argument, k_max_task_function_arguments> arguments;
				size_t argument_count = setup_task_arguments(task, false, arguments);

				c_sample_library_requester sample_library_requester(m_sample_library);
				s_task_function_context task_function_context;
				ZERO_STRUCT(&task_function_context);
				task_function_context.event_interface = &m_event_interface;
				task_function_context.sample_requester = &sample_library_requester;
				task_function_context.sample_rate = m_settings.sample_rate;
				task_function_context.task_memory =
					m_voice_task_memory_pointers[m_settings.task_graph->get_task_count() * voice_index + task];
				task_function_context.arguments = c_task_function_arguments(arguments.get_elements(), argument_count);
				// $TODO fill in timing info, etc.

				task_function.voice_initializer(task_function_context);
			}
		}
	}

	// Setup each initial task predecessor count
	for (uint32 task = 0; task < m_settings.task_graph->get_task_count(); task++) {
		m_task_contexts.get_array()[task].predecessors_remaining.initialize(
			cast_integer_verify<int32>(m_settings.task_graph->get_task_predecessor_count(task)));
	}

	m_buffer_manager.initialize_buffers_for_voice();

	m_tasks_remaining.initialize(cast_integer_verify<int32>(m_settings.task_graph->get_task_count()));

	wl_assert(chunk_context.frames > voice.chunk_offset_samples);
	uint32 frame_count = chunk_context.frames - voice.chunk_offset_samples;

	m_voice_interface = c_voice_interface(
		voice.note_id, voice.note_velocity, voice.note_release_sample - voice.chunk_offset_samples);

	// Add the initial tasks
	c_task_graph_task_array initial_tasks = m_settings.task_graph->get_initial_tasks();
	if (initial_tasks.get_count() > 0) {
		for (size_t initial_task = 0; initial_task < initial_tasks.get_count(); initial_task++) {
			add_task(voice_index, initial_tasks[initial_task], chunk_context.sample_rate, frame_count);
		}

		if (m_settings.profiling_enabled) {
			m_profiler.begin_execution_tasks();
		}

		// Start the threads processing
		m_thread_pool.resume();
		m_all_tasks_complete_signal.wait();
		m_thread_pool.pause();

		if (m_settings.profiling_enabled) {
			m_profiler.end_execution_tasks();
		}
	}

	//if (flag_for_disable_voice_is_true) { $TODO
	//	m_voice_allocator.disable_voice(voice_index);
	//}
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
	if (m_settings.profiling_enabled) {
		m_profiler.begin_task(thread_index, params->task_index,
			m_settings.task_graph->get_task_function_index(params->task_index));
	}

	const s_task_function &task_function =
		c_task_function_registry::get_task_function(m_settings.task_graph->get_task_function_index(params->task_index));

	allocate_output_buffers(params->task_index);

	{
		// Set up the arguments
		s_static_array<s_task_function_argument, k_max_task_function_arguments> arguments;
		size_t argument_count = setup_task_arguments(params->task_index, true, arguments);

		c_array_dereference_interface array_dereference_interface(this);
		c_sample_library_accessor sample_library_accessor(m_sample_library);
		s_task_function_context task_function_context;
		task_function_context.array_dereference_interface = &array_dereference_interface;
		task_function_context.event_interface = &m_event_interface;
		task_function_context.sample_accessor = &sample_library_accessor;
		task_function_context.sample_requester = nullptr;
		task_function_context.voice_interface = &m_voice_interface;
		task_function_context.sample_rate = params->sample_rate;
		task_function_context.buffer_size = params->frames;
		task_function_context.task_memory = m_voice_task_memory_pointers[
			m_settings.task_graph->get_task_count() * params->voice_index + params->task_index];
		task_function_context.arguments = c_task_function_arguments(arguments.get_elements(), argument_count);
		// $TODO fill in timing info, etc.

		// Call the task function
		wl_assert(task_function.function);

		if (m_settings.profiling_enabled) {
			m_profiler.begin_task_function(thread_index, params->task_index);
		}

		task_function.function(task_function_context);

		if (m_settings.profiling_enabled) {
			m_profiler.end_task_function(thread_index, params->task_index);
		}
	}

	decrement_buffer_usages(params->task_index);

	// Decrement remaining predecessor counts for all successors to this task
	c_task_graph_task_array successors = m_settings.task_graph->get_task_successors(params->task_index);
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

	if (m_settings.profiling_enabled) {
		m_profiler.end_task(thread_index, params->task_index);
	}
}

size_t c_executor::setup_task_arguments(uint32 task_index, bool include_dynamic_arguments,
	s_static_array<s_task_function_argument, k_max_task_function_arguments> &out_arguments) {
	// Obtain argument data from the graph
	c_task_graph_data_array argument_data = m_settings.task_graph->get_task_arguments(task_index);

	for (size_t arg = 0; arg < argument_data.get_count(); arg++) {
		s_task_function_argument &argument = out_arguments[arg];
		IF_ASSERTS_ENABLED(argument.data.type = argument_data[arg].data.type);
		argument.data.is_constant = argument_data[arg].is_constant();

		c_task_data_type type = argument_data[arg].data.type;
		if (type.is_array()) {
			wl_assert(type.get_qualifier() == k_task_qualifier_in);
			switch (type.get_primitive_type()) {
			case k_task_primitive_type_real:
				argument.data.value.real_array_in = argument_data[arg].get_real_array_in();
				break;

			case k_task_primitive_type_bool:
				argument.data.value.bool_array_in = argument_data[arg].get_bool_array_in();
				break;

			case k_task_primitive_type_string:
				argument.data.value.string_array_in = argument_data[arg].get_string_array_in();
				break;

			default:
				wl_unreachable();
			}
		} else if (argument.data.is_constant) {
			wl_assert(type.get_qualifier() == k_task_qualifier_in);
			switch (type.get_primitive_type()) {
			case k_task_primitive_type_real:
				argument.data.value.real_constant_in = argument_data[arg].get_real_constant_in();
				break;

			case k_task_primitive_type_bool:
				argument.data.value.bool_constant_in = argument_data[arg].get_bool_constant_in();
				break;

			case k_task_primitive_type_string:
				argument.data.value.string_constant_in = argument_data[arg].get_string_constant_in();
				break;

			default:
				wl_unreachable();
			}
		} else {
			argument.data.value.buffer = include_dynamic_arguments ?
				m_buffer_manager.get_buffer(argument_data[arg].data.value.buffer) : nullptr;
		}
	}

	// Return number of arguments for convenience
	return argument_data.get_count();
}


void c_executor::allocate_output_buffers(uint32 task_index) {
	c_task_graph_data_array arguments = m_settings.task_graph->get_task_arguments(task_index);

	for (size_t index = 0; index < arguments.get_count(); index++) {
		const s_task_graph_data &argument = arguments[index];

		if (!argument.data.type.is_array() && !argument.data.is_constant) {
			e_task_qualifier qualifier = argument.data.type.get_qualifier();
			if (qualifier == k_task_qualifier_out) {
				// Allocate output buffers
				m_buffer_manager.allocate_buffer(argument.data.value.buffer);
			} else {
				wl_assert(qualifier == k_task_qualifier_in || qualifier == k_task_qualifier_inout);
				// Any input buffers should already be allocated (this includes inout buffers)
				wl_assert(m_buffer_manager.is_buffer_allocated(argument.data.value.buffer));
			}
		}
	}
}

void c_executor::decrement_buffer_usages(uint32 task_index) {
	c_task_graph_data_array arguments = m_settings.task_graph->get_task_arguments(task_index);

	// Decrement usage of each buffer
	for (size_t index = 0; index < arguments.get_count(); index++) {
		const s_task_graph_data &argument = arguments[index];

		if (argument.data.is_constant) {
			// Nothing to do for constants and constant arrays since there are no buffers to decrement
			continue;
		}

		if (argument.get_type().is_array()) {
			switch (argument.get_type().get_primitive_type()) {
			case k_task_primitive_type_real:
			{
				c_real_array real_array = argument.get_real_array_in();
				for (size_t index = 0; index < real_array.get_count(); index++) {
					const s_real_array_element &element = real_array[index];
					if (!element.is_constant) {
						m_buffer_manager.decrement_buffer_usage(element.buffer_index_value);
					}
				}
				break;
			}

			case k_task_primitive_type_bool:
			{
				c_bool_array bool_array = argument.get_bool_array_in();
				for (size_t index = 0; index < bool_array.get_count(); index++) {
					const s_bool_array_element &element = bool_array[index];
					if (!element.is_constant) {
						m_buffer_manager.decrement_buffer_usage(element.buffer_index_value);
					}
				}
				break;
			}

			case k_task_primitive_type_string:
				wl_unreachable(); // Non-constant string arrays not supported
				break;

			default:
				wl_unreachable();
			}
		} else {
			m_buffer_manager.decrement_buffer_usage(argument.data.value.buffer);
		}
	}
}

const c_buffer *c_executor::get_buffer_by_index(uint32 buffer_index) const {
	return m_buffer_manager.get_buffer(buffer_index);
}

void c_executor::handle_event_wrapper(void *context, size_t event_size, const void *event_data) {
	static_cast<c_executor *>(context)->handle_event(event_size, event_data);
}

void c_executor::handle_event(size_t event_size, const void *event_data) {
	if (m_event_console.is_running()) {
		c_event_string event_string;
		e_event_level event_level = c_event_interface::build_event_string(event_size, event_data, event_string);

		event_string.truncate_to_length(event_string.get_max_length() - 1);
		event_string.append_verify("\n");

		e_console_color color = k_console_color_white;
		switch (event_level) {
		case k_event_level_verbose:
			color = k_console_color_gray;
			break;

		case k_event_level_message:
			color = k_console_color_white;
			break;

		case k_event_level_warning:
			color = k_console_color_yellow;
			break;

		case k_event_level_error:
			color = k_console_color_red;
			break;

		case k_event_level_critical:
			color = k_console_color_pink;
			break;

		default:
			wl_unreachable();
		}

		m_event_console.print_to_console(event_string.get_string(), color);
	}
}
