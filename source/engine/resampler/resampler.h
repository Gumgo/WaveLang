#pragma once

#include "common/common.h"

enum class e_resampler_filter {
	k_upsample_high_quality,
	k_downsample_2x_high_quality,
	k_downsample_3x_high_quality,
	k_downsample_4x_high_quality,

	k_count
};

struct s_resampler_parameters {
	uint32 upsample_factor;
	uint32 taps_per_phase;
	uint32 latency;
	c_wrapped_array<const real32> phases;
};

const s_resampler_parameters &get_resampler_parameters(e_resampler_filter resampler_filter);

class c_resampler {
public:
	static uint32 get_required_history_samples(const s_resampler_parameters &parameters) {
		// We need one sample per tap including the current index, so we need taps_per_phase - 1 samples of history
		return parameters.taps_per_phase - 1;
	}

	c_resampler(const s_resampler_parameters &parameters);
	real32 resample(c_wrapped_array<const real32> samples, uint32 sample_index, real32 fractional_sample_index) const;

private:
	real32 m_upsample_factor;
	uint32 m_taps_per_phase;
	c_wrapped_array<const real32> m_phases;
};
