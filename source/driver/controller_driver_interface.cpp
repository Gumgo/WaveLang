#include "driver/controller_driver_interface.h"
#include <algorithm>

// Pad by 1 ms - allow events 1ms before the beginning of the buffer to be included
static const int64 k_controller_event_buffer_padding_ns = k_nanoseconds_per_second / k_milliseconds_per_second;

c_controller_driver_interface::c_controller_driver_interface() {
}

c_controller_driver_interface::~c_controller_driver_interface() {
}

s_controller_driver_result c_controller_driver_interface::initialize() {
	s_controller_driver_result result;
	result.clear();

	// Initialize all sub-controllers
	result = m_controller_driver_midi.initialize(submit_controller_event_wrapper, this);
	if (result.result != k_controller_driver_result_success) {
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
	// Initialize the event queue
	m_controller_event_queue_element_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue_queue_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue_free_list_memory.allocate(settings.controller_event_queue_size + 1);
	m_controller_event_queue.initialize(
		m_controller_event_queue_element_memory.get_array(),
		m_controller_event_queue_queue_memory.get_array(),
		m_controller_event_queue_free_list_memory.get_array());

	s_controller_driver_result result = m_controller_driver_midi.start_stream(settings);
	if (result.result != k_controller_driver_result_success) {
		m_controller_event_queue_element_memory.free();
		m_controller_event_queue_queue_memory.free();
		m_controller_event_queue_free_list_memory.free();
		return result;
	}

	m_timestamp_stopwatch.initialize();
	m_timestamp_stopwatch.reset();

	return result;
}

void c_controller_driver_interface::stop_stream() {
	m_controller_driver_midi.stop_stream();

	m_controller_event_queue_element_memory.free();
	m_controller_event_queue_queue_memory.free();
	m_controller_event_queue_free_list_memory.free();
}

bool c_controller_driver_interface::is_stream_running() const {
	return m_controller_driver_midi.is_stream_running();
}

const s_controller_driver_settings &c_controller_driver_interface::get_settings() const {
	return m_controller_driver_midi.get_settings();
}

size_t c_controller_driver_interface::process_controller_events(
	c_wrapped_array<s_timestamped_controller_event> controller_events,
	int64 buffer_duration_ns) {
	size_t controller_event_count = 0;
	int64 largest_allowed_timestamp_ns = m_timestamp_stopwatch.query();
	int64 buffer_beginning_timestamp_ns = largest_allowed_timestamp_ns - buffer_duration_ns;

	bool done = false;
	while (!done) {
		s_controller_event_queue_element element;

		if (!m_controller_event_queue.peek(element) || element.timestamp_ns >= largest_allowed_timestamp_ns) {
			done = true;
		} else {
			// Since this is the only thread reading elements, we're guaranteed to pop the element we just peeked at
			m_controller_event_queue.pop();

			int64 relative_timestamp_ns = element.timestamp_ns - buffer_beginning_timestamp_ns;
			if (relative_timestamp_ns >= -k_controller_event_buffer_padding_ns) {
				relative_timestamp_ns = std::max(0ll, relative_timestamp_ns);

				if (controller_event_count < controller_events.get_count()) {
					// Copy the event into the buffer
					s_timestamped_controller_event &buffer_event = controller_events[controller_event_count];
					buffer_event.timestamp_ns = relative_timestamp_ns;
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
	void *context, const s_controller_event &controller_event) {
	static_cast<c_controller_driver_interface *>(context)->submit_controller_event(controller_event);
}

void c_controller_driver_interface::submit_controller_event(const s_controller_event &controller_event) {
	s_controller_event_queue_element element;
	element.timestamp_ns = m_timestamp_stopwatch.query();
	element.controller_event = controller_event;

	// Attempt to push the element - drop if out of space
	m_controller_event_queue.push(element);
}