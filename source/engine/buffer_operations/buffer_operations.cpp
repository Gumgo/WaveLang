#include "engine/buffer_operations/buffer_operations.h"

#if PREDEFINED(ASSERTS_ENABLED)
void validate_buffer(const c_buffer *buffer) {
	wl_assert(buffer);
	wl_assert(is_pointer_aligned(buffer->get_data(), k_sse_alignment));
}

void validate_buffer(const c_real_buffer_or_constant_in &buffer_or_constant) {
	if (buffer_or_constant.is_buffer()) {
		validate_buffer(buffer_or_constant.get_buffer());
	}
}
#endif // PREDEFINED(ASSERTS_ENABLED)
