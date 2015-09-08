#ifndef WAVELANG_SAMPLE_H__
#define WAVELANG_SAMPLE_H__

#include "common/common.h"
#include "common/utility/aligned_allocator.h"
#include "engine/math/sse.h"
#include <vector>

enum e_sample_loop_mode {
	k_sample_loop_mode_none,
	k_sample_loop_mode_loop,
	k_sample_loop_mode_bidi_loop,

	k_sample_loop_mode_count
};

static const uint32 k_max_sample_padding = 4;

// Contains a predefined buffer of audio sample data. This data has an associated sample rate which is independent of
// the output sample rate. A sample can optionally consist of a mipmap of sub-samples for improved resampling quality.
// See the initialize function for details of mipmap requirements.
class c_sample {
public:
	static c_sample *load(const char *filename, e_sample_loop_mode loop_mode, bool phase_shift_enabled);

	c_sample();
	~c_sample();

	// Initializes the sample to contain audio data.
	// sample_data should contain channel_count sets of frame_count samples, non-interleaved
	void initialize(
		uint32 sample_rate, uint32 channel_count, uint32 frame_count,
		e_sample_loop_mode loop_mode, uint32 loop_start, uint32 loop_end, bool phase_shift_enabled,
		c_wrapped_array_const<real32> sample_data);

	// Initializes the sample to be used in a mipmap
	void initialize_for_mipmap(
		uint32 sample_rate, uint32 channel_count, uint32 frame_count,
		e_sample_loop_mode loop_mode, uint32 loop_start, uint32 loop_end, bool phase_shift_enabled,
		c_wrapped_array_const<real32> sample_data);

	// Initializes the sample as a mipmap of sub-samples containing audio data.
	// mipmap consists of a list of N samples. Each sample in mipmip must itself not be a mipmap. For any two levels n
	// and n+1:
	// - level n must have twice the sample rate as level n+1
	// - both levels must have the same channel count
	// - level n must have twice the frame count as level n+1
	// - the loop point sample indices in level n must be exactly twice the loop point indices in level n+1
	// These conditions are easiest to achieve when the frame count is a power of 2 and the sample's loop points occur
	// at the very beginning and end. This instance of c_sample takes ownership of the provided sub-samples.
	void initialize(c_wrapped_array_const<c_sample *> mipmap);

	bool is_mipmap() const;

	uint32 get_sample_rate() const;
	uint32 get_channel_count() const;

	uint32 get_first_sampling_frame() const;
	uint32 get_sampling_frame_count() const;
	uint32 get_total_frame_count() const;

	e_sample_loop_mode get_loop_mode() const;
	uint32 get_loop_start() const;
	uint32 get_loop_end() const;
	c_wrapped_array_const<real32> get_channel_sample_data(uint32 channel) const;
	bool is_phase_shift_enabled() const;

	uint32 get_mipmap_count() const;
	const c_sample *get_mipmap_sample(uint32 index) const;

private:
	enum e_type {
		k_type_none,						// The sample has no type yet
		k_type_single_sample,				// A single sample, or an initialized sample in a mipmap
		k_type_mipmap,						// A mipmap of samples
		k_type_uninitialized_mipmap_sample,	// A sample which will be in a mipmap but has not been initialized yet

		k_type_count
	};

	// Loop mode and points should already be set when calling this
	void initialize_data_with_padding(uint32 channel_count, uint32 frame_count,
		c_wrapped_array_const<real32> sample_data, uint32 edge_padding, uint32 loop_padding);

	e_type m_type;					// What type of sample this is

	uint32 m_sample_rate;			// Samples per second
	uint32 m_channel_count;			// Number of channels

	uint32 m_first_sampling_frame;	// The first frame not including beginning padding
	uint32 m_sampling_frame_count;	// The number of frames for sampling
	uint32 m_total_frame_count;		// The total number of frames including beginning and ending padding

	e_sample_loop_mode m_loop_mode;	// How this sample should loop
	uint32 m_loop_start;			// Start loop point, in samples
	uint32 m_loop_end;				// End loop point, in samples
	bool m_phase_shift_enabled;		// Whether phase shifting is allowed (implemented by extending the loop)

	c_aligned_allocator<real32, k_sse_alignment> m_sample_data;
	std::vector<c_sample *> m_mipmap;
};

#endif // WAVELANG_SAMPLE_H__