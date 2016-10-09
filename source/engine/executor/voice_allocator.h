#ifndef WAVELANG_VOICE_ALLOCATOR_H__
#define WAVELANG_VOICE_ALLOCATOR_H__

#include "common/common.h"
#include "common/utility/linked_array.h"

#include "driver/controller.h"

#include <vector>

class c_voice_allocator {
public:
	// Represents a voice, either active or inactive
	struct s_voice {
		bool active;					// Whether this voice is currently active
		bool activated_this_chunk;		// Whether this voice became active this chunk
		bool released;					// Whether this note has been released
		uint32 chunk_offset_samples;	// When this voice starts in the current chunk
		int32 note_id;					// The ID of the note being played for the voice
		real32 note_velocity;			// Velocity at which the note is played
		int32 note_release_sample;		// Offset into the chunk at which this note is released, or -1
	};

	c_voice_allocator();
	~c_voice_allocator();

	void initialize(uint32 max_voices);
	void shutdown();

	// Assigns new voices for this chunk, possibly replacing existing voices
	void allocate_voices_for_chunk(
		c_wrapped_array_const<s_timestamped_controller_event> controller_events,
		uint32 sample_rate, uint32 chunk_sample_count);

	// Marks a voice as inactive, freeing up the slot. The synth must explicitly notify the runtime when a voice should
	// be disabled - we cannot know this information from note-up events because many instruments don't immediately cut
	// off when a key is released.
	void disable_voice(uint32 voice_index);

	uint32 get_voice_count() const;
	const s_voice &get_voice(uint32 voice_index) const;

private:
	// Voice allocation algorithm:
	// Initially all voices are inactive. Each time we receive a note-down event, we determine the time offset into the
	// current chunk and attempt to allocate an empty slot for that voice. If there are no empty slots, we cancel the
	// oldest voice and replace it with the new one.

	// Ideally it would be nice if we could have the completely deterministic (w.r.t. chunking randomness) behavior of
	// always playing up to exactly N voices at once with sample-accuracy, but that behavior requires that we have the
	// ability to process a voice multiple times within the same chunk - e.g. note A plays in voice 0 for the first half
	// of the chunk and note B plays in voice 0 for the second half - which is complicated and could lead to
	// unpredictable performance. With our compromise, a voice will always be cut off at the beginning of a chunk and
	// there may be some number of silent samples before the next voice replaces it.

	// Array of all voices, active and inactive
	std::vector<s_voice> m_voices;

	// Time-ordered list of voices. Inactive voices are at the front, followed by active voices ordered from oldest to
	// newest. We use this list to quickly find the oldest voice to replace.
	s_linked_array_list m_voice_list;
	c_linked_array m_voice_list_nodes;
};

#endif // WAVELANG_VOICE_ALLOCATOR_H__