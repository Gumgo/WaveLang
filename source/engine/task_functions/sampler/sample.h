#pragma once

#include "common/common.h"
#include "common/math/simd.h"
#include "common/utility/aligned_allocator.h"

#include <vector>

enum class e_sample_loop_mode {
	k_none,
	k_loop,
	k_bidi_loop,

	k_count
};

struct s_sample_interpolation_coefficients {
	// Coefficients for a cubic polynomial to be evaluated for x in range [0, 1]. This polynomial models the curve
	// between samples i and i+1. It is constructed from a properly upsampled version of the signal. The coefficients
	// are stored in the order [a, b, c, d] for the polynomial a + bx + cx^2 + dx^3.
	ALIGNAS_SIMD_128 s_static_array<real32, 4> coefficients;
};

struct s_sample_data {
	// Ratio of this entry's sample rate to the base sample rate. This stores the ratio with perfect accuracy because
	// the ratio is always a power of two.
	real32 base_sample_rate_ratio;

	// Rather than storing raw samples, we store coefficients for interpolating between the ith and i+1th samples
	c_wrapped_array<const s_sample_interpolation_coefficients> samples;
};

// Contains a predefined buffer of audio sample data. This data has an associated sample rate which is independent of
// the output sample rate. A sample can optionally consist of a wavetable of sub-samples for improved resampling
// quality.
class c_sample {
public:
	static bool load_file(
		const char *filename,
		e_sample_loop_mode loop_mode,
		bool phase_shift_enabled,
		std::vector<c_sample *> &channel_samples_out);

	static c_sample *generate_wavetable(
		c_wrapped_array<const real32> harmonic_weights,
		bool phase_shift_enabled);

	c_sample() = default;
	UNCOPYABLE_MOVABLE(c_sample);

	bool is_wavetable() const;

	// If this sample is a wavetable, the following functions return information about the first entry sub-sample

	uint32 get_sample_rate() const;

	bool is_looping() const;
	uint32 get_loop_start() const;
	uint32 get_loop_end() const;
	bool is_phase_shift_enabled() const;

	// Used to query the wavetable; non-wavetable samples have an entry count of 1
	uint32 get_entry_count() const;
	const s_sample_data *get_entry(uint32 index) const;

private:
	enum class e_type {
		k_none,				// The sample has no type yet
		k_single_sample,	// A single sample, or an initialized sample in a wavetable
		k_wavetable,		// A wavetable of samples

		k_count
	};

	e_type m_type = e_type::k_none;		// What type of sample this is

	uint32 m_sample_rate = 0;			// Samples per second
	uint32 m_total_frame_count = 0;		// The total number of frames

	bool m_looping = false;				// Whether this sample loops
	uint32 m_loop_start = 0;			// Start loop point, in samples
	uint32 m_loop_end = 0;				// End loop point, in samples
	bool m_phase_shift_enabled = false;	// Whether phase shifting is allowed (implemented by doubling the loop)

	std::vector<s_sample_interpolation_coefficients> m_samples;
	std::vector<s_sample_data> m_entries;
};
