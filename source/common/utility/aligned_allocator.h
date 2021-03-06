#pragma once

#include "common/common.h"

#include <memory>

// $TODO consider adding c_aligned<t_type> or using std::aligned_storage so that the usual new/delete and
// std::unique_ptr will work
template<typename t_element, size_t k_alignment>
class c_aligned_allocator {
public:
	// Allocates memory aligned to k_alignment. aligned_pointer_out is the resulting aligned pointer, but
	// base_pointer_out is the pointer which should be later passed into free_aligned_memory.
	static bool allocate_aligned_memory(size_t size, void **base_pointer_out, void **aligned_pointer_out) {
		wl_assert(base_pointer_out);
		wl_assert(aligned_pointer_out);

		size_t aligned_size = size + k_alignment - 1;
		*base_pointer_out = std::malloc(aligned_size);
		if (!(*base_pointer_out)) {
			return false;
		}

		*aligned_pointer_out = align_pointer(*base_pointer_out, k_alignment);
		return true;
	}

	// base_pointer should be out_base_pointer from allocate_aligned_memory
	static void free_aligned_memory(void *base_pointer) {
		if (base_pointer) {
			std::free(base_pointer);
		}
	}

	c_aligned_allocator()
		: m_array(nullptr, 0) {
		m_base_pointer = nullptr;
	}

	~c_aligned_allocator() {
		free_memory();
	}

	void swap(c_aligned_allocator<t_element, k_alignment> &other) {
		std::swap(m_base_pointer, other.m_base_pointer);
		std::swap(m_array, other.m_array);
	}

	bool allocate(size_t count) {
		wl_assert(!m_base_pointer);

		void *aligned_pointer;
		if (!allocate_aligned_memory(sizeof(t_element) * count, &m_base_pointer, &aligned_pointer)) {
			return false;
		}

		m_array = c_wrapped_array<t_element>(static_cast<t_element *>(aligned_pointer), count);
		return true;
	}

	// free() gets defined in debug builds so use free_memory() instead
	void free_memory() {
		if (m_base_pointer) {
			wl_assert(m_array.get_pointer());
			free_aligned_memory(m_base_pointer);
			m_base_pointer = nullptr;
			m_array = c_wrapped_array<t_element>(nullptr, 0);
		}
	}

	c_wrapped_array<t_element> get_array() {
		return m_array;
	}

	c_wrapped_array<const t_element> get_array() const {
		return m_array;
	}

private:
	void *m_base_pointer;
	c_wrapped_array<t_element> m_array;
};

