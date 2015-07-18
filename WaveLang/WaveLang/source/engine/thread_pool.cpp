#include "engine/thread_pool.h"
#include <string>

c_thread_pool::c_thread_pool() {
}

c_thread_pool::~c_thread_pool() {
}

void c_thread_pool::start(const s_thread_pool_settings &settings) {
	wl_assert(m_threads.size() == 0);
	wl_assert(settings.thread_count > 0);
	wl_assert(settings.max_tasks > 0);

	// Set up the task queue
	m_pending_tasks_element_memory.allocate(settings.max_tasks + 1);
	m_pending_tasks_queue_memory.allocate(settings.max_tasks + 1);
	m_pending_tasks_free_list_memory.allocate(settings.max_tasks + 1);
	m_pending_tasks.initialize(
		m_pending_tasks_element_memory.get_array(),
		m_pending_tasks_queue_memory.get_array(),
		m_pending_tasks_free_list_memory.get_array());

	m_check_paused.initialize(settings.start_paused);
	m_paused = settings.start_paused;

	// Setup the threads
	m_threads.resize(settings.thread_count);
	for (size_t thread = 0; thread < m_threads.size(); thread++) {
		s_thread_definition thread_definition;
		std::string thread_name = "worker_" + std::to_string(thread);
		thread_definition.thread_name = thread_name.c_str();
		thread_definition.stack_size = 0;
		thread_definition.thread_priority = k_thread_priority_normal;
		thread_definition.processor = -1;
		thread_definition.thread_entry_point = worker_thread_entry_point;
		
		ZERO_STRUCT(&thread_definition.parameter_block);
		s_worker_thread_context *params = thread_definition.parameter_block.get_memory_typed<s_worker_thread_context>();
		params->this_ptr = this;
		params->worker_thread_index = static_cast<uint32>(thread);

		m_threads[thread].start(thread_definition);
	}
}

uint32 c_thread_pool::stop() {
	wl_assert(m_threads.size() > 0);

	// Push a "terminate" task for each thread - a task with no function pointer.
	// Note: when a thread encounters this, it will terminate immediately! There could still be pending tasks left.
	s_task terminate_task;
	ZERO_STRUCT(&terminate_task);
	for (size_t thread = 0; thread < m_threads.size(); thread++) {
		m_pending_tasks.push(terminate_task);
	}

	if (m_check_paused.get_unsafe()) {
		resume();
	}

	// Now join with all threads to ensure they all terminate cleanly
	for (size_t thread = 0; thread < m_threads.size(); thread++) {
		m_threads[thread].join();
	}

	m_threads.clear();

	uint32 unexecuted_tasks = 0;
	s_task executed_task_placeholder;
	while (m_pending_tasks.pop(executed_task_placeholder)) {
		unexecuted_tasks++;
	}

	m_pending_tasks_element_memory.free();
	m_pending_tasks_queue_memory.free();
	m_pending_tasks_free_list_memory.free();

	return unexecuted_tasks;
}

void c_thread_pool::pause() {
	wl_assert(m_threads.size() > 0);

	IF_ASSERTS_ENABLED(int32 prev_value = ) m_check_paused.increment();
	wl_assert(prev_value == 0);

	{
		// Atomically set pause to true. We don't really need the lock here but we use it anyway for symmetry.
		c_scoped_lock lock(m_pause_mutex);
		m_paused = true;
	}
}

void c_thread_pool::resume() {
	wl_assert(m_threads.size() > 0);

	IF_ASSERTS_ENABLED(int32 prev_value = ) m_check_paused.decrement();
	wl_assert(prev_value == 1);

	{
		// Atomically set pause to false. We need to lock because we don't want to set paused to false and notify
		// threads to wake up after a thread has detected that we are paused, but before it has started waiting.
		c_scoped_lock lock(m_pause_mutex);
		m_paused = false;
	}

	// Signal all threads that we should unpause
	m_pause_condition_variable.notify_all();
}

bool c_thread_pool::add_task(const s_thread_pool_task &task) {
	wl_assert(!m_threads.empty());

	s_task pending_task;
	pending_task.task_function = task.task_entry_point;
	memcpy(&pending_task.params, &task.parameter_block, sizeof(pending_task.params));

	return m_pending_tasks.push(pending_task);
}

void c_thread_pool::worker_thread_entry_point(const s_thread_parameter_block *param_block) {
	const s_worker_thread_context &context = *param_block->get_memory_typed<s_worker_thread_context>();

	// Keep looping until we find a termination task
	while (true) {
		// Check if we should pause
		while (context.this_ptr->m_check_paused.get()) {
			// Check if we're paused in a thread-safe manner
			c_scoped_lock lock(context.this_ptr->m_pause_mutex);
			if (context.this_ptr->m_paused) {
				// Wait on the condition variable. If we spuriously wake up, we will loop and pause again.
				context.this_ptr->m_pause_condition_variable.wait(lock);
			}
		}

		// Attempt to pop a task from the queue
		s_task task;
		if (context.this_ptr->m_pending_tasks.pop(task)) {
			// If this task has no task function, it is an indication that we should terminate
			if (!task.task_function) {
				break;
			}

			task.task_function(&task.params);
		}
	}
}
