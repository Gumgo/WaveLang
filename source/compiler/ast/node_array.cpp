#include "compiler/ast/node_array.h"

c_ast_node_array::c_ast_node_array()
	: c_ast_node_expression(k_ast_node_type) {
	set_data_type(c_ast_qualified_data_type::empty_array());
}

void c_ast_node_array::set_data_type(const c_ast_qualified_data_type &data_type) {
	wl_assert(data_type.is_array() || data_type.is_error());
	c_ast_node_expression::set_data_type(data_type);
}

void c_ast_node_array::add_element(c_ast_node_expression *element) {
	m_elements.emplace_back(element);
}

size_t c_ast_node_array::get_element_count() const {
	return m_elements.size();
}

c_ast_node_expression *c_ast_node_array::get_element(size_t index) const {
	return m_elements[index].get();
}

c_ast_node *c_ast_node_array::copy_internal() const {
	c_ast_node_array *node_copy = new c_ast_node_array();
	node_copy->set_data_type(get_data_type());
	node_copy->m_elements.reserve(m_elements.size());
	for (const std::unique_ptr<c_ast_node_expression> &element : m_elements) {
		node_copy->m_elements.emplace_back(element->copy());
	}

	return node_copy;
}
