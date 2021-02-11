#pragma once

#include "common/common.h"

// $TODO add more format options
enum class e_sample_format {
	k_float32,

	k_count
};

size_t get_sample_format_size(e_sample_format sample_format);

