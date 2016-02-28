#ifndef WAVELANG_BUFFER_ALLOCATOR_H__
#define WAVELANG_BUFFER_ALLOCATOR_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/lock_free_pool.h"
#include "engine/buffer.h"

struct s_buffer_allocator_settings {
	e_buffer_type buffer_type;	// Type of each buffer
	size_t buffer_size;			// Number of element in each buffer
	size_t buffer_count;		// Number of buffers to be allocated
};

class c_buffer_allocator {
public:
	c_buffer_allocator();

	void initialize(const s_buffer_allocator_settings &settings);
	const s_buffer_allocator_settings &get_settings() const;

	uint32 allocate_buffer();
	void free_buffer(uint32 buffer_handle);

	inline c_buffer *get_buffer(uint32 buffer_handle);
	inline const c_buffer *get_buffer(uint32 buffer_handle) const;

private:
	s_buffer_allocator_settings m_settings;

	// Pool of buffers to allocate from
	c_lock_free_pool m_buffer_pool;
	c_lock_free_aligned_allocator<s_aligned_lock_free_handle> m_buffer_pool_free_list_memory;

	// The buffer backing memory
	size_t m_aligned_padded_buffer_size;
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