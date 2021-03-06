#pragma once

#include "common/common.h"
#include "common/math/simd.h"
#include "common/threading/lock_free.h"
#include "common/threading/lock_free_pool.h"

#include "engine/buffer.h"

#include <vector>

enum class e_buffer_type {
	k_real,
	k_bool,

	k_count
};

struct s_buffer_pool_description {
	e_buffer_type type;		// Type of buffer
	size_t size;			// Number of elements before upsampling
	uint32 upsample_factor;	// Upsample factor for element count
	size_t count;			// Number of buffers to allocate in this pool
};

struct s_buffer_allocator_settings {
	// List of buffer pools to allocate. The pools are indexed by the element order provided here.
	c_wrapped_array<const s_buffer_pool_description> buffer_pool_descriptions;
};

class c_buffer_allocator {
public:
	c_buffer_allocator();

	void initialize(const s_buffer_allocator_settings &settings);
	void shutdown();

	uint32 get_buffer_pool_count() const;
	const s_buffer_pool_description &get_buffer_pool_description(uint32 pool_index) const;

	void *allocate_buffer_memory(uint32 pool_index);
	void free_buffer_memory(void *buffer_memory);

#if IS_TRUE(ASSERTS_ENABLED)
	void assert_no_allocations() const;
#endif // IS_TRUE(ASSERTS_ENABLED)

private:
	struct s_buffer_pool {
		// Pool settings
		s_buffer_pool_description description;

		// The starting handle for buffers in this pool - each pool accounts for a set range of the total buffer handles
		uint32 first_buffer_handle;

		// Pool of buffers to allocate from. Allocations will return handles in the range [0, description.count-1], they
		// are transformed to "global" buffer handles by adding first_buffer_handle.
		c_lock_free_pool buffer_pool;
	};

	// This structure appears directly before each buffer memory allocation
	struct ALIGNAS_SIMD s_buffer_memory_header {
		uint32 pool_index;
		uint32 pool_buffer_index;
	};

	static size_t calculate_aligned_padded_buffer_size(
		e_buffer_type type,
		size_t element_count,
		uint32 upsample_factor);

	// List of buffer pools
	std::vector<s_buffer_pool> m_buffer_pools;

	// Memory backing the free lists for all buffer pools
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_buffer_pool_free_list_memory;

	// The buffer backing memory
	c_lock_free_aligned_allocator<uint8> m_buffer_memory;

	// The pointers associated with each buffer handle
	c_lock_free_aligned_allocator<s_buffer_memory_header *> m_buffer_memory_pointers;
};
