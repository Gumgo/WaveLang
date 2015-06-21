#ifndef WAVELANG_LOCK_FREE_H__
#define WAVELANG_LOCK_FREE_H__

#include "common/common.h"
#include "common/threading/atomics.h"

// Data structures containing a lock-free handle should be cache line aligned
#define LOCK_FREE_ALIGNMENT CACHE_LINE_SIZE

static const uint32 k_lock_free_invalid_handle = static_cast<uint32>(-1);

// Uses 32-bit handles for indexing nodes and 32-bit ABA counter
struct s_lock_free_handle {
	c_atomic_int64 handle;
};

// Aligned version
alignas(LOCK_FREE_ALIGNMENT) struct s_aligned_lock_free_handle : public s_lock_free_handle {};

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