#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_FETCH_SAMPLE_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_FETCH_SAMPLE_H__

#include "common/common.h"

#include "engine/math/math.h"
#include "engine/sample/sample.h"
#include "engine/task_functions/sampler/sinc_window.h"

// $TODO get rid of branching in here if possible - get rid of the sample shift thing by making several lookup tables,
// one for each level of shifting

// Calculates the interpolated sample at the given time. The input sample should not be a mipmap.
real32 fetch_sample(const c_sample *sample, uint32 channel, real64 sample_index);

// Calculates the given number of wavetable samples (max 4) and stores them at the given location. Note: if less than 4
// samples are requested, the remaining ones up to 4 will be filled with garbage data; it is expected that these will
// later be overwritten, or are past the end of the buffer but exist for padding.
void fetch_wavetable_samples(
	const c_sample *sample, uint32 channel, real32 stream_sample_rate, real32 sample_rate_0, const c_real32_4 &speed,
	size_t count, const s_static_array<real64, k_simd_block_elements> &samples, real32 *out_ptr);

/*
Windowed sinc sampling chart
Serves as a visual representation for why our padding size must be (window size)/2
window size = 5
padding = (5/2) = 2
+ = sample
--- = time between samples
s = first sampling sample
e = first non-sampling sample (i.e. first end padding sample)
A = earliest time we are allowed to sample
B = latest time we are allowed to sample from (B can get arbitrarily close to e)
x = samples required for computing A or B
				s														e
		+---+---A---+---+---+---+---+---+---+---+---+---+---+---+---+--B+---+---
+-------x-------+										+-------x-------+
	+-------x-------+										+-------x-------+
		+-------x-------+										+-------x-------+
			+-------x-------+										+-------x-------+

Consider computing the following interpolated sample S:

+-------+-------+-------+-----S-+-------+-------+-------+-------+
+---------------A-------------a-+
		+---------------B-----b---------+
				+-------------c-C---------------+
						+-----d---------D---------------+

The value of S is: samples[A]*sinc[a-A] + samples[B]*sinc[b-B] + samples[C]*sinc[c-C] + samples[D]*sinc[d-D]
where sample[X] is the value of the sample at time X, and sinc[y] is the windowed sinc value at position y, which can
range from -window_size/2 to window_size/2. Notice that for a given offset S-floor(S), the list of sinc values required
are always the same. Therefore, we precompute the required sinc values for N possible sample fractions, then interpolate
between them.
*/

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_FETCH_SAMPLE_H__
