#pragma once

#include "common/common.h"

#include "engine/sample/sample.h"

// Calculates the interpolated sample at the given time. The input sample should not be a mipmap.
real32 fetch_sample(const c_sample *sample, real64 sample_index);

// Calculates the interpolated bandlimited sample at the given time using the wavetable
real32 fetch_wavetable_sample(
	const c_sample *sample,
	real32 stream_sample_rate,
	real32 base_sample_rate,
	real32 speed,
	real64 sample_index);
