#include "execution_graph/execution_graph.h"
#include "execution_graph/node_interface/node_interface.h"

c_node_interface::c_node_interface(c_execution_graph *execution_graph)
	: m_execution_graph(execution_graph) {}

c_node_reference c_node_interface::create_constant_node(real32 value) {
	c_node_reference node_reference = m_execution_graph->add_constant_node(value);
	m_created_node_references.push_back(node_reference);
	return node_reference;
}

c_node_reference c_node_interface::create_constant_node(bool value) {
	c_node_reference node_reference = m_execution_graph->add_constant_node(value);
	m_created_node_references.push_back(node_reference);
	return node_reference;
}

c_node_reference c_node_interface::create_constant_node(const char *value) {
	c_node_reference node_reference = m_execution_graph->add_constant_node(value);
	m_created_node_references.push_back(node_reference);
	return node_reference;
}

c_wrapped_array<const c_node_reference> c_node_interface::get_created_node_references() const {
	return c_wrapped_array<const c_node_reference>(m_created_node_references);
}
