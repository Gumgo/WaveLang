#ifndef WAVELANG_THREAD_POOL_H__
#define WAVELANG_THREAD_POOL_H__

#include "common/common.h"
#include "common/threading/thread.h"
#include "common/threading/lock_free_queue.h"
#include "common/threading/mutex.h"
#include "common/threading/condition_variable.h"

#include <vector>

struct s_thread_pool_settings {
	uint32 thread_count;
	uint32 max_tasks;
	bool start_paused;
};

// An individual thread pool task acts like its own thread
struct s_thread_pool_task {
	f_thread_entry_point task_entry_point;
	s_thread_parameter_block parameter_block;
};

// Simple thread pool class. Spawns N threads which consume work tasks. Task dependencies should be externally managed.
class c_thread_pool {
public:
	c_thread_pool();
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
		f_thread_entry_point task_function;
		s_thread_parameter_block params;
	};

	struct s_worker_thread_context {
		c_thread_pool *this_ptr;
		uint32 worker_thread_index;
	};

	static void worker_thread_entry_point(const s_thread_parameter_block *param_block);

	std::vector<c_thread> m_threads;

	// Used to allow threads to pause in a blocking way when we don't want to hog CPU
	c_atomic_int32 m_check_paused;						// Lock-free guard against unnecessarily acquiring the mutex
	bool m_paused;										// Whether threads should be paused
	c_mutex m_pause_mutex;								// Mutex to protect the paused bool
	c_condition_variable m_pause_condition_variable;	// Used with the pause mutex

	// Queue of tasks to complete
	c_lock_free_queue<s_task> m_pending_tasks;
	c_lock_free_aligned_array_allocator<s_task> m_pending_tasks_element_memory;
	c_lock_free_aligned_array_allocator<s_aligned_lock_free_handle> m_pending_tasks_queue_memory;
	c_lock_free_aligned_array_allocator<s_aligned_lock_free_handle> m_pending_tasks_free_list_memory;
};

#endif // WAVELANG_THREAD_POOL_H__
