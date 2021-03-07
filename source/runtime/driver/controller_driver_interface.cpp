#include "runtime/driver/controller_driver_interface.h"

#include <algorithm>

// Pad by 100ms to pick up late events
static constexpr real64 k_controller_event_buffer_padding_sec = 0.1;

c_controller_driver_interface::c_controller_driver_interface() {}

c_controller_driver_interface::~c_controller_driver_interface() {}

s_controller_driver_result c_controller_driver_interface::initialize() {
	s_controller_driver_result result;
	result.clear();

	// Initialize all sub-controllers
	result = m_controller_driver_midi.initialize(submit_controller_event_wrapper, this);
	if (result.result != e_controller_driver_result::k_success) {
		return result;
	}

	return result;
}

void c_controller_driver_interface::shutdown() {
	m_controller_driver_midi.shutdown();
}

uint32 c_controller_driver_interface::get_device_count() const {
	return m_controller_driver_midi.get_device_count();
}

uint32 c_controller_driver_interface::get_default_device_index() const {
	return m_controller_driver_midi.get_default_device_index();
}

s_controller_device_info c_controller_driver_interface::get_device_info(uint32 device_index) const {
	return m_controller_driver_midi.get_device_info(device_index);
}

bool c_controller_driver_interface::are_settings_supported(const s_controller_driver_settings &settings) const {
	return m_controller_driver_midi.are_settings_supported(settings);
}

s_controller_driver_result c_controller_driver_interface::start_stream(const s_controller_driver_settings &settings) {
	// Lock to ensure that we don't try to submit events while the stream is starting
	c_scoped_lock stream_lock(m_stream_lock);

	// Initialize the event queue
	m_controller_event_queue_element_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue_queue_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue_free_list_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue.initialize(
		m_controller_event_queue_element_memory.get_array(),
		m_controller_event_queue_queue_memory.get_array(),
		m_controller_event_queue_free_list_memory.get_array());

	s_controller_driver_result result = m_controller_driver_midi.start_stream(settings);
	if (result.result != e_controller_driver_result::k_success) {
		m_controller_event_queue_element_memory.free_memory();
		m_controller_event_queue_queue_memory.free_memory();
		m_controller_event_queue_free_list_memory.free_memory();
		return result;
	}

	m_timestamp_stopwatch.initialize();
	m_timestamp_stopwatch.reset();

	return result;
}

void c_controller_driver_interface::stop_stream() {
	// Lock to make sure we don't submit events while the stream is stopping
	c_scoped_lock stream_lock(m_stream_lock);

	m_controller_driver_midi.stop_stream();

	m_controller_event_queue_element_memory.free_memory();
	m_controller_event_queue_queue_memory.free_memory();
	m_controller_event_queue_free_list_memory.free_memory();
}

bool c_controller_driver_interface::is_stream_running() const {
	return m_controller_driver_midi.is_stream_running();
}

const s_controller_driver_settings &c_controller_driver_interface::get_settings() const {
	return m_controller_driver_midi.get_settings();
}

size_t c_controller_driver_interface::process_controller_events(
	c_wrapped_array<s_timestamped_controller_event> controller_events,
	real64 buffer_time_sec,
	real64 buffer_duration_sec) {
	size_t controller_event_count = 0;

	// The provided buffer time represents the start time of the buffer that's about to be processed, taking expected
	// latency into account. Ideally, this time is the current time (it should be on a realtime system). In a truly
	// "realtime" system where we process events the moment they come in, this chunk would consume events from
	// buffer_time_sec to buffer_time_sec + buffer_duration_sec. However, those events are actually in the future
	// relative to now, so instead, use the events from the last chunk. Finally, to compensate for jitter from the
	// scheduler and other sources, we add some extra ("unknown") latency.

	//                 Unknown latency   Known latency
	//                         /-------+-----------------------\
	//             Events      |       |                       |
	// --------[-$---$-----$--]------------------------------------------------------------
	//               |                                         |
	//               \---------------\                         |
	//                               v                         |
	//         Prev buffer     This buffer now                 |
	// --------[==============][=$===$=====$==]--------------------------------------------
	//                         |
	//                         \-------------------------------\
	//                                                         v
	// --------------------------------------------------------[=$===$=====$==]------------
	//                                                         Buffer is sent to ADC

	real64 smallest_allowed_timestamp_sec = buffer_time_sec - buffer_duration_sec - get_settings().unknown_latency;
	real64 largest_allowed_timestamp_sec = smallest_allowed_timestamp_sec + buffer_duration_sec;

	bool done = false;
	while (!done) {
		s_controller_event_queue_element element;

		if (!m_controller_event_queue.peek(element) || element.timestamp_sec >= largest_allowed_timestamp_sec) {
			done = true;
		} else {
			// Since this is the only thread reading elements, we're guaranteed to pop the element we just peeked at
			m_controller_event_queue.pop();

			real64 relative_timestamp_sec = element.timestamp_sec - smallest_allowed_timestamp_sec;
			if (relative_timestamp_sec >= -k_controller_event_buffer_padding_sec) {
				relative_timestamp_sec = std::max(0.0, relative_timestamp_sec);

				if (controller_event_count < controller_events.get_count()) {
					// Copy the event into the buffer
					s_timestamped_controller_event &buffer_event = controller_events[controller_event_count];
					buffer_event.timestamp_sec = relative_timestamp_sec;
					buffer_event.controller_event = element.controller_event;
					controller_event_count++;
				} else {
					// Too many events
				}
			} else {
				// Drop the event - it's in the past
			}
		}
	}

	// Return the number of events we provided to the buffer
	return controller_event_count;
}

void c_controller_driver_interface::submit_controller_event_wrapper(
	void *context,
	const s_controller_event &controller_event) {
	static_cast<c_controller_driver_interface *>(context)->submit_controller_event(controller_event);
}

void c_controller_driver_interface::submit_controller_event(const s_controller_event &controller_event) {
	// Lock to make sure we don't submit events while the stream is starting or stopping
	c_scoped_lock stream_lock(m_stream_lock);

	if (!is_stream_running()) {
		return;
	}

	const s_controller_driver_settings &settings = get_settings();

	if (settings.event_hook) {
		bool consumed = settings.event_hook(settings.event_hook_context, controller_event);
		if (consumed) {
			return;
		}
	}

	s_controller_event_queue_element element;
	element.timestamp_sec = settings.clock ? settings.clock(settings.clock_context) : 0.0;
	element.controller_event = controller_event;

	// Attempt to push the element - drop if out of space
	m_controller_event_queue.push(element);
}
