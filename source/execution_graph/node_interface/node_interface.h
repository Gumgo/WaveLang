#pragma once

#include "common/common.h"

#include "execution_graph/node_reference.h"

#include <vector>

class c_execution_graph;

class c_node_interface {
public:
	c_node_interface(c_execution_graph *execution_graph);

	c_node_reference create_constant_node(real32 value);
	c_node_reference create_constant_node(bool value);
	c_node_reference create_constant_node(const char *value);

	c_wrapped_array<const c_node_reference> get_created_node_references() const;

private:
	c_execution_graph *m_execution_graph;
	std::vector<c_node_reference> m_created_node_references;
};
