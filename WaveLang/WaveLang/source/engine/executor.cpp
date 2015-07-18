#include "engine/executor.h"
#include "engine/task_graph.h"
#include "engine/task_functions.h"

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
		thread_pool_settings.max_tasks = m_task_graph->get_max_task_concurrency();
		thread_pool_settings.start_paused = true;
		m_thread_pool.start(thread_pool_settings);
	}

	{ // Initialize buffer allocator
		s_buffer_allocator_settings buffer_allocator_settings;
		buffer_allocator_settings.buffer_type = k_buffer_type_real;
		buffer_allocator_settings.buffer_size = settings.max_buffer_size;
		buffer_allocator_settings.buffer_count = m_task_graph->get_max_buffer_concurrency(); // $TODO reserve N buffers for multi-voice accumulation for N channels
		m_buffer_allocator.initialize(buffer_allocator_settings);
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
			size_t required_memory_for_task = 0; // $TODO query task for memory requirement!

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

	// Process each active voice
	bool any_voice_processed = false;
	for (size_t voice = 0; voice < m_voice_contexts.size(); voice++) {
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
		for (size_t initial_task = 0; initial_task < m_task_graph->get_initial_tasks_count(); initial_task++) {
			add_task(m_task_graph->get_initial_task(initial_task), chunk_context.frames);
		}

		// Start the threads processing
		m_thread_pool.resume();
		m_all_tasks_complete_signal.wait();
		m_thread_pool.pause();

		if (!any_voice_processed) {
			// $TODO straight copy into the output buffer

			any_voice_processed = true;
		} else {
			// $TODO add to the output buffer
		}

		// Free the output buffers
		for (size_t index = 0; index < m_task_graph->get_output_count(); index++) {
			if (m_task_graph->is_output_buffer(index)) {
				uint32 buffer_index = m_task_graph->get_output_buffer(index);
				decrement_buffer_usage(buffer_index);
			}
		}

#if PREDEFINED(ASSERTS_ENABLED)
		// All buffers should have been freed
		for (size_t buffer = 0; buffer < m_buffer_contexts.get_array().get_count(); buffer++) {
			s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
			wl_assert(buffer_context.handle == k_lock_free_invalid_handle);
			wl_assert(buffer_context.usages_remaining.get_unsafe() == 0);
		}
#endif // PREDEFINED(ASSERTS_ENABLED)
	}

	if (!any_voice_processed) {
		// $TODO zero the output buffer (might not be zero, e.g. uint8 format)
	}

	// $TODO make sure to handle clamping and interleaving - accumulate first, convert to the desired format and
	// interleave at the very end.

	// $TODO temporary!
	memset(chunk_context.output_buffers, 0,
		get_sample_format_size(chunk_context.sample_format) * chunk_context.frames * chunk_context.output_channels);
}

void c_executor::add_task(uint32 task_index, uint32 frames) {
	wl_assert(m_task_contexts.get_array()[task_index].predecessors_remaining.get() == 0);

	s_thread_pool_task task;
	task.task_entry_point = process_task_wrapper;
	s_task_parameters *task_params = task.parameter_block.get_memory_typed<s_task_parameters>();
	task_params->this_ptr = this;
	task_params->task_index = task_index;
	task_params->frames = frames;

	m_thread_pool.add_task(task);
}

void c_executor::process_task_wrapper(const s_thread_parameter_block *params) {
	const s_task_parameters *task_params = params->get_memory_typed<s_task_parameters>();
	task_params->this_ptr->process_task(task_params->task_index, task_params->frames);
}

void c_executor::process_task(uint32 task_index, uint32 frames) {
	e_task_function task_function = m_task_graph->get_task_function(task_index);
	const s_task_function_description &task_function_description =
		c_task_function_registry::get_task_function_description(task_function);

	allocate_output_buffers(task_function_description, task_index);

	// $TODO process the task

	decrement_buffer_usages(task_function_description, task_index);

	// Decrement remaining predecessor counts for all successors to this task
	size_t successor_count = m_task_graph->get_task_successors_count(task_index);
	for (size_t successor = 0; successor < successor_count; successor++) {
		uint32 successor_index = m_task_graph->get_task_successor(task_index, successor);

		int32 prev_predecessors_remaining =
			m_task_contexts.get_array()[successor_index].predecessors_remaining.decrement();
		wl_assert(prev_predecessors_remaining > 0);
		if (prev_predecessors_remaining == 1) {
			add_task(successor_index, frames);
		}
	}

	int32 prev_tasks_remaining = m_tasks_remaining.decrement();
	wl_assert(prev_tasks_remaining > 0);
	if (prev_tasks_remaining == 1) {
		// Notify the calling thread that we are done
		m_all_tasks_complete_signal.notify();
	}
}

void c_executor::allocate_output_buffers(
	const s_task_function_description &task_function_description, uint32 task_index) {
	// Any input buffers should already be allocated
#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_in_buffer(task_index, index);
		wl_assert(m_buffer_contexts.get_array()[buffer_index].handle != k_lock_free_invalid_handle);
	}

	for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_inout_buffer(task_index, index);
		wl_assert(m_buffer_contexts.get_array()[buffer_index].handle != k_lock_free_invalid_handle);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Allocate output buffers
	for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_out_buffer(task_index, index);
		wl_assert(m_buffer_contexts.get_array()[buffer_index].handle == k_lock_free_invalid_handle);
		m_buffer_contexts.get_array()[buffer_index].handle = m_buffer_allocator.allocate_buffer();
	}
}

void c_executor::decrement_buffer_usages(
	const s_task_function_description &task_function_description, uint32 task_index) {
	// Decrement usage of each buffer
	for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_in_buffer(task_index, index);
		decrement_buffer_usage(buffer_index);
	}

	for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_out_buffer(task_index, index);
		decrement_buffer_usage(buffer_index);
	}

	for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
		uint32 buffer_index = m_task_graph->get_task_inout_buffer(task_index, index);
		decrement_buffer_usage(buffer_index);
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