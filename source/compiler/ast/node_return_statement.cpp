#include "compiler/ast/node_return_statement.h"

c_AST_node_return_statement::c_AST_node_return_statement()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node_expression *c_AST_node_return_statement::get_expression() const {
	return m_expression.get();
}

void c_AST_node_return_statement::set_expression(c_AST_node_expression *expression) {
	m_expression.reset(expression);
}

c_AST_node *c_AST_node_return_statement::copy_internal() const {
	c_AST_node_return_statement *node_copy = new c_AST_node_return_statement();
	if (m_expression) {
		node_copy->m_expression.reset(m_expression->copy());
	}

	return node_copy;
}
