#pragma once

#include "common/common.h"

enum class e_resampler_filter {
	k_invalid = -1,

	k_upsample_low_quality,
	k_downsample_2x_low_quality,
	k_downsample_3x_low_quality,
	k_downsample_4x_low_quality,

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
};

const s_resampler_parameters &get_resampler_parameters(e_resampler_filter resampler_filter);
