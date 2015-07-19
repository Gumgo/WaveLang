#include "engine/buffer_operations/buffer_operations.h"

#if PREDEFINED(ASSERTS_ENABLED)
void validate_buffer(const c_buffer *buffer) {
	wl_assert(buffer);
	wl_assert(is_pointer_aligned(buffer->get_data(), k_sse_alignment));
}
#endif // PREDEFINED(ASSERTS_ENABLED)
