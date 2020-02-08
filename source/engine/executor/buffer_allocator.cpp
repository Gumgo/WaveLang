#include "engine/executor/buffer_allocator.h"
#include "engine/math/simd.h"

static_assert(CACHE_LINE_SIZE >= SIMD_ALIGNMENT, "Cache line too small for SSE");

static const size_t k_bits_per_buffer_element[] = {
	32,	// k_buffer_type_real
	1	// k_buffer_type_bool
};
static_assert(NUMBEROF(k_bits_per_buffer_element) == k_buffer_type_count, "Buffer type bits mismatch");

static size_t calculate_aligned_padded_buffer_size(e_buffer_type type, size_t element_count) {
	size_t buffer_size_bits = k_bits_per_buffer_element[type] * element_count;
	size_t buffer_size_bytes = (buffer_size_bits + 7) / 8;
	// Accounts for both SSE alignment and cache alignment
	return align_size(buffer_size_bytes, CACHE_LINE_SIZE);
}

c_buffer_allocator::c_buffer_allocator() {
}

void c_buffer_allocator::initialize(const s_buffer_allocator_settings &settings) {
	size_t total_buffer_count = 0;
	size_t total_buffer_bytes = 0;
	m_buffer_pools.clear();
	m_buffer_pools.resize(settings.buffer_pool_descriptions.get_count());

	for (uint32 pool_index = 0; pool_index < settings.buffer_pool_descriptions.get_count(); pool_index++) {
		s_buffer_pool &pool = m_buffer_pools[pool_index];
		pool.description = settings.buffer_pool_descriptions[pool_index];
		pool.first_buffer_handle = cast_integer_verify<uint32>(total_buffer_count);
		total_buffer_count += pool.description.count;

		// Calculate the total amount of buffer backing memory we need
		size_t aligned_padded_buffer_size =
			calculate_aligned_padded_buffer_size(pool.description.type, pool.description.size);
		total_buffer_bytes += aligned_padded_buffer_size * pool.description.count;
	}

	// Initialize the pool
	m_buffer_pool_free_list_memory.free();
	m_buffer_pool_free_list_memory.allocate(total_buffer_count);

	m_buffer_memory.free();
	m_buffer_memory.allocate(total_buffer_bytes);

	m_buffers.free();
	m_buffers.allocate(total_buffer_count);

	// Initialize each buffer
	size_t buffer_memory_offset = 0;
	c_wrapped_array<uint8> buffer_memory_array = m_buffer_memory.get_array();
	for (uint32 pool_index = 0; pool_index < settings.buffer_pool_descriptions.get_count(); pool_index++) {
		s_buffer_pool &pool = m_buffer_pools[pool_index];
		pool.buffer_pool.initialize(c_lock_free_handle_array(
			&m_buffer_pool_free_list_memory.get_array()[pool.first_buffer_handle],
			pool.description.count));

		size_t aligned_padded_buffer_size =
			calculate_aligned_padded_buffer_size(pool.description.type, pool.description.size);

		for (uint32 pool_buffer_index = 0; pool_buffer_index < pool.description.count; pool_buffer_index++) {
			uint32 buffer_index = pool.first_buffer_handle + pool_buffer_index;
			c_buffer &buffer = m_buffers.get_array()[buffer_index];

			// We don't initialize buffers so it cannot be assumed to be constant
			buffer.m_constant = false;
			buffer.m_data = &buffer_memory_array[buffer_memory_offset];
			buffer.m_pool_index = pool_index;

			buffer_memory_offset += aligned_padded_buffer_size;
			wl_assert(buffer_memory_offset <= buffer_memory_array.get_count());
		}
	}
}

uint32 c_buffer_allocator::get_buffer_pool_count() const {
	return cast_integer_verify<uint32>(m_buffer_pools.size());
}

const s_buffer_pool_description &c_buffer_allocator::get_buffer_pool_description(uint32 pool_index) const {
	return m_buffer_pools[pool_index].description;
}

h_allocated_buffer c_buffer_allocator::allocate_buffer(uint32 pool_index) {
	s_buffer_pool &pool = m_buffer_pools[pool_index];
	uint32 buffer_handle = pool.buffer_pool.allocate();
	wl_vassert(buffer_handle != k_lock_free_invalid_handle, "Out of buffers");
	return h_allocated_buffer::construct(pool.first_buffer_handle + buffer_handle);
}

void c_buffer_allocator::free_buffer(h_allocated_buffer buffer_handle) {
	wl_assert(buffer_handle.is_valid());
	uint32 pool_index = m_buffers.get_array()[buffer_handle.get_data()].m_pool_index;
	s_buffer_pool &pool = m_buffer_pools[pool_index];

	// Make sure the buffer handle is in the pool's handle range
	wl_assert(buffer_handle.get_data() >= pool.first_buffer_handle &&
		buffer_handle.get_data() < pool.first_buffer_handle + pool.description.count);
	pool.buffer_pool.free(buffer_handle.get_data() - pool.first_buffer_handle);
}

#if IS_TRUE(ASSERTS_ENABLED)
void c_buffer_allocator::assert_all_buffers_free() const {
	for (uint32 pool_index = 0; pool_index < m_buffer_pools.size(); pool_index++) {
		const s_buffer_pool &pool = m_buffer_pools[pool_index];
		wl_assert(pool.buffer_pool.calculate_used_count_unsafe() == 0);
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)
