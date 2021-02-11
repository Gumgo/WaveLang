#include "engine/sample_format.h"

static constexpr size_t k_sample_format_sizes[] = {
	4	// e_sample_format::k_float32
};
STATIC_ASSERT(is_enum_fully_mapped<e_sample_format>(k_sample_format_sizes));

size_t get_sample_format_size(e_sample_format sample_format) {
	wl_assert(valid_enum_index(sample_format));
	return k_sample_format_sizes[enum_index(sample_format)];
}