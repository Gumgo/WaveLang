#include "compiler/ast/node_module_declaration_argument.h"

c_AST_node_module_declaration_argument::c_AST_node_module_declaration_argument()
	: c_AST_node(k_ast_node_type) {}

e_AST_argument_direction c_AST_node_module_declaration_argument::get_argument_direction() const {
	return m_argument_direction;
}

void c_AST_node_module_declaration_argument::set_argument_direction(e_AST_argument_direction argument_direction) {
	m_argument_direction = argument_direction;
}

c_AST_node_value_declaration *c_AST_node_module_declaration_argument::get_value_declaration() const {
	return m_value_declaration.get();
}

void c_AST_node_module_declaration_argument::set_value_declaration(c_AST_node_value_declaration *value_declaration) {
	m_value_declaration.reset(value_declaration);
}

const char *c_AST_node_module_declaration_argument::get_name() const {
	return m_value_declaration->get_name();
}

const c_AST_qualified_data_type &c_AST_node_module_declaration_argument::get_data_type() const {
	return m_value_declaration->get_data_type();
}

c_AST_node_expression *c_AST_node_module_declaration_argument::get_initialization_expression() const {
	return m_value_declaration->get_initialization_expression();
}
