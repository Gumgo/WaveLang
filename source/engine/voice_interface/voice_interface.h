#ifndef WAVELANG_VOICE_INTERFACE_H__
#define WAVELANG_VOICE_INTERFACE_H__

#include "common/common.h"

class c_voice_interface {
public:
	c_voice_interface();
	c_voice_interface(int32 note_id, real32 note_velocity, int32 note_release_sample);

	int32 get_note_id() const;
	real32 get_note_velocity() const;
	int32 get_note_release_sample() const;

private:
	int32 m_note_id;
	real32 m_note_velocity;
	int32 m_note_release_sample;
};

#endif // WAVELANG_VOICE_INTERFACE_H__