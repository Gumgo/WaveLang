#include "compiler/ast/node_expression.h"

c_ast_node_expression::c_ast_node_expression(c_ast_node_type type)
	: c_ast_node(type + k_ast_node_type) {}

const c_ast_qualified_data_type &c_ast_node_expression::get_data_type() const {
	return m_data_type;
}

void c_ast_node_expression::set_data_type(const c_ast_qualified_data_type &data_type) {
	m_data_type = data_type;
}
