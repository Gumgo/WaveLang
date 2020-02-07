#pragma once

#include "common/common.h"
#include "common/utility/aligned_allocator.h"

#include "engine/math/simd.h"

#include <vector>

enum e_sample_loop_mode {
	k_sample_loop_mode_none,
	k_sample_loop_mode_loop,
	k_sample_loop_mode_bidi_loop,

	k_sample_loop_mode_count
};

static const uint32 k_max_sample_padding = 4;

// Contains a predefined buffer of audio sample data. This data has an associated sample rate which is independent of
// the output sample rate. A sample can optionally consist of a wavetable of sub-samples for improved resampling
// quality. See the initialize function for details of wavetable requirements.
class c_sample {
public:
	static c_sample *load_file(
		const char *filename,
		e_sample_loop_mode loop_mode,
		bool phase_shift_enabled);

	static c_sample *generate_wavetable(
		c_wrapped_array<const real32> harmonic_weights,
		uint32 sample_count,
		bool phase_shift_enabled);

	c_sample();

	bool is_wavetable() const;

	// If this sample is a wavetable, the following functions return information about the first entry sub-sample

	uint32 get_sample_rate() const;
	real64 get_base_sample_rate_ratio() const;
	uint32 get_channel_count() const;

	uint32 get_first_sampling_frame() const;
	uint32 get_sampling_frame_count() const;
	uint32 get_total_frame_count() const;

	e_sample_loop_mode get_loop_mode() const;
	uint32 get_loop_start() const;
	uint32 get_loop_end() const;
	c_wrapped_array<const real32> get_channel_sample_data(uint32 channel) const;
	bool is_phase_shift_enabled() const;

	uint32 get_wavetable_entry_count() const;
	const c_sample *get_wavetable_entry(uint32 index) const;

private:
	enum e_type {
		k_type_none,							// The sample has no type yet
		k_type_single_sample,					// A single sample, or an initialized sample in a wavetable
		k_type_wavetable,						// A wavetable of samples

		k_type_count
	};

	static bool load_wave(
		std::ifstream &file, e_sample_loop_mode loop_mode, bool phase_shift_enabled, c_sample *out_sample);

	// Initializes the sample to contain audio data.
	// sample_data should contain channel_count sets of frame_count samples, non-interleaved
	void initialize(
		uint32 sample_rate,
		uint32 channel_count,
		uint32 frame_count,
		e_sample_loop_mode loop_mode,
		uint32 loop_start,
		uint32 loop_end,
		bool phase_shift_enabled,
		c_wrapped_array<const real32> sample_data);

	// Initializes the sample using the already filled in wavetable array of sub samples, verifying the following:
	// For any two adjacent wavetable entries n and n+1:
	// - level n must have either the same or twice the sample rate as level n+1
	// - both levels must have the same channel count
	// - level n must have twice the sampling frame count as level n+1
	// - the loop point sample indices in level n must be exactly twice the loop point indices in level n+1
	// These conditions are easiest to achieve when the frame count is a power of 2 and the sample's loop points occur
	// at the very beginning and end. This instance of c_sample takes ownership of the provided sub-samples.
	void initialize_wavetable();

	// Loop mode and points should already be set when calling this
	// If destination is null, the sample's allocator will be used
	void initialize_data_with_padding(
		uint32 channel_count,
		uint32 frame_count,
		c_wrapped_array<const real32> sample_data,
		c_wrapped_array<real32> *destination,
		bool initialize_metadata_only);

	e_type m_type;						// What type of sample this is

	uint32 m_sample_rate;				// Samples per second
	real64 m_base_sample_rate_ratio;	// Ratio of this entry's sample rate to the base sample rate
	uint32 m_channel_count;				// Number of channels

	uint32 m_first_sampling_frame;		// The first frame not including beginning padding
	uint32 m_sampling_frame_count;		// The number of frames for sampling
	uint32 m_total_frame_count;			// The total number of frames including beginning and ending padding

	e_sample_loop_mode m_loop_mode;		// How this sample should loop
	uint32 m_loop_start;				// Start loop point, in samples
	uint32 m_loop_end;					// End loop point, in samples
	bool m_phase_shift_enabled;			// Whether phase shifting is allowed (implemented by doubling the loop)

	c_aligned_allocator<real32, k_simd_alignment> m_sample_data_allocator;
	c_wrapped_array<const real32> m_sample_data;
	std::vector<c_sample> m_wavetable;
};

