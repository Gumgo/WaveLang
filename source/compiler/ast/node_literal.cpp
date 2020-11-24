#include "compiler/ast/node_literal.h"

c_AST_node_literal::c_AST_node_literal()
	: c_AST_node_expression(k_ast_node_type) {}

real32 c_AST_node_literal::get_real_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_real),
			enum_flag(e_AST_qualifier::k_const)));
	return m_real_value;
}

void c_AST_node_literal::set_real_value(real32 value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_real),
			enum_flag(e_AST_qualifier::k_const)));
	m_real_value = value;
}

bool c_AST_node_literal::get_bool_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_bool),
			enum_flag(e_AST_qualifier::k_const)));
	return m_bool_value;
}

void c_AST_node_literal::set_bool_value(bool value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_bool),
			enum_flag(e_AST_qualifier::k_const)));
	m_bool_value = value;
}

const char *c_AST_node_literal::get_string_value() const {
	wl_assert(
		get_data_type() == c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_string),
			enum_flag(e_AST_qualifier::k_const)));
	return m_string_value.c_str();
}

void c_AST_node_literal::set_string_value(const char *value) {
	set_data_type(
		c_AST_qualified_data_type(
			c_AST_data_type(e_AST_primitive_type::k_string),
			enum_flag(e_AST_qualifier::k_const)));
	m_string_value = value;
}
