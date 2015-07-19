#include "engine/buffer_allocator.h"
#include "engine/buffer_operations/sse.h"

static_assert(CACHE_LINE_SIZE >= SSE_ALIGNMENT, "Cache line too small for SSE");

c_buffer_allocator::c_buffer_allocator() {
}

void c_buffer_allocator::initialize(const s_buffer_allocator_settings &settings) {
	m_settings = settings;

	// Initialize the pool
	m_buffer_pool_free_list_memory.free();
	m_buffer_pool_free_list_memory.allocate(settings.buffer_count);
	m_buffer_pool.initialize(m_buffer_pool_free_list_memory.get_array());

	// Calculate the total amount of buffer backing memory we need

	// Determine the size of each individual element
	size_t element_size = 0;
	switch (settings.buffer_type) {
	case k_buffer_type_real:
		element_size = 4;
		break;

	default:
		wl_unreachable();
	}

	// Round the number of elements up to a multiple of 4. This is because SSE instructions work on groups of 4.
	size_t element_count = align_size(settings.buffer_size, k_sse_block_size);

	// Calculate the size of a single buffer, which should be both cache aligned, for threading, and 16-byte aligned,
	// for SSE
	m_aligned_padded_buffer_size = align_size(element_size * element_count, CACHE_LINE_SIZE);

	// All bytes for all buffers
	size_t total_bytes = m_aligned_padded_buffer_size * settings.buffer_count;

	m_buffer_memory.free();
	m_buffer_memory.allocate(total_bytes);

	m_buffers.free();
	m_buffers.allocate(settings.buffer_count);

	// Initialize each buffer
	for (size_t index = 0; index < settings.buffer_count; index++) {
		// We don't initialize buffers so it cannot be assumed to be constant
		c_buffer &buffer = m_buffers.get_array()[index];
		buffer.m_constant = false;

		// Calculate the memory offset for this buffer
		c_wrapped_array<uint8> buffer_memory_array = m_buffer_memory.get_array();
		size_t start_offset = index * m_aligned_padded_buffer_size;
		buffer.m_data = buffer_memory_array.get_pointer() + start_offset;
		wl_assert(buffer.m_data <=
			buffer_memory_array.get_pointer() + (buffer_memory_array.get_count() - m_aligned_padded_buffer_size));
	}
}

const s_buffer_allocator_settings &c_buffer_allocator::get_settings() const {
	return m_settings;
}

uint32 c_buffer_allocator::allocate_buffer() {
	uint32 buffer_handle = m_buffer_pool.allocate();
	wl_vassert(buffer_handle != k_lock_free_invalid_handle, "Out of buffers");
	return buffer_handle;
}

void c_buffer_allocator::free_buffer(uint32 buffer_handle) {
	m_buffer_pool.free(buffer_handle);
}

c_buffer *c_buffer_allocator::get_buffer(uint32 buffer_handle) {
	return &m_buffers.get_array()[buffer_handle];
}
