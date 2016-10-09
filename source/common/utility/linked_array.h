#ifndef WAVELANG_COMMON_UTILITY_LINKED_ARRAY_H__
#define WAVELANG_COMMON_UTILITY_LINKED_ARRAY_H__

#include "common/common.h"

#include <vector>

struct s_linked_array_list {
	size_t front;
	size_t back;
};

class c_linked_array {
public:
	static const size_t k_invalid_linked_array_index = static_cast<size_t>(-1);

	void initialize(size_t count);

	void initialize_list(s_linked_array_list &list);
	size_t allocate_node();
	void free_node(size_t node_index);

	void insert_node_into_list(s_linked_array_list &list, size_t node_index, size_t insert_before_this_node_index);
	void push_node_onto_list_front(s_linked_array_list &list, size_t node_index);
	void push_node_onto_list_back(s_linked_array_list &list, size_t node_index);
	void remove_node_from_list(s_linked_array_list &list, size_t node_index);

	size_t get_prev_node(size_t node_index) const;
	size_t get_next_node(size_t node_index) const;

	bool is_node_allocated_slow(size_t node_index) const;
	bool is_node_either_free_or_in_any_list(size_t node_index) const;
	bool is_node_in_list_slow(const s_linked_array_list &list, size_t node_index) const;

private:
	struct s_node {
		// We use a special policy for node pointers. If a node is on no list at all, its prev and next indices are
		// invalid. However, if a node has no prev or next node but it is on a list, it points back to itself. This
		// allows us to distinguish between these cases.
		size_t prev;
		size_t next;
	};

	std::vector<s_node> m_nodes;

	// Only need a front pointer for the free list
	size_t m_free_list_front;
};

#endif // WAVELANG_COMMON_UTILITY_LINKED_ARRAY_H__
