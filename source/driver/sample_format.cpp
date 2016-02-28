#include "driver/sample_format.h"

static const size_t k_sample_format_sizes[] = {
	4	// k_sample_format_float32
};
static_assert(NUMBEROF(k_sample_format_sizes) == k_sample_format_count, "Sample format size mismatch");

size_t get_sample_format_size(e_sample_format sample_format) {
	wl_assert(VALID_INDEX(sample_format, k_sample_format_count));
	return k_sample_format_sizes[sample_format];
}