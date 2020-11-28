#include "compiler/ast/node_literal.h"

c_AST_node_literal::c_AST_node_literal()
	: c_AST_node_expression(k_ast_node_type) {}

real32 c_AST_node_literal::get_real_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_real),
			e_AST_data_mutability::k_constant));
	return m_real_value;
}

void c_AST_node_literal::set_real_value(real32 value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_real),
			e_AST_data_mutability::k_constant));
	m_real_value = value;
}

bool c_AST_node_literal::get_bool_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_bool),
			e_AST_data_mutability::k_constant));
	return m_bool_value;
}

void c_AST_node_literal::set_bool_value(bool value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_bool),
			e_AST_data_mutability::k_constant));
	m_bool_value = value;
}

const char *c_AST_node_literal::get_string_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_string),
			e_AST_data_mutability::k_constant));
	return m_string_value.c_str();
}

void c_AST_node_literal::set_string_value(const char *value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_string),
			e_AST_data_mutability::k_constant));
	m_string_value = value;
}

c_AST_node *c_AST_node_literal::copy_internal() const {
	c_AST_node_literal *node_copy = new c_AST_node_literal();
	node_copy->set_data_type(get_data_type());
	switch (get_data_type().get_primitive_type()) {
	case e_AST_primitive_type::k_invalid:
		break;

	case e_AST_primitive_type::k_real:
		node_copy->m_real_value = m_real_value;
		break;

	case e_AST_primitive_type::k_bool:
		node_copy->m_bool_value = m_bool_value;
		break;

	case e_AST_primitive_type::k_string:
		node_copy->m_string_value = m_string_value;
		break;

	default:
		wl_unreachable();
	}

	return node_copy;
}
