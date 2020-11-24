#include "compiler/ast/node_array.h"

c_AST_node_array::c_AST_node_array()
	: c_AST_node_expression(k_ast_node_type) {
	set_data_type(c_AST_qualified_data_type::empty_array());
}

void c_AST_node_array::set_data_type(const c_AST_qualified_data_type &data_type) {
	wl_assert(data_type.is_array() || data_type.is_error());
	c_AST_node_expression::set_data_type(data_type);
}

void c_AST_node_array::add_element(const c_AST_node_expression *element) {
	m_elements.emplace_back(element);
}

size_t c_AST_node_array::get_element_count() const {
	return m_elements.size();
}

c_AST_node_expression *c_AST_node_array::get_element(size_t index) const {
	return m_elements[index].get();
}
