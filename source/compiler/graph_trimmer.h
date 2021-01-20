#pragma once

#include "common/common.h"

#include "instrument/native_module_graph.h"

#include <stack>
#include <unordered_set>

class c_graph_trimmer {
public:
	c_graph_trimmer(c_native_module_graph &native_module_graph);

	c_native_module_graph &get_native_module_graph();

	// Temporary references are used to prevent a node from getting trimmed when it's in an intermediate state (e.g. the
	// result of an expression that has yet to be assigned to a variable). The methods do nothing if called on invalid
	// node references.
	void add_temporary_reference(h_graph_node node_handle);
	void remove_temporary_reference(h_graph_node node_handle, bool try_trim = true);

	void try_trim_node(h_graph_node node_handle);

private:
	void add_pending_node(h_graph_node node_handle);

	// The graph
	c_native_module_graph &m_native_module_graph;

	// List of nodes to check, cached here to avoid unnecessary allocations
	std::stack<h_graph_node> m_pending_nodes;
	std::unordered_set<h_graph_node> m_visited_nodes;
};
