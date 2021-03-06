#pragma once

#include "common/common.h"
#include "common/utility/aligned_allocator.h"

#include <atomic>

// Data structures containing a lock-free handle should be cache line aligned
#define LOCK_FREE_ALIGNMENT CACHE_LINE_SIZE
#define ALIGNAS_LOCK_FREE alignas(LOCK_FREE_ALIGNMENT)

// More type-friendly version:
static constexpr size_t k_lock_free_alignment = LOCK_FREE_ALIGNMENT;

// Can't do partial-template typedefs
template<typename t_element>
class c_lock_free_aligned_allocator : public c_aligned_allocator<t_element, k_lock_free_alignment> {};

// $TODO replace this with c_handle
static constexpr uint32 k_lock_free_invalid_handle = static_cast<uint32>(-1);

// Uses 32-bit handles for indexing nodes and 32-bit ABA counter
struct s_lock_free_handle {
	std::atomic<int64> handle;
};

// Aligned version to avoid false sharing
struct ALIGNAS_LOCK_FREE s_aligned_lock_free_handle : public s_lock_free_handle {};

// A list of handles pointing to the next node in the list
using c_lock_free_handle_array = c_wrapped_array<s_aligned_lock_free_handle>;

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
#if IS_TRUE(ASSERTS_ENABLED)
// Not thread safe - use only for asserts
uint32 lock_free_list_count_unsafe(const c_lock_free_handle_array &node_storage, const s_lock_free_handle &list_head);
#endif // IS_TRUE(ASSERTS_ENABLED)

