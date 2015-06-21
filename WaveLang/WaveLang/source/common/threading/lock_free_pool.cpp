#include "common/threading/lock_free_pool.h"

c_lock_free_pool::c_lock_free_pool()
	: m_free_list(nullptr, 0) {
	s_lock_free_handle_data head;
	head.handle = k_lock_free_invalid_handle;
	head.tag = 0;
	m_free_list_head.handle.initialize(head.data);
}

void c_lock_free_pool::initialize(c_lock_free_handle_array free_list_memory) {
	if (free_list_memory.get_count() == 0) {
		s_lock_free_handle_data head;
		head.handle = k_lock_free_invalid_handle;
		head.tag = 0;
		m_free_list_head.handle.initialize(head.data);
		m_free_list = c_lock_free_handle_array(nullptr, 0);
		return;
	}

	m_free_list = free_list_memory;
	for (uint32 index = 0; index < m_free_list.get_count() - 1; index++) {
		s_lock_free_handle_data node;
		node.handle = index + 1;
		node.tag = 0;
		m_free_list[index].handle.initialize(node.data);
	}

	s_lock_free_handle_data last_node;
	last_node.handle = k_lock_free_invalid_handle;
	last_node.tag = 0;
	m_free_list[m_free_list.get_count() - 1].handle.initialize(last_node.data);

	// Head points to node 0
	s_lock_free_handle_data head;
	head.handle = 0;
	head.tag = 0;
	m_free_list_head.handle.initialize(head.data);
}

uint32 c_lock_free_pool::allocate() {
	return lock_free_list_pop(m_free_list, m_free_list_head);
}

void c_lock_free_pool::free(uint32 handle) {
	lock_free_list_push(m_free_list, m_free_list_head, handle);
}
