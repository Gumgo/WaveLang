#include "compiler/ast/node_if_statement.h"

c_ast_node_if_statement::c_ast_node_if_statement()
	: c_ast_node_scope_item(k_ast_node_type) {}

c_ast_node_expression *c_ast_node_if_statement::get_expression() const {
	return m_expression.get();
}

void c_ast_node_if_statement::set_expression(c_ast_node_expression *expression) {
	m_expression.reset(expression);
}

c_ast_node_scope *c_ast_node_if_statement::get_true_scope() const {
	return m_true_scope.get();
}

void c_ast_node_if_statement::set_true_scope(c_ast_node_scope *true_scope) {
	m_true_scope.reset(true_scope);
}

c_ast_node_scope *c_ast_node_if_statement::get_false_scope() const {
	return m_false_scope.get();
}

void c_ast_node_if_statement::set_false_scope(c_ast_node_scope *false_scope) {
	m_false_scope.reset(false_scope);
}

c_ast_node *c_ast_node_if_statement::copy_internal() const {
	c_ast_node_if_statement *node_copy = new c_ast_node_if_statement();
	if (m_expression) {
		node_copy->m_expression.reset(m_expression->copy());
	}
	if (m_true_scope) {
		node_copy->m_true_scope.reset(m_true_scope->copy());
	}
	if (m_false_scope) {
		node_copy->m_false_scope.reset(m_false_scope->copy());
	}

	return node_copy;
}
