#include "common/utility/stopwatch.h"

#include "engine/executor/voice_allocator.h"

#include <algorithm>

c_voice_allocator::c_voice_allocator() {
}

c_voice_allocator::~c_voice_allocator() {
}

void c_voice_allocator::initialize(uint32 max_voices) {
	wl_assert(max_voices > 0);
	m_voices.resize(max_voices);
	m_voice_list_nodes.initialize(max_voices);

	// All voices are initially inactive and the voice list is put in its initial state
	m_voice_list_nodes.initialize_list(m_voice_list);
	for (size_t index = 0; index < max_voices; index++) {
		ZERO_STRUCT(&m_voices[index]);

		size_t list_node_index = m_voice_list_nodes.allocate_node();
		m_voice_list_nodes.push_node_onto_list_back(m_voice_list, list_node_index);
	}
}

void c_voice_allocator::shutdown() {
	m_voices.clear();
}

void c_voice_allocator::allocate_voices_for_chunk(
	c_wrapped_array_const<s_timestamped_controller_event> controller_events,
	uint32 sample_rate, uint32 chunk_sample_count) {
	wl_assert(!m_voices.empty());

	// Any active voice with an offset the previous chunk should no longer have an offset
	for (size_t index = m_voice_list.back;
		 index != c_linked_array::k_invalid_linked_array_index;
		 index = m_voice_list_nodes.get_prev_node(index)) {
		s_voice &voice = m_voices[index];
		if (!voice.active) {
			break; // Break on first inactive voice
		}

		wl_assert(voice.chunk_offset_samples == 0 || voice.activated_this_chunk);
		voice.activated_this_chunk = false;
		voice.chunk_offset_samples = 0;
		voice.note_release_sample = -1;
	}

	for (size_t event_index = 0; event_index < controller_events.get_count(); event_index++) {
		const s_timestamped_controller_event &controller_event = controller_events[event_index];

		if (controller_event.controller_event.event_type == k_controller_event_type_note_on) {
			const s_controller_event_data_note_on *note_on_data =
				controller_event.controller_event.get_data<s_controller_event_data_note_on>();

			// Ignore if this note is already active and not released
			bool already_active = false;
			for (size_t index = m_voice_list.back;
				 !already_active && index != c_linked_array::k_invalid_linked_array_index;
				 index = m_voice_list_nodes.get_prev_node(index)) {
				s_voice &voice = m_voices[index];
				if (!voice.active) {
					break;
				}

				already_active = (voice.note_id == note_on_data->note_id) && !voice.released;
			}

			if (already_active) {
				// Skip this note-on event
				continue;
			}

			// Grab the first voice in the list. This will first attempt to grab inactive voices, then will grab the
			// oldest active voice.
			size_t voice_index = m_voice_list.front;
			s_voice &voice = m_voices[voice_index];

			// Move the node to the back of the list
			m_voice_list_nodes.remove_node_from_list(m_voice_list, voice_index);
			m_voice_list_nodes.push_node_onto_list_back(m_voice_list, voice_index);

			// Possible alternative: prioritize the first N notes for this chunk, rather than the list. I don't think this
			// really makes sense though, as it seems more expected to have the last N notes pressed play.
			//if (voice.activated_this_chunk) {
			//	continue;
			//}

			voice.active = true;
			voice.activated_this_chunk = true;
			voice.released = false;

			// offset_samples = timestamp_ns / ns_per_sample
			int64 offset_samples = controller_event.timestamp_ns * sample_rate / k_nanoseconds_per_second;
			offset_samples = std::min(std::max(offset_samples, 0ll), static_cast<int64>(sample_rate - 1));

			voice.chunk_offset_samples = cast_integer_verify<uint32>(offset_samples);
			voice.note_id = note_on_data->note_id;
			voice.note_velocity = note_on_data->velocity;
			voice.note_release_sample = -1;
		} else if (controller_event.controller_event.event_type == k_controller_event_type_note_off) {
			const s_controller_event_data_note_off *note_off_data =
				controller_event.controller_event.get_data<s_controller_event_data_note_off>();

			// Find the active voice with this note ID and set its release sample
			for (size_t index = m_voice_list.back;
				 index != c_linked_array::k_invalid_linked_array_index;
				 index = m_voice_list_nodes.get_prev_node(index)) {
				s_voice &voice = m_voices[index];
				if (!voice.active) {
					break;
				}

				if (voice.note_id == note_off_data->note_id && !voice.released) {
					int64 offset_samples = controller_event.timestamp_ns * sample_rate / k_nanoseconds_per_second;
					offset_samples = std::min(std::max(offset_samples, 0ll), static_cast<int64>(sample_rate - 1));
					voice.note_release_sample = cast_integer_verify<uint32>(offset_samples);
					voice.released = true;
					break;
				}
			}
		} else {
			// Ignore other event types - we really shouldn't even be getting them here
		}
	}
}

void c_voice_allocator::disable_voice(uint32 voice_index) {
	s_voice &voice = m_voices[voice_index];

	if (!voice.active) {
		return;
	}

	voice.active = false;
	voice.activated_this_chunk = false;

	// Move this voice to the front of the list
	m_voice_list_nodes.remove_node_from_list(m_voice_list, voice_index);
	m_voice_list_nodes.push_node_onto_list_front(m_voice_list, voice_index);
}

uint32 c_voice_allocator::get_voice_count() const {
	return static_cast<uint32>(m_voices.size());
}

const c_voice_allocator::s_voice &c_voice_allocator::get_voice(uint32 voice_index) const {
	return m_voices[voice_index];
}