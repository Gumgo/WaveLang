#include "compiler/ast/node_expression_statement.h"

c_ast_node_expression_statement::c_ast_node_expression_statement()
	: c_ast_node_scope_item(k_ast_node_type) {}

c_ast_node_expression *c_ast_node_expression_statement::get_expression() const {
	return m_expression.get();
}

void c_ast_node_expression_statement::set_expression(c_ast_node_expression *expression) {
	m_expression.reset(expression);
}

c_ast_node *c_ast_node_expression_statement::copy_internal() const {
	c_ast_node_expression_statement *node_copy = new c_ast_node_expression_statement();
	if (m_expression) {
		node_copy->m_expression.reset(m_expression->copy());
	}

	return node_copy;
}
