#include "compiler/ast/node_module_call_argument.h"

c_AST_node_module_call_argument::c_AST_node_module_call_argument()
	: c_AST_node(k_ast_node_type) {}

e_AST_argument_direction c_AST_node_module_call_argument::get_argument_direction() const {
	return m_argument_direction;
}

void c_AST_node_module_call_argument::set_argument_direction(e_AST_argument_direction argument_direction) {
	m_argument_direction = argument_direction;
}

const char *c_AST_node_module_call_argument::get_name() const {
	return m_name.empty() ? nullptr : m_name.c_str();
}

void c_AST_node_module_call_argument::set_name(const char *name) {
	wl_assert(!name || !is_string_empty(name));
	m_name = name ? name : "";
}

c_AST_node_expression *c_AST_node_module_call_argument::get_value_expression() const {
	return m_value_expression.get();
}

void c_AST_node_module_call_argument::set_value_expression(c_AST_node_expression *value_expression) {
	m_value_expression.reset(value_expression);
}

c_AST_node *c_AST_node_module_call_argument::copy_internal() const {
	c_AST_node_module_call_argument *node_copy = new c_AST_node_module_call_argument();
	node_copy->m_argument_direction = m_argument_direction;
	node_copy->m_name = m_name;
	if (m_value_expression) {
		node_copy->m_value_expression.reset(m_value_expression->copy());
	}

	return node_copy;
}
