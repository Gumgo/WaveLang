#include "engine/voice_interface/voice_interface.h"

c_voice_interface::c_voice_interface() {}

c_voice_interface::c_voice_interface(int32 note_id, real32 note_velocity, int32 note_release_sample) {
	m_note_id = note_id;
	m_note_velocity = note_velocity;
	m_note_release_sample = note_release_sample;
}

int32 c_voice_interface::get_note_id() const {
	return m_note_id;
}

real32 c_voice_interface::get_note_velocity() const {
	return m_note_velocity;
}

int32 c_voice_interface::get_note_release_sample() const {
	return m_note_release_sample;
}