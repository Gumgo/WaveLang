#include "driver/sample_format.h"

static const size_t k_sample_format_sizes[] = {
	4	// e_sample_format::k_float32
};
static_assert(NUMBEROF(k_sample_format_sizes) == enum_count<e_sample_format>(), "Sample format size mismatch");

size_t get_sample_format_size(e_sample_format sample_format) {
	wl_assert(valid_enum_index(sample_format));
	return k_sample_format_sizes[enum_index(sample_format)];
}