#ifndef WAVELANG_EXECUTOR_H__
#define WAVELANG_EXECUTOR_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/semaphore.h"
#include "common/threading/atomics.h"
#include "engine/buffer_allocator.h"
#include "engine/thread_pool.h"
#include "driver/sample_format.h"

class c_task_graph;
struct s_task_function_description;

struct s_executor_settings {
	const c_task_graph *task_graph;
	uint32 max_buffer_size;
	uint32 output_channels;
};

struct s_executor_chunk_context {
	double sample_rate;
	uint32 output_channels;
	e_sample_format sample_format;
	uint32 frames;

	void *output_buffers;
};

class c_executor {
public:
	c_executor();

	void initialize(const s_executor_settings &settings);
	void shutdown();

	void execute(const s_executor_chunk_context &chunk_context);

private:
	// State flow:
	// uninitialized <---> initialized
	//       ^                  |
	//       |                  |
	//     <sync>               |
	//       |                  v
	//  terminating <------- running
	enum e_state {
		k_state_uninitialized,
		k_state_initialized,
		k_state_running,
		k_state_terminating,

		k_state_count
	};

	struct s_voice_context {
		bool active;
	};

	struct ALIGNAS_LOCK_FREE s_task_context {
		c_atomic_int32 predecessors_remaining;
	};

	struct ALIGNAS_LOCK_FREE s_buffer_context {
		c_atomic_int32 usages_remaining;
		uint32 handle;
	};

	struct s_task_parameters {
		c_executor *this_ptr;
		uint32 voice_index;
		uint32 task_index;
		uint32 frames;
	};

	void initialize_internal(const s_executor_settings &settings);
	void shutdown_internal();

	void execute_internal(const s_executor_chunk_context &chunk_context);

	void add_task(uint32 voice_index, uint32 task_index, uint32 frames);

	static void process_task_wrapper(const s_thread_parameter_block *params);
	void process_task(uint32 voice_index, uint32 task_index, uint32 frames);

	void allocate_output_buffers(uint32 task_index);
	void decrement_buffer_usages(uint32 task_index);
	void decrement_buffer_usage(uint32 buffer_index);

	// Used to enable/disable the executor in a thread-safe manner
	c_atomic_int32 m_state;
	c_semaphore m_shutdown_signal;

	// The source task graph
	const c_task_graph *m_task_graph;

	// Max frames we can process at a time
	uint32 m_max_buffer_size;

	// Pool of worker threads
	c_thread_pool m_thread_pool;

	// Used to allocate buffers for processing
	c_buffer_allocator m_buffer_allocator;

	// Allocator for task-persistent memory
	c_lock_free_aligned_array_allocator<uint8> m_task_memory_allocator;

	// Pointer to each task's persistent memory for each voice
	std::vector<void *> m_voice_task_memory_pointers;

	// Context for each voice
	std::vector<s_voice_context> m_voice_contexts;

	// Context for each task for the currently processing voice
	c_lock_free_aligned_array_allocator<s_task_context> m_task_contexts;

	// Context for each buffer for the currently processing voice
	c_lock_free_aligned_array_allocator<s_buffer_context> m_buffer_contexts;

	// Buffers to accumulate the final result into
	std::vector<uint32> m_voice_output_accumulation_buffers;

	// Buffers to mix to output channels
	std::vector<uint32> m_channel_mix_buffers;

	// Total number of tasks remaining for the currently processing voice
	ALIGNAS_LOCK_FREE c_atomic_int32 m_tasks_remaining;

	// Signaled when all tasks are complete
	c_semaphore m_all_tasks_complete_signal;
};

#endif // WAVELANG_EXECUTOR_H__
