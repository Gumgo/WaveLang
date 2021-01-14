#include "compiler/ast/node_convert.h"

c_ast_node_convert::c_ast_node_convert()
	: c_ast_node_expression(k_ast_node_type) {}

c_ast_node_expression *c_ast_node_convert::get_expression() const {
	return m_expression.get();
}

void c_ast_node_convert::set_expression_and_data_type(
	c_ast_node_expression *expression,
	const c_ast_qualified_data_type &data_type) {
	m_expression.reset(expression);
	c_ast_node_expression::set_data_type(data_type);
}

c_ast_node *c_ast_node_convert::copy_internal() const {
	c_ast_node_convert *node_copy = new c_ast_node_convert();
	node_copy->set_expression_and_data_type(m_expression ? m_expression->copy() : nullptr, get_data_type());
	return node_copy;
}
