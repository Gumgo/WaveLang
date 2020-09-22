#include "common/threading/atomics.h"

#include "engine/events/async_event_handler.h"

#include <algorithm>

// Flush if we wake up and we're within this window of time until the next flush
static constexpr int64 k_flush_time_padding_ms = 50;

struct s_read_write_position {
	union {
		int64 data;			// Interface with std::atomic<int64>
		struct {
			uint32 read;	// Current read position, always ahead of write
			uint32 write;	// Current write position
		};
	};
};

// Returns true if moving from position a to position b passes position c in a circular buffer
static bool does_a_to_b_pass_c(uint32 a, uint32 b, uint32 c) {
	// a -> b passes c in these situations:
	// [ a cb ] : a < c <= b
	// [ b ac ] : b < a < c
	// [ cb a ] : c <= b < a
	// (a < c && c <= b) || (b < a && a < c) || (c <= b && b < a)
	// c1: a < c
	// c2: c <= b
	// c3: b < a
	// (c1 && c2) || (c3 && c1) || (c2 && c3)
	// c1 + c2 + c3 == 2
	bool c1 = a < c;
	bool c2 = c <= b;
	bool c3 = b < a;
	return (c1 + c2 + c3) == 2;
}

c_async_event_handler::c_async_event_handler() {
	m_initialized = false;
}

void c_async_event_handler::initialize(const s_async_event_handler_settings &settings) {
	wl_assert(!m_initialized);
	m_settings = settings;
	m_settings.max_event_size = align_size(m_settings.max_event_size, k_buffer_alignment);
	m_settings.buffer_size = align_size(m_settings.buffer_size, k_buffer_alignment);

	// Validate the settings

	wl_assert(m_settings.event_handler);
	wl_assert(m_settings.max_event_size > 0);
	// Buffer must be able to hold more than one event, because we don't allow the buffer to completely fill
	wl_assert(m_settings.buffer_size > sizeof(uint32) + m_settings.max_event_size);
	// We store offsets as uint32, so the buffer size can't exceed this capacity
	wl_assert(m_settings.buffer_size <= std::numeric_limits<uint32>::max());
	wl_assert(m_settings.flush_period_ms > 0);

	// Allocate the buffer with extra padding to avoid wrapping events
	size_t total_buffer_size = m_settings.buffer_size + m_settings.max_event_size;
	m_buffer.allocate(total_buffer_size);
	// Zero out the buffer
	zero_type(m_buffer.get_array().get_pointer(), m_buffer.get_array().get_count());

	m_initialized = true;
}

bool c_async_event_handler::is_initialized() const {
	return m_initialized;
}

void c_async_event_handler::begin_event_handling() {
	wl_assert(m_initialized);
	wl_assert(!m_event_handling_thread.is_running());

	m_flush_required = false;
	m_read_write_position = 0;
	m_stopwatch.initialize();
	m_stopwatch.reset();
	m_last_flush_time_ms = 0;
	m_event_handling_thread_terminate_flag = false;

	s_thread_definition thread_definition;
	thread_definition.thread_name = "event_handler";
	thread_definition.stack_size = 0;
	thread_definition.thread_priority = e_thread_priority::k_normal;
	thread_definition.processor = -1;
	thread_definition.thread_entry_point = event_handling_thread_function_entry_point;
	zero_type(&thread_definition.parameter_block);
	// Set a single parameter to point to 'this'
	*thread_definition.parameter_block.get_memory_typed<c_async_event_handler *>() = this;
	m_event_handling_thread.start(thread_definition);
}

void c_async_event_handler::end_event_handling() {
	wl_assert(m_event_handling_thread.is_running());

	{
		// Signal the thread to terminate
		c_scoped_lock lock(m_lock);
		m_event_handling_thread_terminate_flag = true;
		m_event_handling_thread_signal.notify_one();
	}

	m_event_handling_thread.join();
}

void c_async_event_handler::submit_event(size_t event_size, const void *event_data) {
	// When we stop the event handling thread, we should ensure that event submission on all threads is first
	// synchronously stopped so that this assert is thread-safe.
	wl_assert(m_event_handling_thread.is_running());
	wl_assert(c_thread::get_current_thread_id() != m_event_handling_thread.get_thread_id());

	wl_assert(event_size <= m_settings.max_event_size);
	if (event_size == 0) {
		// Drop empty events because writing a size of 0 to the buffer would cause the event handling thread to wait
		// forever
		return;
	}

	wl_assert(event_data);

	// Include the size header. Make sure we only ever perform aligned writes, since the size header is written as an
	// atomic operation.
	size_t total_size = align_size(sizeof(uint32) + event_size, k_buffer_alignment);

	c_wrapped_array<uint8> buffer = m_buffer.get_array();
	bool wrote_to_buffer = false;
	do {
		s_read_write_position old_position;
		s_read_write_position new_position;
		bool write_passed_read;

		// We want to atomically advance write if it doesn't pass read. We use the looping method (lock-free but not
		// wait-free) since there is no single atomic instruction that does it. As long as we can atomically get space
		// in the buffer, we don't care whether the read pointer looped around in between our read and update
		// operations. Therefore, the ABA problem is not relevant here.
		int64 expected;
		do {
			old_position.data = m_read_write_position;

			// Read position doesn't change, only write
			new_position.read = old_position.read;
			// Don't allow write position to ever exactly reach read position. This is so we can tell the difference
			// between stepping off the read position and stepping to the read position - only the first is allowed.
			// i.e. the buffer is allowed to be entirely empty but not entirely full.
			new_position.write = (old_position.write + total_size) % static_cast<uint32>(m_settings.buffer_size);

			write_passed_read = does_a_to_b_pass_c(old_position.write, new_position.write, old_position.read);
			if (write_passed_read) {
				// Our write position will pass the current read pointer, so we should flush and try again
				break;
			}

			// Keep trying until we're able to perform an iteration where m_read_write_position hasn't changed
			expected = old_position.data;
		} while (!m_read_write_position.compare_exchange_strong(expected, new_position.data));

		// We should always be aligned
		wl_assert(is_size_aligned(old_position.write, k_buffer_alignment));
		// Verify that we made progress, and that we didn't step around the entire buffer back to our initial position
		wl_assert(old_position.write != new_position.write);

		if (!write_passed_read) {
			// We have enough space to write. First write the event, leaving space for the size at the beginning.
			// old_position.write is where we start writing our header and event data
			// new_position.write is where the next event would start

			// Since we allocated our buffer with a tail equal to the size of the largest possible event, we don't need
			// to worry about splitting our event data across the end and beginning of the buffer if we wrapped. We can
			// just do a straight copy.
			wl_assert(old_position.write + total_size <= buffer.get_count());
			memcpy(buffer.get_pointer() + old_position.write + sizeof(uint32), event_data, event_size);

			// Ensure that the event data is fully written before the size is published
			std::atomic_thread_fence(std::memory_order_release);

			// Write the packet size, not the full size. The event handling thread will perform its own alignment.
			wl_assert(old_position.write + sizeof(uint32) <= m_settings.buffer_size);
			// Since we write to an aligned address in a single instruction, the write is atomic
			*reinterpret_cast<uint32 *>(buffer.get_pointer() + old_position.write) =
				cast_integer_verify<uint32>(event_size);

			// Publish the size, which publishes the entire event
			std::atomic_thread_fence(std::memory_order_release);

			// If this write will cause us to pass the halfway mark in the buffer, we should queue a flush anyway.
			// However, we don't need to block on this flush.
			uint32 read_plus_half_buffer =
				(old_position.read + m_settings.buffer_size / 2) % static_cast<uint32>(m_settings.buffer_size);
			if (does_a_to_b_pass_c(old_position.write, new_position.write, read_plus_half_buffer)) {
				// Flush but don't block
				signal_flush();
			}

			wrote_to_buffer = true;
		} else {
			// Queue a flush and wait for it to complete before trying again. It's possible that we will signal right at
			// the end of a flush and still not have enough space. In that case, we'll just try again immediately.
			signal_flush_and_wait();
		}
	} while (!wrote_to_buffer);
}

void c_async_event_handler::flush() {
	// Obtain the initial read position
	s_read_write_position current_position;
	current_position.data = m_read_write_position;

	c_wrapped_array<uint8> buffer = m_buffer.get_array();

	// Loop until we have no more to read
	bool done = false;
	do {
		wl_assert(is_size_aligned(current_position.read, k_buffer_alignment));
		wl_assert(current_position.read < m_settings.buffer_size);

		// Read the size header
		uint32 event_size = *reinterpret_cast<uint32 *>(buffer.get_pointer() + current_position.read);
		if (event_size == 0) {
			// Reached the end, or an event still being written (the data is written before the size header, and writing
			// the size header is what causes the event to be published)
			done = true;
		} else {
			wl_assert(event_size <= m_settings.max_event_size);

			// Process the event data - since the buffer has a tail, don't worry about wrapping
			m_settings.event_handler(
				m_settings.event_handler_context,
				event_size,
				buffer.get_pointer() + current_position.read + sizeof(uint32));

			// Advance the read position, making sure we first align the size
			uint32 read_advance = static_cast<uint32>(align_size(sizeof(uint32) + event_size, k_buffer_alignment));

			// Clear the buffer we just read from to 0 so we don't accidentally read it again
			zero_type(buffer.get_pointer() + current_position.read, read_advance);

			// Make sure the memset is published before updating the read pointer (though this should be unnecessary,
			// since the read pointer update is atomic)
			std::atomic_thread_fence(std::memory_order_release);

			struct s_read_advancer {
				uint32 new_position;
				int64 operator()(int64 v) const {
					s_read_write_position pos;
					pos.data = v;
					pos.read = new_position;
					return pos.data;
				}
			};

			s_read_advancer read_advancer;
			read_advancer.new_position =
				(current_position.read + read_advance) % static_cast<uint32>(m_settings.buffer_size);

			// Update read position atomically
			current_position.data = execute_atomic(m_read_write_position, read_advancer);
			current_position.read = read_advancer.new_position;
		}
	} while (!done);
}

void c_async_event_handler::signal_flush() {
	c_scoped_lock lock(m_lock);
	m_flush_required = true;
	m_event_handling_thread_signal.notify_one();
}

void c_async_event_handler::signal_flush_and_wait() {
	c_scoped_lock lock(m_lock);
	m_flush_required = true;
	m_event_handling_thread_signal.notify_one();

	// Wait until we get a signal that the flush is done
	// If we wake up spuriously, we'll just attempt to write, see that we don't have space, and start waiting again
	m_flush_complete_signal.wait(lock);
}

void c_async_event_handler::event_handling_thread_function_entry_point(const s_thread_parameter_block *params) {
	c_async_event_handler *this_ptr = *params->get_memory_typed<c_async_event_handler *>();
	this_ptr->event_handling_thread_function();
}

void c_async_event_handler::event_handling_thread_function() {
	c_scoped_lock lock(m_lock);

	bool done = false;
	do {
		int64 current_time_ms = m_stopwatch.query_ms();
		bool flush_required = false;
		uint32 sleep_time_ms = std::numeric_limits<uint32>::max();

		// If we ever implement multiple event handlers, this would become a for loop over each one, and we'll be taking
		// the min time to flush across all of them.
		{
			// Check if it's been enough time since we last flushed
			int64 time_since_last_flush_ms = current_time_ms - m_last_flush_time_ms;
			int64 time_to_next_flush_ms = std::max(m_settings.flush_period_ms - time_since_last_flush_ms, 0ll);

			if (time_to_next_flush_ms < k_flush_time_padding_ms) {
				m_flush_required = true;
			}

			if (m_flush_required) {
				sleep_time_ms = std::min(sleep_time_ms, m_settings.flush_period_ms);
			} else {
				sleep_time_ms = std::min(sleep_time_ms, static_cast<uint32>(time_to_next_flush_ms));
			}

			flush_required = m_flush_required;
		}

		m_lock.unlock();
		if (flush_required) {
			flush();
		}
		m_lock.lock();

		// We reset flush_required after all flushing occurs. It is possible that a thread will set flush_required to
		// true while we are flushing but we will miss the flush and reset it to false. This is okay because what will
		// happen is the thread that requested the flush will receive the "flush complete" signal and attempt to write
		// to the buffer, but will run out of space again and will re-queue another flush, which will not be missed the
		// second time.

		if (flush_required) {
			m_flush_required = false;
		}

		// Tell threads that the flush is done
		m_flush_complete_signal.notify_all();

		// Wait for a signal or timeout
		m_event_handling_thread_signal.wait(lock, sleep_time_ms);

		if (m_event_handling_thread_terminate_flag) {
			// Flush events - lock status doesn't matter because all event queueing threads had better be terminated if
			// we're terminating the event handling thread.
			flush();
			done = true;
		}
	} while (!done);
}