#include "instrument/native_modules/resampler/resampler.h"

static constexpr real32 k_resampler_quality_low = 0.0f;
static constexpr real32 k_resampler_quality_high = 1.0f;

e_resampler_filter get_resampler_filter_from_quality(bool is_upsampling, uint32 resample_factor, real32 quality) {
	if (is_upsampling) {
		if (quality == k_resampler_quality_low) {
			return e_resampler_filter::k_upsample_low_quality;
		} else if (quality == k_resampler_quality_high) {
			return e_resampler_filter::k_upsample_high_quality;
		}
	} else {
		switch (resample_factor) {
		case 2:
			if (quality == k_resampler_quality_low) {
				return e_resampler_filter::k_downsample_2x_low_quality;
			} else if (quality == k_resampler_quality_high) {
				return e_resampler_filter::k_downsample_2x_high_quality;
			}
			break;

		case 3:
			if (quality == k_resampler_quality_low) {
				return e_resampler_filter::k_downsample_3x_low_quality;
			} else if (quality == k_resampler_quality_high) {
				return e_resampler_filter::k_downsample_3x_high_quality;
			}
			break;

		case 4:
			if (quality == k_resampler_quality_low) {
				return e_resampler_filter::k_downsample_4x_low_quality;
			} else if (quality == k_resampler_quality_high) {
				return e_resampler_filter::k_downsample_4x_high_quality;
			}
			break;

		default:
			wl_halt();
		}
	}

	return e_resampler_filter::k_invalid;
}
