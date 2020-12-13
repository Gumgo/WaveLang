#include "engine/executor/buffer_allocator.h"

STATIC_ASSERT_MSG(CACHE_LINE_SIZE >= k_simd_alignment, "Cache line too small for SSE");

static constexpr size_t k_bits_per_buffer_element[] = {
	32,	// e_buffer_type::k_real
	1	// e_buffer_type::k_bool
};
STATIC_ASSERT(is_enum_fully_mapped<e_buffer_type>(k_bits_per_buffer_element));

c_buffer_allocator::c_buffer_allocator() {}

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

	m_buffer_memory_pointers.free();
	m_buffer_memory_pointers.allocate(total_buffer_count);

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
			// Setup the header
			s_buffer_memory_header *header =
				reinterpret_cast<s_buffer_memory_header *>(&buffer_memory_array[buffer_memory_offset]);
			header->pool_index = pool_index;
			header->pool_buffer_index = pool_buffer_index;

			uint32 buffer_index = pool.first_buffer_handle + pool_buffer_index;
			m_buffer_memory_pointers.get_array()[buffer_index] = header;

			buffer_memory_offset += aligned_padded_buffer_size;
			wl_assert(buffer_memory_offset <= buffer_memory_array.get_count());
		}
	}
}

void c_buffer_allocator::shutdown() {
	m_buffer_pools.clear();
	m_buffer_pool_free_list_memory.free();
	m_buffer_memory.free();
	m_buffer_memory_pointers.free();
}

uint32 c_buffer_allocator::get_buffer_pool_count() const {
	return cast_integer_verify<uint32>(m_buffer_pools.size());
}

const s_buffer_pool_description &c_buffer_allocator::get_buffer_pool_description(uint32 pool_index) const {
	return m_buffer_pools[pool_index].description;
}

void *c_buffer_allocator::allocate_buffer_memory(uint32 pool_index) {
	s_buffer_pool &pool = m_buffer_pools[pool_index];
	uint32 buffer_handle = pool.buffer_pool.allocate();
	wl_vassert(buffer_handle != k_lock_free_invalid_handle, "Out of buffers");
	s_buffer_memory_header *header = m_buffer_memory_pointers.get_array()[pool.first_buffer_handle + buffer_handle];
	return header + 1; // Buffer memory is directly after the header
}

void c_buffer_allocator::free_buffer_memory(void *buffer_memory) {
	wl_assert(buffer_memory >= m_buffer_memory.get_array().begin()
		&& buffer_memory < m_buffer_memory.get_array().end());
	s_buffer_memory_header *header = reinterpret_cast<s_buffer_memory_header *>(buffer_memory) - 1;
	s_buffer_pool &pool = m_buffer_pools[header->pool_index];

	// Make sure the buffer handle is in the pool's handle range
	wl_assert(header->pool_buffer_index < pool.description.count);
	pool.buffer_pool.free(header->pool_buffer_index);
}

#if IS_TRUE(ASSERTS_ENABLED)
void c_buffer_allocator::assert_no_allocations() const {
	for (uint32 pool_index = 0; pool_index < m_buffer_pools.size(); pool_index++) {
		const s_buffer_pool &pool = m_buffer_pools[pool_index];
		wl_assert(pool.buffer_pool.calculate_used_count_unsafe() == 0);
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)

size_t c_buffer_allocator::calculate_aligned_padded_buffer_size(e_buffer_type type, size_t element_count) {
	size_t header_size_bytes = sizeof(s_buffer_memory_header);
	wl_assert(is_size_aligned(header_size_bytes, k_simd_alignment));

	size_t buffer_size_bits = k_bits_per_buffer_element[enum_index(type)] * element_count;
	size_t buffer_size_bytes = align_size((buffer_size_bits + 7) / 8, k_simd_alignment);

	// Accounts for both SSE alignment and cache alignment
	return align_size(header_size_bytes + buffer_size_bytes, CACHE_LINE_SIZE);
}
