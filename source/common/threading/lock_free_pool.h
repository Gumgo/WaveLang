#ifndef WAVELANG_COMMON_THREADING_LOCK_FREE_POOL_H__
#define WAVELANG_COMMON_THREADING_LOCK_FREE_POOL_H__

#include "common/common.h"
#include "common/threading/lock_free.h"

class c_lock_free_pool {
public:
	// Non-thread-safe functions:

	c_lock_free_pool();

	// Initializes the pool. The user must provide and manage the backing memory for the free-list, whose capacity is
	// determines the pool size.
	void initialize(c_lock_free_handle_array free_list_memory);

	// Thread-safe functions:

	// Allocates a node and returns its handle, or returns k_lock_free_invalid_handle if the pool is empty
	uint32 allocate();

	// Frees the given node, allowing it to be reused
	void free(uint32 handle);

#if IS_TRUE(ASSERTS_ENABLED)
	// Calculates the amount of elements used. Not thread safe! Currently this is only used for asserts.
	uint32 calculate_used_count_unsafe() const;
#endif // IS_TRUE(ASSERTS_ENABLED)

private:
	// Free-list is a linked stack. To free an element, push to the top. To allocate, pop from the top.
	// Initially all elements are on the free-list.
	s_aligned_lock_free_handle m_free_list_head;
	c_lock_free_handle_array m_free_list;
};

#endif // WAVELANG_COMMON_THREADING_LOCK_FREE_POOL_H__
