#include "compiler/ast/node_value_declaration.h"

c_AST_node_value_declaration::c_AST_node_value_declaration()
	: c_AST_node_declaration(k_ast_node_type) {}

const c_AST_qualified_data_type &c_AST_node_value_declaration::get_data_type() const {
	return m_data_type;
}

void c_AST_node_value_declaration::set_data_type(const c_AST_qualified_data_type &data_type) {
	m_data_type = data_type;
}

bool c_AST_node_value_declaration::is_modifiable() const {
	return m_modifiable;
}

void c_AST_node_value_declaration::set_modifiable(bool modifiable) {
	m_modifiable = modifiable;
}

c_AST_node_expression *c_AST_node_value_declaration::get_initialization_expression() const {
	return m_initialization_expression.get();
}

void c_AST_node_value_declaration::set_initialization_expression(c_AST_node_expression *initialization_expression) {
	m_initialization_expression.reset(initialization_expression);
}
