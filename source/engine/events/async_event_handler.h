#pragma once

#include "common/common.h"
#include "common/threading/condition_variable.h"
#include "common/threading/mutex.h"
#include "common/threading/thread.h"
#include "common/utility/aligned_allocator.h"
#include "common/utility/stopwatch.h"

#include <atomic>

using f_event_handler = void (*)(void *context, size_t event_size, const void *event_data);

struct s_async_event_handler_settings {
	f_event_handler event_handler;
	void *event_handler_context;
	size_t max_event_size;
	size_t buffer_size;
	uint32 flush_period_ms;

	void set_default() {
		event_handler = nullptr;
		event_handler_context = nullptr;
		max_event_size = 1024;
		buffer_size = 4 * 1024 * 1024; // 4mb
		flush_period_ms = 250;
	}
};

class c_async_event_handler {
public:
	c_async_event_handler();

	// Functions called on the controlling thread:

	void initialize(const s_async_event_handler_settings &settings);
	bool is_initialized() const;
	void begin_event_handling();
	void end_event_handling();

	// Functions called from any thread:

	// Queues an event to be processed by all registered event handlers
	void submit_event(size_t event_size, const void *event_data);

private:
	// Called from the event handling thread to flush all queued events
	void flush();

	// Called from any thread to signal that the event handling thread should flush soon
	void signal_flush();

	// Called from any thread; signals the event handling thread to flush and waits for the flush to complete
	void signal_flush_and_wait();

	// Event handling thread function (the static one is a wrapper)
	static void event_handling_thread_function_entry_point(const s_thread_parameter_block *params);
	void event_handling_thread_function();

	static const size_t k_buffer_alignment = sizeof(uint32);

	// Whether settings have been initialized
	bool m_initialized;

	// Settings provided by the user
	s_async_event_handler_settings m_settings;

	// Allocator for the event queue buffer. The allocated size is actually buffer_size + max_event_size, in order to
	// avoid dealing with event slicing when wrapping from the end to the beginning.
	c_aligned_allocator<uint8, k_buffer_alignment> m_buffer;

	// Event handling thread
	c_thread m_event_handling_thread;

	// Indicates that a flush is required. This gets set when the log fills up too much, as well as periodically.
	bool m_flush_required;

	// Combined read position (for event handling) and write position (for event queueing)
	std::atomic<int64> m_read_write_position;

	// Used to track timing on the event handling thread
	c_stopwatch m_stopwatch;

	// Time of the last flush
	int64 m_last_flush_time_ms;

	// Used in combination with the condition variables below
	c_mutex m_lock;

	// Signals the event handling thread to wake up and flush (or terminate)
	c_condition_variable m_event_handling_thread_signal;

	// Signals threads that a flush has been completed
	c_condition_variable m_flush_complete_signal;

	// Flag indicating that the event handling thread should terminate
	bool m_event_handling_thread_terminate_flag;
};

