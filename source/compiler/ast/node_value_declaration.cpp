#include "compiler/ast/node_value_declaration.h"

c_ast_node_value_declaration::c_ast_node_value_declaration()
	: c_ast_node_declaration(k_ast_node_type) {}

const c_ast_qualified_data_type &c_ast_node_value_declaration::get_data_type() const {
	return m_data_type;
}

void c_ast_node_value_declaration::set_data_type(const c_ast_qualified_data_type &data_type) {
	m_data_type = data_type;
}

bool c_ast_node_value_declaration::is_modifiable() const {
	return m_modifiable;
}

void c_ast_node_value_declaration::set_modifiable(bool modifiable) {
	m_modifiable = modifiable;
}

c_ast_node_expression *c_ast_node_value_declaration::get_initialization_expression() const {
	return m_initialization_expression.get();
}

void c_ast_node_value_declaration::set_initialization_expression(c_ast_node_expression *initialization_expression) {
	m_initialization_expression.reset(initialization_expression);
}

c_ast_node *c_ast_node_value_declaration::copy_internal() const {
	c_ast_node_value_declaration *node_copy = new c_ast_node_value_declaration();
	node_copy->m_data_type = m_data_type;
	node_copy->m_modifiable = m_modifiable;
	if (m_initialization_expression) {
		node_copy->m_initialization_expression.reset(m_initialization_expression->copy());
	}

	return node_copy;
}
