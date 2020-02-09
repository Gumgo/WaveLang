#include "common/utility/merge_sort.h"

#include "engine/executor/controller_event_manager.h"

void c_controller_event_manager::initialize(size_t max_controller_events, size_t max_parameter_count) {
	wl_assert(max_controller_events > 0);

	m_buffer = -1;
	m_controller_events.resize(max_controller_events);
	m_sort_scratch_buffer.resize(max_controller_events);

	size_t bucket_count = max_parameter_count;
	m_parameter_state_table.initialize(bucket_count, max_parameter_count);
}

c_wrapped_array<s_timestamped_controller_event> c_controller_event_manager::get_writable_controller_events() {
	return c_wrapped_array<s_timestamped_controller_event>(&m_controller_events.front(), m_controller_events.size());
}

void c_controller_event_manager::process_controller_events(size_t controller_event_count) {
	wl_assert(controller_event_count <= m_controller_events.size());

	m_buffer++;

	// Sort the event stream so that note events come first and parameter events are grouped together by ID
	struct s_controller_event_comparator {
		bool operator()(
			const s_timestamped_controller_event &event_a, const s_timestamped_controller_event &event_b) const {
			uint64 sort_key_a = 0;
			uint64 sort_key_b = 0;

			if (event_a.controller_event.event_type == e_controller_event_type::k_note_on ||
				event_a.controller_event.event_type == e_controller_event_type::k_note_off) {
				sort_key_a = 0;
			} else {
				wl_assert(event_a.controller_event.event_type == e_controller_event_type::k_parameter_change);
				const s_controller_event_data_parameter_change *data =
					event_a.controller_event.get_data<s_controller_event_data_parameter_change>();
				sort_key_a = data->parameter_id;
				sort_key_a++; // Offset by 1 to avoid colliding with the note sort key
			}

			if (event_b.controller_event.event_type == e_controller_event_type::k_note_on ||
				event_b.controller_event.event_type == e_controller_event_type::k_note_off) {
				sort_key_b = 0;
			} else {
				wl_assert(event_b.controller_event.event_type == e_controller_event_type::k_parameter_change);
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

	// Form groups of note events
	m_note_events = c_wrapped_array<const s_timestamped_controller_event>();

	size_t parameter_change_start_index = 0;
	size_t parameter_change_count = 0;
	e_controller_event_type previous_event_type = e_controller_event_type::k_count;
	uint32 previous_parameter_change_id = 0;

	for (size_t index = 0; index < controller_event_count; index++) {
		const s_timestamped_controller_event &controller_event = m_controller_events[index];

		if (controller_event.controller_event.event_type == e_controller_event_type::k_note_on ||
			controller_event.controller_event.event_type == e_controller_event_type::k_note_off) {
			m_note_events = c_wrapped_array<const s_timestamped_controller_event>(
				&m_controller_events.front(), m_note_events.get_count() + 1);
		} else {
			wl_assert(controller_event.controller_event.event_type == e_controller_event_type::k_parameter_change);

			// Check if this is the first parameter change event for this ID
			const s_controller_event_data_parameter_change *parameter_change_event_data =
				controller_event.controller_event.get_data<s_controller_event_data_parameter_change>();

			if (previous_event_type != e_controller_event_type::k_parameter_change ||
				previous_parameter_change_id != parameter_change_event_data->parameter_id) {
				// This is the first parameter change event we've seen for this ID
				if (parameter_change_count > 0) {
					// Store the previous parameter event list
					update_parameter_state(
						previous_parameter_change_id, parameter_change_start_index, parameter_change_count);
				}

				parameter_change_start_index = index;
				parameter_change_count = 1;
				previous_parameter_change_id = parameter_change_event_data->parameter_id;
			} else {
				parameter_change_count++;
			}
		}

		previous_event_type = controller_event.controller_event.event_type;
	}

	// Store the last parameter change event list if one exists
	if (parameter_change_count > 0) {
		update_parameter_state(previous_parameter_change_id, parameter_change_start_index, parameter_change_count);
	}
}

c_wrapped_array<const s_timestamped_controller_event> c_controller_event_manager::get_note_events() const {
	return m_note_events;
}

c_wrapped_array<const s_timestamped_controller_event> c_controller_event_manager::get_parameter_change_events(
	uint32 parameter_id, real32 &out_previous_value) const {
	c_wrapped_array<const s_timestamped_controller_event> result;

	const s_parameter_state *parameter_state = m_parameter_state_table.find(parameter_id);
	if (!parameter_state) {
		// This parameter hasn't been used yet
		out_previous_value = 0.0f;
	} else if (parameter_state->last_buffer_active == m_buffer) {
		// There are new events for this buffer
		out_previous_value = parameter_state->previous_value;
		result = parameter_state->current_events;
	} else {
		wl_assert(parameter_state->last_buffer_active < m_buffer);

		// This parameter wasn't updated for this buffer. We want our previous value to be the value after the last
		// update, so we actually return next_previous_value, since previous_value refers to the value before the last
		// update. There are no events for this buffer.
		out_previous_value = parameter_state->next_previous_value;
	}

	return result;
}

void c_controller_event_manager::update_parameter_state(
	uint32 parameter_id, size_t event_start_index, size_t event_count) {
	wl_assert(event_count > 0);

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = event_start_index; index < event_count; index++) {
		wl_assert(
			m_controller_events[index].controller_event.event_type == e_controller_event_type::k_parameter_change);
		const s_controller_event_data_parameter_change *parameter_change_event_data =
			m_controller_events[index].controller_event.get_data<s_controller_event_data_parameter_change>();
		wl_assert(parameter_change_event_data->parameter_id == parameter_id);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	c_wrapped_array<const s_timestamped_controller_event> events(&m_controller_events[event_start_index], event_count);
	s_parameter_state *parameter_state = m_parameter_state_table.find(parameter_id);

	if (!parameter_state) {
		parameter_state = m_parameter_state_table.insert(parameter_id);
		if (!parameter_state) {
			// $TODO report error?
			return;
		}

		parameter_state->next_previous_value = 0.0f;
	}

	wl_assert(parameter_state);
	parameter_state->last_buffer_active = m_buffer;
	parameter_state->previous_value = parameter_state->next_previous_value;
	parameter_state->next_previous_value =
		events[event_count - 1].controller_event.get_data<s_controller_event_data_parameter_change>()->value;
	parameter_state->current_events = events;
}
