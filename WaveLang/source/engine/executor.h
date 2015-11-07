#ifndef WAVELANG_EXECUTOR_H__
#define WAVELANG_EXECUTOR_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/semaphore.h"
#include "common/threading/atomics.h"
#include "engine/buffer_allocator.h"
#include "engine/thread_pool.h"
#include "engine/profiler/profiler.h"
#include "driver/sample_format.h"
#include "engine/sample/sample_library.h"

class c_task_graph;
struct s_task_function;

struct s_executor_settings {
	const c_task_graph *task_graph;
	uint32 thread_count;
	uint32 sample_rate;
	uint32 max_buffer_size;
	uint32 output_channels;

	bool profiling_enabled;
};

// $TODO we probably don't need these settings to be both in settings and chunk context. We should instead store off the
// relevant settings, then assert against the ones provided in the chunk context to make sure they don't change.
struct s_executor_chunk_context {
	uint32 sample_rate;
	uint32 output_channels;
	e_sample_format sample_format;
	uint32 frames;

	// $TODO make this a wrapped array
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
		uint32 sample_rate;
		uint32 frames;
	};

	void initialize_internal(const s_executor_settings &settings);
	void initialize_thread_pool(const s_executor_settings &settings);
	void initialize_buffer_allocator(const s_executor_settings &settings);
	void initialize_task_memory(const s_executor_settings &settings);
	void initialize_tasks(const s_executor_settings &settings);
	void initialize_voice_contexts();
	void initialize_task_contexts();
	void initialize_buffer_contexts();
	void initialize_profiler(const s_executor_settings &settings);

	void shutdown_internal();

	void execute_internal(const s_executor_chunk_context &chunk_context);
	void process_voice(const s_executor_chunk_context &chunk_context, uint32 voice);
	void swap_output_buffers_with_accumulation_buffers(const s_executor_chunk_context &chunk_context);
	void add_output_buffers_to_accumulation_buffers(const s_executor_chunk_context &chunk_context);
#if PREDEFINED(ASSERTS_ENABLED)
	void assert_all_output_buffers_free();
#else // PREDEFINED(ASSERTS_ENABLED)
	void assert_all_output_buffers_free() {}
#endif // PREDEFINED(ASSERTS_ENABLED)
	void allocate_and_clear_output_buffers(const s_executor_chunk_context &chunk_context);
	void mix_accumulation_buffers_to_channel_buffers(const s_executor_chunk_context &chunk_context);

	void add_task(uint32 voice_index, uint32 task_index, uint32 sample_rate, uint32 frames);

	static void process_task_wrapper(uint32 thread_index, const s_thread_parameter_block *params);
	void process_task(uint32 thread_index, const s_task_parameters *params);

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
	c_lock_free_aligned_allocator<uint8> m_task_memory_allocator;

	// Pointer to each task's persistent memory for each voice
	std::vector<void *> m_voice_task_memory_pointers;

	// Context for each voice
	std::vector<s_voice_context> m_voice_contexts;

	// Context for each task for the currently processing voice
	c_lock_free_aligned_allocator<s_task_context> m_task_contexts;

	// Context for each buffer for the currently processing voice
	c_lock_free_aligned_allocator<s_buffer_context> m_buffer_contexts;

	// Buffers to accumulate the final result into
	std::vector<uint32> m_voice_output_accumulation_buffers;

	// Buffers to mix to output channels
	std::vector<uint32> m_channel_mix_buffers;

	// Total number of tasks remaining for the currently processing voice
	ALIGNAS_LOCK_FREE c_atomic_int32 m_tasks_remaining;

	// Signaled when all tasks are complete
	c_semaphore m_all_tasks_complete_signal;

	// Sample library
	c_sample_library m_sample_library;

	// Used to measure execution time
	bool m_profiling_enabled;
	c_profiler m_profiler;
};

#endif // WAVELANG_EXECUTOR_H__
