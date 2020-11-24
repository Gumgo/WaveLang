#include "compiler/ast/node_if_statement.h"

c_AST_node_if_statement::c_AST_node_if_statement()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node_expression *c_AST_node_if_statement::get_expression() const {
	return m_expression.get();
}

void c_AST_node_if_statement::set_expression(c_AST_node_expression *expression) {
	m_expression.reset(expression);
}

c_AST_node_scope *c_AST_node_if_statement::get_true_scope() const {
	return m_true_scope.get();
}

void c_AST_node_if_statement::set_true_scope(c_AST_node_scope *true_scope) {
	m_true_scope.reset(true_scope);
}

c_AST_node_scope *c_AST_node_if_statement::get_false_scope() const {
	return m_false_scope.get();
}

void c_AST_node_if_statement::set_false_scope(c_AST_node_scope *false_scope) {
	m_false_scope.reset(false_scope);
}
