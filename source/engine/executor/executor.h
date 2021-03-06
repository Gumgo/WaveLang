#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/semaphore.h"

#include "engine/controller_interface/controller_interface.h"
#include "engine/events/async_event_handler.h"
#include "engine/events/event_console.h"
#include "engine/events/event_interface.h"
#include "engine/executor/buffer_manager.h"
#include "engine/executor/controller_event_manager.h"
#include "engine/executor/task_memory_manager.h"
#include "engine/executor/voice_allocator.h"
#include "engine/profiler/profiler.h"
#include "engine/sample_format.h"
#include "engine/thread_pool.h"
#include "engine/voice_interface/voice_interface.h"

#include "instrument/instrument_stage.h"

#include "task_function/task_function.h"

#include <atomic>

class c_runtime_instrument;
class c_task_graph;

using f_process_controller_events = size_t (*)(
	void *context, c_wrapped_array<s_timestamped_controller_event> controller_events,
	real64 buffer_time_sec, real64 buffer_duration_sec);

struct s_executor_settings {
	const c_runtime_instrument *runtime_instrument;
	uint32 thread_count;
	uint32 sample_rate;
	uint32 max_buffer_size;
	uint32 input_channel_count;
	uint32 output_channel_count;

	size_t controller_event_queue_size;
	size_t max_controller_parameters;
	f_process_controller_events process_controller_events;
	void *process_controller_events_context;

	bool event_console_enabled; // $TODO move this into the runtime - executor should be passed an event callback
	bool profiling_enabled;
	real32 profiling_threshold;
};

// $TODO we probably don't need these settings to be both in settings and chunk context. We should instead store off the
// relevant settings, then assert against the ones provided in the chunk context to make sure they don't change.
struct s_executor_chunk_context {
	uint32 sample_rate;
	uint32 frames;
	real64 buffer_time_sec;

	uint32 input_channel_count;
	e_sample_format input_sample_format;
	c_wrapped_array<const uint8> input_buffer;

	uint32 output_channel_count;
	e_sample_format output_sample_format;
	c_wrapped_array<uint8> output_buffer;
};

class c_executor {
public:
	c_executor();

	void initialize(const s_executor_settings &settings, c_wrapped_array<void *> task_function_library_contexts);
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
	enum class e_state {
		k_uninitialized,
		k_initialized,
		k_running,
		k_terminating,

		k_count
	};

	struct alignas(CACHE_LINE_SIZE) s_thread_context {
		// Each thread has a pre-initialized context to avoid setting it up for each task
		s_task_function_context task_function_context;
	};

	struct ALIGNAS_LOCK_FREE s_task_context {
		std::atomic<int32> predecessors_remaining;
	};

	struct s_task_parameters {
		c_executor *this_ptr;
		e_instrument_stage instrument_stage;
		uint32 voice_index;
		uint32 task_index;
		uint32 sample_rate;
		uint32 frames;
	};

	void initialize_internal(
		const s_executor_settings &settings,
		c_wrapped_array<void *> task_function_library_contexts);
	void initialize_events();
	void initialize_thread_pool();
	void initialize_buffer_manager();
	void pre_initialize_task_function_libraries();
	void initialize_task_memory();
	void initialize_tasks();
	void post_initialize_task_function_libraries();
	void initialize_voice_allocator();
	void initialize_controller_event_manager();
	void initialize_task_contexts();
	void initialize_profiler();

	void shutdown_internal();
	void deinitialize_tasks();

	static s_task_memory_query_result task_memory_query_wrapper(
		void *context,
		e_instrument_stage instrument_stage,
		const c_task_graph *task_graph,
		uint32 task_index);
	s_task_memory_query_result task_memory_query(
		e_instrument_stage instrument_stage,
		const c_task_graph *task_graph,
		uint32 task_index);

	void execute_internal(const s_executor_chunk_context &chunk_context);
	void process_instrument_stage(
		e_instrument_stage instrument_stage,
		const s_executor_chunk_context &chunk_context,
		uint32 voice_index);

	void add_task(
		e_instrument_stage instrument_stage,
		uint32 voice_index,
		uint32 task_index,
		uint32 sample_rate,
		uint32 frames);

	static void process_task_wrapper(uint32 thread_index, const s_thread_parameter_block *params);
	void process_task(uint32 thread_index, const s_task_parameters *params);

	static void handle_event_wrapper(void *context, size_t event_size, const void *event_data);
	void handle_event(size_t event_size, const void *event_data);

	// Used to enable/disable the executor in a thread-safe manner
	std::atomic<int32> m_state;
	c_semaphore m_shutdown_signal;

	// Contexts for each task function library
	std::vector<void *> m_task_function_library_contexts;

	// Settings for the executor
	s_executor_settings m_settings;

	// Pool of worker threads
	c_thread_pool m_thread_pool;

	// Context for each thread
	c_aligned_allocator<s_thread_context, CACHE_LINE_SIZE> m_thread_contexts;

	// Voice activators don't run in the thread pool so they get their own context
	alignas(CACHE_LINE_SIZE) s_task_function_context m_voice_activator_task_function_context;

	// Manages lifetime of various buffers used during processing
	c_buffer_manager m_buffer_manager;

	// Manages user-facing memory for each task
	c_task_memory_manager m_task_memory_manager;

	// Manages which voices are active
	c_voice_allocator m_voice_allocator;

	// Interface for accessing voice data, e.g. which note ID is pressed
	c_voice_interface m_voice_interface;

	// Processes controller events and generates buffers for parameters
	c_controller_event_manager m_controller_event_manager;

	// Interface to access controller events
	c_controller_interface m_controller_interface;

	// Context for each task for the currently processing voice
	c_lock_free_aligned_allocator<s_task_context> m_task_contexts;

	// Total number of tasks remaining for the currently processing voice
	ALIGNAS_LOCK_FREE std::atomic<int32> m_tasks_remaining;

	// Signaled when all tasks are complete
	c_semaphore m_all_tasks_complete_signal;

	// Sends events from the stream threads to the event handling thread
	c_async_event_handler m_async_event_handler;

	// Event console
	c_event_console m_event_console;

	// The event interface passed into tasks
	c_event_interface m_event_interface;

	// Used to measure execution time
	c_profiler m_profiler;
};

