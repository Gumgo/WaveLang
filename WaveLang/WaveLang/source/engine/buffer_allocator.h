#ifndef WAVELANG_BUFFER_ALLOCATOR_H__
#define WAVELANG_BUFFER_ALLOCATOR_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/lock_free_pool.h"

// $TODO bool buffers?
enum e_buffer_type {
	k_buffer_type_real,

	k_buffer_type_count
};

struct s_buffer_allocator_settings {
	e_buffer_type buffer_type;	// Type of each buffer
	size_t buffer_size;			// Number of element in each buffer
	size_t buffer_count;		// Number of buffers to be allocated
};

ALIGNAS_LOCK_FREE struct c_buffer : private c_uncopyable {
public:
	bool is_constant() const {
		return m_constant;
	}

	void set_constant(bool constant) {
		m_constant = constant;
	}

	void *get_data() {
		return m_data;
	}

	const void *get_data() const {
		return m_data;
	}

private:
	c_buffer() {}

	friend class c_buffer_allocator;

	void *m_data;		// Pointer to the data
	bool m_constant;	// Whether this buffer consists of a single value across all elements
};

class c_buffer_allocator {
public:
	c_buffer_allocator();

	void initialize(const s_buffer_allocator_settings &settings);
	const s_buffer_allocator_settings &get_settings() const;

	uint32 allocate_buffer();
	void free_buffer(uint32 buffer_handle);

	c_buffer *get_buffer(uint32 buffer_handle);

private:
	s_buffer_allocator_settings m_settings;

	// Pool of buffers to allocate from
	c_lock_free_pool m_buffer_pool;
	c_lock_free_aligned_array_allocator<s_aligned_lock_free_handle> m_buffer_pool_free_list_memory;

	// The buffer backing memory
	size_t m_aligned_padded_buffer_size;
	c_lock_free_aligned_array_allocator<uint8> m_buffer_memory;

	// The buffers themselves
	c_lock_free_aligned_array_allocator<c_buffer> m_buffers;
};

#endif // WAVELANG_BUFFER_ALLOCATOR_H__