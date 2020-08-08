#include "common/threading/lock_free_pool.h"

// Lock-free handle lists are aligned to the cache line size which is larger than the handles themselves, so we can use
// the space in-between the handles for extra validation. When a node is allocated, we will write a '1' after it, and
// when it is freed we will write a '0'.
#if IS_TRUE(ASSERTS_ENABLED)
#define ALLOCATION_VERIFICATION_ENABLED 1
static_assert(sizeof(s_aligned_lock_free_handle) > sizeof(s_lock_free_handle), "Not enough space for verification");
#else // IS_TRUE(ASSERTS_ENABLED)
#define ALLOCATION_VERIFICATION_ENABLED 0
#endif // IS_TRUE(ASSERTS_ENABLED)

c_lock_free_pool::c_lock_free_pool()
	: m_free_list(nullptr, 0) {
	s_lock_free_handle_data head;
	head.handle = k_lock_free_invalid_handle;
	head.tag = 0;
	m_free_list_head.handle = head.data;
}

c_lock_free_pool::c_lock_free_pool(c_lock_free_pool &&other)
	: m_free_list(nullptr, 0) {
	m_free_list_head.handle = other.m_free_list_head.handle.load();

	s_lock_free_handle_data head;
	head.handle = k_lock_free_invalid_handle;
	head.tag = 0;
	other.m_free_list_head.handle = head.data;

	std::swap(m_free_list, other.m_free_list);
}

void c_lock_free_pool::initialize(c_lock_free_handle_array free_list_memory) {
	if (free_list_memory.get_count() == 0) {
		s_lock_free_handle_data head;
		head.handle = k_lock_free_invalid_handle;
		head.tag = 0;
		m_free_list_head.handle = head.data;
		m_free_list = c_lock_free_handle_array(nullptr, 0);
		return;
	}

#if IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)
	// If we are verifying allocations, clear all the "used" bits to 0 initially.
	memset(free_list_memory.get_pointer(), 0, sizeof(s_aligned_lock_free_handle) * free_list_memory.get_count());
#endif // IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)

	m_free_list = free_list_memory;
	for (uint32 index = 0; index < m_free_list.get_count() - 1; index++) {
		s_lock_free_handle_data node;
		node.handle = index + 1;
		node.tag = 0;
		m_free_list[index].handle = node.data;
	}

	s_lock_free_handle_data last_node;
	last_node.handle = k_lock_free_invalid_handle;
	last_node.tag = 0;
	m_free_list[m_free_list.get_count() - 1].handle = last_node.data;

	// Head points to node 0
	s_lock_free_handle_data head;
	head.handle = 0;
	head.tag = 0;
	m_free_list_head.handle = head.data;
}

uint32 c_lock_free_pool::allocate() {
	uint32 handle = lock_free_list_pop(m_free_list, m_free_list_head);

#if IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)
	// This thread now owns this node, so we can safely set the bit without atomics
	uint8 *handle_ptr = reinterpret_cast<uint8 *>(&m_free_list[handle]);
	// Offset by the non-aligned size - there is some extra space behind the handle which we can use
	uint8 *verification_ptr = handle_ptr + sizeof(s_lock_free_handle);
	// Verify that this node was previously free, and mark it as used
	wl_vassert(*verification_ptr == 0, "Attempted to allocate an already-allocated node");
	*verification_ptr = 1;
#endif // IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)

	return handle;
}

void c_lock_free_pool::free(uint32 handle) {
#if IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)
	// This thread still owns this node, so we can safely set the bit without atomics
	uint8 *handle_ptr = reinterpret_cast<uint8 *>(&m_free_list[handle]);
	// Offset by the non-aligned size - there is some extra space behind the handle which we can use
	uint8 *verification_ptr = handle_ptr + sizeof(s_lock_free_handle);
	// Verify that this node was previously used, and mark it as free
	wl_vassert(*verification_ptr == 1, "Attempted to free an already-free node");
	*verification_ptr = 0;
#endif // IS_TRUE(ALLOCATION_VERIFICATION_ENABLED)

	lock_free_list_push(m_free_list, m_free_list_head, handle);
}

#if IS_TRUE(ASSERTS_ENABLED)
uint32 c_lock_free_pool::calculate_used_count_unsafe() const {
	return static_cast<uint32>(m_free_list.get_count() - lock_free_list_count_unsafe(m_free_list, m_free_list_head));
}
#endif // IS_TRUE(ASSERTS_ENABLED)
