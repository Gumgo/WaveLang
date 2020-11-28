#pragma once

#include "common/common.h"

#include "execution_graph/execution_graph.h"

#include <stack>
#include <unordered_set>

class c_graph_trimmer {
public:
	using f_on_node_removed = void (*)(void *context, c_node_reference node_reference);

	c_graph_trimmer(c_execution_graph &execution_graph);

	void set_on_node_removed(f_on_node_removed on_node_removed, void *context);
	c_execution_graph &get_execution_graph();

	// Temporary references are used to prevent a node from getting trimmed when it's in an intermediate state (e.g. the
	// result of an expression that has yet to be assigned to a variable). The methods do nothing if called on invalid
	// node references.
	void add_temporary_reference(c_node_reference node_reference);
	void remove_temporary_reference(c_node_reference node_reference, bool try_trim = true);

	void try_trim_node(c_node_reference node_reference);

private:
	void add_pending_node(c_node_reference node_reference);

	// The graph
	c_execution_graph &m_execution_graph;

	f_on_node_removed m_on_node_removed = nullptr;
	void *m_on_node_removed_context = nullptr;

	// List of nodes to check, cached here to avoid unnecessary allocations
	std::stack<c_node_reference> m_pending_nodes;
	std::unordered_set<c_node_reference> m_visited_nodes;
};
