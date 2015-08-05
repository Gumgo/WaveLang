#ifndef WAVELANG_LOCK_FREE_H__
#define WAVELANG_LOCK_FREE_H__

#include "common/common.h"
#include "common/threading/atomics.h"

// Data structures containing a lock-free handle should be cache line aligned
#define LOCK_FREE_ALIGNMENT CACHE_LINE_SIZE
#define ALIGNAS_LOCK_FREE alignas(LOCK_FREE_ALIGNMENT)

// More type-friendly version:
static const size_t k_lock_free_alignment = LOCK_FREE_ALIGNMENT;

// Allocates memory aligned to LOCK_FREE_ALIGNMENT. out_aligned_pointer is the resulting aligned pointer, but
// out_base_pointer is the pointer which should be later passed into free_lock_free_aligned_memory.
bool allocate_lock_free_aligned_memory(size_t size, void **out_base_pointer, void **out_aligned_pointer);
void free_lock_free_aligned_memory(void *base_pointer);

template<typename t_element>
class c_lock_free_aligned_array_allocator {
public:
	c_lock_free_aligned_array_allocator()
		: m_array(nullptr, 0) {
		m_base_pointer = nullptr;
	}

	~c_lock_free_aligned_array_allocator() {
		free();
	}

	bool allocate(size_t count) {
		wl_assert(!m_base_pointer);

		void *aligned_pointer;
		if (!allocate_lock_free_aligned_memory(sizeof(t_element) * count, &m_base_pointer, &aligned_pointer)) {
			return false;
		}

		m_array = c_wrapped_array<t_element>(static_cast<t_element *>(aligned_pointer), count);
		return true;
	}

	void free() {
		if (m_base_pointer) {
			wl_assert(m_array.get_pointer());
			free_lock_free_aligned_memory(m_base_pointer);
			m_base_pointer = nullptr;
			m_array = c_wrapped_array<t_element>(nullptr, 0);
		}
	}

	c_wrapped_array<t_element> get_array() {
		return m_array;
	}

	c_wrapped_array_const<t_element> get_array() const {
		return m_array;
	}

private:
	void *m_base_pointer;
	c_wrapped_array<t_element> m_array;
};

static const uint32 k_lock_free_invalid_handle = static_cast<uint32>(-1);

// Uses 32-bit handles for indexing nodes and 32-bit ABA counter
struct s_lock_free_handle {
	c_atomic_int64 handle;
};

// Aligned version to avoid false sharing
struct ALIGNAS_LOCK_FREE s_aligned_lock_free_handle : public s_lock_free_handle {};

// A list of handles pointing to the next node in the list
typedef c_wrapped_array<s_aligned_lock_free_handle> c_lock_free_handle_array;

// Used to modify lock free handles
struct s_lock_free_handle_data {
	union {
		int64 data;

		struct {
			uint32 tag;
			uint32 handle;
		};
	};
};

// Functions to push/pop from list head

void lock_free_list_push(c_lock_free_handle_array &node_storage, s_lock_free_handle &list_head, uint32 handle);
uint32 lock_free_list_pop(c_lock_free_handle_array &node_storage, s_lock_free_handle &list_head);

#endif // WAVELANG_LOCK_FREE_H__
