#include "common/utility/merge_sort.h"

#include "engine/executor/controller_event_manager.h"

void c_controller_event_manager::initialize(size_t max_controller_events) {
	wl_assert(max_controller_events > 0);
	m_controller_events.resize(max_controller_events);
	m_sort_scratch_buffer.resize(max_controller_events);
}

c_wrapped_array<s_timestamped_controller_event> c_controller_event_manager::get_writable_controller_events() {
	return c_wrapped_array<s_timestamped_controller_event>(&m_controller_events.front(), m_controller_events.size());
}

void c_controller_event_manager::process_controller_events(size_t controller_event_count) {
	wl_assert(controller_event_count <= m_controller_events.size());

	// Sort the event stream so that note events come first and parameter events are grouped together by ID
	struct s_controller_event_comparator {
		bool operator()(
			const s_timestamped_controller_event &event_a, const s_timestamped_controller_event &event_b) const {
			uint64 sort_key_a = 0;
			uint64 sort_key_b = 0;

			if (event_a.controller_event.event_type == k_controller_event_type_note_on ||
				event_a.controller_event.event_type == k_controller_event_type_note_off) {
				sort_key_a = 0;
			} else {
				wl_assert(event_a.controller_event.event_type == k_controller_event_type_parameter_change);
				const s_controller_event_data_parameter_change *data =
					event_a.controller_event.get_data<s_controller_event_data_parameter_change>();
				sort_key_a = data->parameter_id;
				sort_key_a++; // Offset by 1 to avoid colliding with the note sort key
			}

			if (event_b.controller_event.event_type == k_controller_event_type_note_on ||
				event_b.controller_event.event_type == k_controller_event_type_note_off) {
				sort_key_b = 0;
			} else {
				wl_assert(event_b.controller_event.event_type == k_controller_event_type_parameter_change);
				const s_controller_event_data_parameter_change *data =
					event_b.controller_event.get_data<s_controller_event_data_parameter_change>();
				sort_key_b = data->parameter_id;
				sort_key_b++; // Offset by 1 to avoid colliding with the note sort key
			}

			return (sort_key_a == sort_key_b) ?
				event_a.timestamp_sec < event_b.timestamp_sec :
				sort_key_a < sort_key_b;
		}
	};

	merge_sort(
		c_wrapped_array<s_timestamped_controller_event>(&m_controller_events.front(), controller_event_count),
		c_wrapped_array<s_timestamped_controller_event>(&m_sort_scratch_buffer.front(), controller_event_count),
		s_controller_event_comparator());

	// Form groups of note events and parameter change events
	m_note_events = c_wrapped_array_const<s_timestamped_controller_event>();

	for (size_t index = 0; index < controller_event_count; index++) {
		const s_timestamped_controller_event &controller_event = m_controller_events[index];

		if (controller_event.controller_event.event_type == k_controller_event_type_note_on ||
			controller_event.controller_event.event_type == k_controller_event_type_note_off) {
			m_note_events = c_wrapped_array_const<s_timestamped_controller_event>(
				&m_controller_events.front(), m_note_events.get_count() + 1);
		} else {
			wl_assert(controller_event.controller_event.event_type == k_controller_event_type_parameter_change);
			// $TODO
		}
	}
}

c_wrapped_array_const<s_timestamped_controller_event> c_controller_event_manager::get_note_events() const {
	return m_note_events;
}