#pragma once

#include "common/common.h"
#include "common/threading/condition_variable.h"
#include "common/threading/lock_free_queue.h"
#include "common/threading/mutex.h"
#include "common/threading/thread.h"

#include <atomic>
#include <vector>

struct s_thread_pool_settings {
	// If thread_count is 0, tasks will be processed on the calling thread when resume() is called
	uint32 thread_count = 0;
	uint32 max_tasks = 0;
	bool start_paused = false;

	// Whether memory allocations are allowed on worker threads (used in assert-enabled builds only)
	bool memory_allocations_allowed = true;
};

// Entry point worker thread function
using f_worker_thread_entry_point = void (*)(uint32 thread_index, const s_thread_parameter_block *param_block);

// An individual thread pool task acts like its own thread
struct s_thread_pool_task {
	f_worker_thread_entry_point task_entry_point;
	s_thread_parameter_block parameter_block;
};

// Simple thread pool class. Spawns N threads which consume work tasks. Task dependencies should be externally managed.
class c_thread_pool {
public:
	c_thread_pool() = default;
	~c_thread_pool();

	// Non thread-safe functions: these must be called from a single thread, or using some form of synchronization

	// Starts threads processing
	void start(const s_thread_pool_settings &settings);

	// Stops all threads, returns the number of unexecuted tasks
	uint32 stop();

	// Pauses all worker threads so that they don't eat up 100% CPU usage
	void pause();

	// Resumes paused worker threads
	void resume();

	// Thread-safe functions: these can be called from any thread, including worker threads:

	bool add_task(const s_thread_pool_task &task);

private:
	// Internal representation of a task
	struct ALIGNAS_LOCK_FREE s_task {
		// A task acts like its own thread
		f_worker_thread_entry_point task_function;
		s_thread_parameter_block params;
	};

	struct s_worker_thread_context {
		c_thread_pool *this_ptr;
		uint32 worker_thread_index;
	};

	static void worker_thread_entry_point(const s_thread_parameter_block *param_block);
	void execute_all_tasks_synchronous();

	std::vector<c_thread> m_threads;

#if IS_TRUE(ASSERTS_ENABLED)
	// Whether memory allocations are allowed on worker threads
	bool m_memory_allocations_allowed = false;

	// Whether the thread pool is running - used for asserts only, not safe to access from worker threads
	bool m_running = false;
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Used to allow threads to pause in a blocking way when we don't want to hog CPU
	std::atomic<bool> m_check_paused = false;			// Lock-free guard against unnecessarily acquiring the mutex
	bool m_paused = false;								// Whether threads should be paused
	c_mutex m_pause_mutex;								// Mutex to protect the paused bool
	c_condition_variable m_pause_condition_variable;	// Used with the pause mutex

	// Queue of tasks to complete
	c_lock_free_queue<s_task> m_pending_tasks;
	c_lock_free_aligned_allocator<s_task> m_pending_tasks_element_memory;
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_pending_tasks_queue_memory;
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_pending_tasks_free_list_memory;
};

