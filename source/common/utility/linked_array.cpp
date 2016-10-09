#include "common/utility/linked_array.h"

#define SLOW_LINKED_ARRAY_ASSERTS_ENABLED 1

void c_linked_array::initialize(size_t count) {
	m_nodes.resize(count);

	if (count == 0) {
		m_free_list_front = k_invalid_linked_array_index;
	} else {
		// Initially, all nodes are on the free list
		m_free_list_front = 0;
		for (size_t index = 0; index < count; index++) {
			// Point each node to the prev and next nodes. Nodes at the end of the list point back at themselves.
			m_nodes[index].prev = (index == 0) ? 0 : (index - 1);
			m_nodes[index].next = (index == count - 1) ? count - 1 : (index + 1);
		}
	}
}

void c_linked_array::initialize_list(s_linked_array_list &list) {
	list.back = k_invalid_linked_array_index;
	list.front = k_invalid_linked_array_index;
}

size_t c_linked_array::allocate_node() {
	// Grab a node from the front of the free list
	size_t node_index = m_free_list_front;

	if (node_index != k_invalid_linked_array_index) {
		m_free_list_front = m_nodes[node_index].next;
		if (m_free_list_front == node_index) {
			// The node pointed back at itself, so it must be the end of the free list
			m_free_list_front = k_invalid_linked_array_index;
		}

		// This node is not on any list
		s_node &node = m_nodes[node_index];
		node.prev = k_invalid_linked_array_index;
		node.next = k_invalid_linked_array_index;
	}

	return node_index;
}

void c_linked_array::free_node(size_t node_index) {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));
	s_node &node = m_nodes[node_index];

	// Make sure this node isn't on any list, including the free list
	wl_assert(node.prev == k_invalid_linked_array_index);
	wl_assert(node.next == k_invalid_linked_array_index);

	// Move the node to the free list
	node.prev = node_index;
	node.next = (m_free_list_front == k_invalid_linked_array_index) ? node_index : m_free_list_front;
	m_free_list_front = node_index;
}

void c_linked_array::insert_node_into_list(
	s_linked_array_list &list, size_t node_index, size_t insert_before_this_node_index) {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));

	s_node &node = m_nodes[node_index];
	wl_assert(!is_node_either_free_or_in_any_list(node_index));

#if IS_TRUE(SLOW_LINKED_ARRAY_ASSERTS_ENABLED)
	wl_assert(insert_before_this_node_index == k_invalid_linked_array_index ||
		is_node_in_list_slow(list, insert_before_this_node_index));
#endif // IS_TRUE(SLOW_LINKED_ARRAY_ASSERTS_ENABLED)

	size_t insert_after_this_node_index;
	if (insert_before_this_node_index == k_invalid_linked_array_index) {
		insert_after_this_node_index = list.back;
	} else {
		insert_after_this_node_index = m_nodes[insert_before_this_node_index].prev;
		if (insert_after_this_node_index == insert_before_this_node_index) {
			insert_after_this_node_index = k_invalid_linked_array_index;
		}
	}

	// Update next node to point back to this node
	if (insert_before_this_node_index == k_invalid_linked_array_index) {
		node.prev = (list.back == k_invalid_linked_array_index) ? node_index : list.back;
		list.back = node_index;
	} else {
		s_node &next_node = m_nodes[insert_before_this_node_index];
		node.prev = (next_node.prev == insert_before_this_node_index) ? node_index : next_node.prev;
		next_node.prev = node_index;
	}

	// Update prev node to point forward to this node
	if (insert_after_this_node_index == k_invalid_linked_array_index) {
		node.next = (list.front == k_invalid_linked_array_index) ? node_index : list.front;
		list.front = node_index;
	} else {
		s_node &prev_node = m_nodes[insert_after_this_node_index];
		node.next = (prev_node.next == insert_after_this_node_index) ? node_index : prev_node.next;
		prev_node.next = node_index;
	}
}

void c_linked_array::push_node_onto_list_front(s_linked_array_list &list, size_t node_index) {
	insert_node_into_list(list, node_index, list.front);
}

void c_linked_array::push_node_onto_list_back(s_linked_array_list &list, size_t node_index) {
	insert_node_into_list(list, node_index, k_invalid_linked_array_index);
}

void c_linked_array::remove_node_from_list(s_linked_array_list &list, size_t node_index) {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));
	s_node &node = m_nodes[node_index];

	wl_assert(is_node_either_free_or_in_any_list(node_index));

#if IS_TRUE(SLOW_LINKED_ARRAY_ASSERTS_ENABLED)
	wl_assert(is_node_in_list_slow(list, node_index));
#endif // IS_TRUE(SLOW_LINKED_ARRAY_ASSERTS_ENABLED)

	if (node.prev == node_index) {
		// This node is the list front
		list.front = (node.next == node_index) ? k_invalid_linked_array_index : node.next;
	} else {
		s_node &prev_node = m_nodes[node.prev];
		prev_node.next = (node.next == node_index) ? node.prev : node.next;
	}

	if (node.next == node_index) {
		// This node is the list back
		list.back = (node.prev == node_index) ? k_invalid_linked_array_index : node.prev;
	} else {
		s_node &next_node = m_nodes[node.next];
		next_node.prev = (node.prev == node_index) ? node.next : node.prev;
	}

	node.prev = k_invalid_linked_array_index;
	node.next = k_invalid_linked_array_index;
}

size_t c_linked_array::get_prev_node(size_t node_index) const {
	size_t prev = m_nodes[node_index].prev;
	return (prev == node_index) ? k_invalid_linked_array_index : prev;
}

size_t c_linked_array::get_next_node(size_t node_index) const {
	size_t next = m_nodes[node_index].next;
	return (next == node_index) ? k_invalid_linked_array_index : next;
}

bool c_linked_array::is_node_allocated_slow(size_t node_index) const {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));

	if (m_free_list_front == k_invalid_linked_array_index) {
		// Free list is empty, must be allocated
		return true;
	}

	size_t free_list_index = m_free_list_front;
	while (true) {
		if (free_list_index == node_index) {
			return false;
		}

		size_t next_free_list_index = m_nodes[free_list_index].next;
		if (free_list_index == next_free_list_index) {
			// Reached end of the list
			break;
		} else {
			free_list_index = next_free_list_index;
		}
	}

	return true;
}

bool c_linked_array::is_node_either_free_or_in_any_list(size_t node_index) const {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));
	const s_node &node = m_nodes[node_index];

	wl_assert((node.next == k_invalid_linked_array_index) == (node.prev == k_invalid_linked_array_index));
	return node.prev != k_invalid_linked_array_index;
}

bool c_linked_array::is_node_in_list_slow(const s_linked_array_list &list, size_t node_index) const {
	wl_assert(VALID_INDEX(node_index, m_nodes.size()));

	if (list.front == k_invalid_linked_array_index) {
		// List is empty
		return false;
	}

	size_t list_index = list.front;
	while (true) {
		if (list.front == node_index) {
			return true;
		}

		size_t next_list_index = m_nodes[list_index].next;
		if (list_index == next_list_index) {
			// Reached end of the list
			break;
		} else {
			list_index = next_list_index;
		}
	}

	return false;
}