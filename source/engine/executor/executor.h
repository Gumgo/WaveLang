#ifndef WAVELANG_EXECUTOR_H__
#define WAVELANG_EXECUTOR_H__

#include "common/common.h"
#include "common/threading/atomics.h"
#include "common/threading/lock_free.h"
#include "common/threading/semaphore.h"

#include "driver/sample_format.h"

#include "engine/task_function.h"
#include "engine/thread_pool.h"
#include "engine/events/async_event_handler.h"
#include "engine/events/event_console.h"
#include "engine/events/event_interface.h"
#include "engine/executor/buffer_manager.h"
#include "engine/executor/controller_event_manager.h"
#include "engine/executor/voice_allocator.h"
#include "engine/profiler/profiler.h"
#include "engine/sample/sample_library.h"
#include "engine/voice_interface/voice_interface.h"

class c_task_graph;
class c_buffer;
struct s_task_function;

typedef size_t (*f_process_controller_events)(
	void *context, c_wrapped_array<s_timestamped_controller_event> controller_events, int64 buffer_duration_ns);

struct s_executor_settings {
	const c_task_graph *task_graph;
	uint32 thread_count;
	uint32 sample_rate;
	uint32 max_buffer_size;
	uint32 output_channels;

	size_t controller_event_queue_size;
	f_process_controller_events process_controller_events;
	void *process_controller_events_context;

	bool event_console_enabled;
	bool profiling_enabled;
};

// $TODO we probably don't need these settings to be both in settings and chunk context. We should instead store off the
// relevant settings, then assert against the ones provided in the chunk context to make sure they don't change.
struct s_executor_chunk_context {
	uint32 sample_rate;
	uint32 output_channels;
	e_sample_format sample_format;
	uint32 frames;

	c_wrapped_array<uint8> output_buffer;
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

	struct ALIGNAS_LOCK_FREE s_task_context {
		c_atomic_int32 predecessors_remaining;
	};

	struct s_task_parameters {
		c_executor *this_ptr;
		uint32 voice_index;
		uint32 task_index;
		uint32 sample_rate;
		uint32 frames;
	};

	void initialize_internal(const s_executor_settings &settings);
	void initialize_events();
	void initialize_thread_pool();
	void initialize_buffer_manager();
	void initialize_task_memory();
	void initialize_tasks();
	void initialize_voice_allocator();
	void initialize_controller_event_manager();
	void initialize_task_contexts();
	void initialize_profiler();

	void shutdown_internal();

	void execute_internal(const s_executor_chunk_context &chunk_context);
	void process_voice(const s_executor_chunk_context &chunk_context, uint32 voice_index);

	void add_task(uint32 voice_index, uint32 task_index, uint32 sample_rate, uint32 frames);

	static void process_task_wrapper(uint32 thread_index, const s_thread_parameter_block *params);
	void process_task(uint32 thread_index, const s_task_parameters *params);
	size_t setup_task_arguments(uint32 task_index, bool include_dynamic_arguments,
		s_static_array<s_task_function_argument, k_max_task_function_arguments> &out_arguments);

	void allocate_output_buffers(uint32 task_index);
	void decrement_buffer_usages(uint32 task_index);

	// Used for array dereference
	friend struct s_task_function_context;
	const c_buffer *get_buffer_by_index(uint32 buffer_index) const;

	static void handle_event_wrapper(void *context, size_t event_size, const void *event_data);
	void handle_event(size_t event_size, const void *event_data);

	// Used to enable/disable the executor in a thread-safe manner
	c_atomic_int32 m_state;
	c_semaphore m_shutdown_signal;

	// Settings for the executor
	s_executor_settings m_settings;

	// Pool of worker threads
	c_thread_pool m_thread_pool;

	// Manages lifetime of various buffers used during processing
	c_buffer_manager m_buffer_manager;

	// Allocator for task-persistent memory
	c_lock_free_aligned_allocator<uint8> m_task_memory_allocator;

	// Pointer to each task's persistent memory for each voice
	std::vector<void *> m_voice_task_memory_pointers;

	// Manages which voices are active
	c_voice_allocator m_voice_allocator;

	// Interface for accessing voice data, e.g. which note ID is pressed
	c_voice_interface m_voice_interface;

	// Processes controller events and generates buffers for parameters
	c_controller_event_manager m_controller_event_manager;

	// Context for each task for the currently processing voice
	c_lock_free_aligned_allocator<s_task_context> m_task_contexts;

	// Total number of tasks remaining for the currently processing voice
	ALIGNAS_LOCK_FREE c_atomic_int32 m_tasks_remaining;

	// Signaled when all tasks are complete
	c_semaphore m_all_tasks_complete_signal;

	// Sample library
	c_sample_library m_sample_library;

	// Sends events from the stream threads to the event handling thread
	c_async_event_handler m_async_event_handler;

	// Event console
	c_event_console m_event_console;

	// The event interface passed into tasks
	c_event_interface m_event_interface;

	// Used to measure execution time
	c_profiler m_profiler;
};

#endif // WAVELANG_EXECUTOR_H__
