#pragma once

#include "common/common.h"

// $TODO add more format options
enum e_sample_format {
	k_sample_format_float32,

	k_sample_format_count
};

size_t get_sample_format_size(e_sample_format sample_format);

