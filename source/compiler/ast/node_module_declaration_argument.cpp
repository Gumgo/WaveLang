#include "compiler/ast/node_module_declaration_argument.h"

c_ast_node_module_declaration_argument::c_ast_node_module_declaration_argument()
	: c_ast_node(k_ast_node_type) {}

e_ast_argument_direction c_ast_node_module_declaration_argument::get_argument_direction() const {
	return m_argument_direction;
}

void c_ast_node_module_declaration_argument::set_argument_direction(e_ast_argument_direction argument_direction) {
	m_argument_direction = argument_direction;
}

c_ast_node_value_declaration *c_ast_node_module_declaration_argument::get_value_declaration() const {
	return m_value_declaration.get();
}

void c_ast_node_module_declaration_argument::set_value_declaration(c_ast_node_value_declaration *value_declaration) {
	m_value_declaration.reset(value_declaration);
}

const char *c_ast_node_module_declaration_argument::get_name() const {
	return m_value_declaration->get_name();
}

const c_ast_qualified_data_type &c_ast_node_module_declaration_argument::get_data_type() const {
	return m_value_declaration->get_data_type();
}

c_ast_node_expression *c_ast_node_module_declaration_argument::get_initialization_expression() const {
	return m_value_declaration->get_initialization_expression();
}

c_ast_node *c_ast_node_module_declaration_argument::copy_internal() const {
	c_ast_node_module_declaration_argument *node_copy = new c_ast_node_module_declaration_argument();
	node_copy->m_argument_direction = m_argument_direction;
	if (m_value_declaration) {
		node_copy->m_value_declaration.reset(m_value_declaration->copy());
	}

	return node_copy;
}
