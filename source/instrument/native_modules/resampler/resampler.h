#pragma once

#include "common/common.h"

#include "instrument/resampler/resampler.h"

e_resampler_filter get_resampler_filter_from_quality(bool is_upsampling, uint32 resample_factor, real32 quality);
