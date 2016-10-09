#ifndef WAVELANG_BUFFER_ALLOCATOR_H__
#define WAVELANG_BUFFER_ALLOCATOR_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/lock_free_pool.h"

#include "engine/buffer.h"

#include <vector>

struct s_buffer_pool_description {
	e_buffer_type type;	// Type of buffer
	size_t size;		// Number of elements
	size_t count;		// Number of buffers to allocate in this pool
};

struct s_buffer_allocator_settings {
	// List of buffer pools to allocate. The pools are indexed by the element order provided here.
	c_wrapped_array_const<s_buffer_pool_description> buffer_pool_descriptions;
};

class c_buffer_allocator {
public:
	c_buffer_allocator();

	void initialize(const s_buffer_allocator_settings &settings);
	uint32 get_buffer_pool_count() const;
	const s_buffer_pool_description &get_buffer_pool_description(uint32 pool_index) const;

	uint32 allocate_buffer(uint32 pool_index);
	void free_buffer(uint32 buffer_handle);

	inline c_buffer *get_buffer(uint32 buffer_handle);
	inline const c_buffer *get_buffer(uint32 buffer_handle) const;

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

	// List of buffer pools
	std::vector<s_buffer_pool> m_buffer_pools;

	// Memory backing the free lists for all buffer pools
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_buffer_pool_free_list_memory;

	// The buffer backing memory
	c_lock_free_aligned_allocator<uint8> m_buffer_memory;

	// The buffers themselves
	c_lock_free_aligned_allocator<c_buffer> m_buffers;
};

inline c_buffer *c_buffer_allocator::get_buffer(uint32 buffer_handle) {
	return &m_buffers.get_array()[buffer_handle];
}

inline const c_buffer *c_buffer_allocator::get_buffer(uint32 buffer_handle) const {
	return &m_buffers.get_array()[buffer_handle];
}

#endif // WAVELANG_BUFFER_ALLOCATOR_H__