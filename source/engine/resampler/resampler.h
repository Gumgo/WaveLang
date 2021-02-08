#pragma once

#include "common/common.h"

#include "instrument/resampler/resampler.h"

c_wrapped_array<const real32> get_resampler_phases(e_resampler_filter resampler_filter);

class c_resampler {
public:
	static uint32 get_required_history_samples(const s_resampler_parameters &parameters) {
		// We need one sample per tap including the current index, so we need taps_per_phase - 1 samples of history
		return parameters.taps_per_phase - 1;
	}

	c_resampler(const s_resampler_parameters &parameters, c_wrapped_array<const real32> phases);
	real32 resample(c_wrapped_array<const real32> samples, size_t sample_index, real32 fractional_sample_index) const;

private:
	real32 m_upsample_factor;
	uint32 m_taps_per_phase;
	c_wrapped_array<const real32> m_phases;
};
