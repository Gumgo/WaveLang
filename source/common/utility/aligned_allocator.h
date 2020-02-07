#pragma once

#include "common/common.h"

#include <memory>

template<typename t_element, size_t k_alignment>
class c_aligned_allocator {
public:
	// Allocates memory aligned to k_alignment. out_aligned_pointer is the resulting aligned pointer, but
	// out_base_pointer is the pointer which should be later passed into free_aligned_memory.
	static bool allocate_aligned_memory(size_t size, void **out_base_pointer, void **out_aligned_pointer) {
		wl_assert(out_base_pointer);
		wl_assert(out_aligned_pointer);

		size_t aligned_size = size + k_alignment - 1;
		*out_base_pointer = std::malloc(aligned_size);
		if (!(*out_base_pointer)) {
			return false;
		}

		*out_aligned_pointer = align_pointer(*out_base_pointer, k_alignment);
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
		free();
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

	void free() {
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

