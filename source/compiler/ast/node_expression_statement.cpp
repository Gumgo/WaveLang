#include "compiler/ast/node_expression_statement.h"

c_AST_node_expression_statement::c_AST_node_expression_statement()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node_expression *c_AST_node_expression_statement::get_expression() const {
	return m_expression.get();
}

void c_AST_node_expression_statement::set_expression(c_AST_node_expression *expression) {
	m_expression.reset(expression);
}
