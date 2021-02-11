#pragma once

#include "common/common.h"
#include "common/threading/lock_free_queue.h"
#include "common/threading/mutex.h"
#include "common/utility/stopwatch.h"

#include "runtime/driver/controller_driver.h"
#include "runtime/driver/controller_driver_midi.h"

class c_controller_driver_interface {
public:
	c_controller_driver_interface();
	~c_controller_driver_interface();

	s_controller_driver_result initialize();
	void shutdown();

	uint32 get_device_count() const;
	uint32 get_default_device_index() const;
	s_controller_device_info get_device_info(uint32 device_index) const;

	bool are_settings_supported(const s_controller_driver_settings &settings) const;

	s_controller_driver_result start_stream(const s_controller_driver_settings &settings);
	void stop_stream();
	bool is_stream_running() const;
	const s_controller_driver_settings &get_settings() const;

	// Returns event count
	size_t process_controller_events(
		c_wrapped_array<s_timestamped_controller_event> controller_events,
		real64 buffer_time_sec,
		real64 buffer_duration_sec);

private:
	struct ALIGNAS_LOCK_FREE s_controller_event_queue_element : public s_timestamped_controller_event {};

	static void submit_controller_event_wrapper(void *context, const s_controller_event &controller_event);
	void submit_controller_event(const s_controller_event &controller_event);

	c_mutex m_stream_lock;

	c_controller_driver_midi m_controller_driver_midi;
	// c_controller_driver_osc m_controller_driver_osc; // $TODO

	// Queue of controller events from the stream
	c_lock_free_queue<s_controller_event_queue_element> m_controller_event_queue;
	c_lock_free_aligned_allocator<s_controller_event_queue_element> m_controller_event_queue_element_memory;
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_controller_event_queue_queue_memory;
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_controller_event_queue_free_list_memory;

	// Stopwatch for tracking timestamps
	c_stopwatch m_timestamp_stopwatch;
};

