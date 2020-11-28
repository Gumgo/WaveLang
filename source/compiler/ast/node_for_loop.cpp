#include "compiler/ast/node_for_loop.h"

c_AST_node_for_loop::c_AST_node_for_loop()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node_value_declaration *c_AST_node_for_loop::get_value_declaration() const {
	return m_value_declaration.get();
}

void c_AST_node_for_loop::set_value_declaration(c_AST_node_value_declaration *value_declaration) {
	m_value_declaration.reset(value_declaration);
}

c_AST_node_expression *c_AST_node_for_loop::get_range_expression() const {
	return m_range_expression.get();
}

void c_AST_node_for_loop::set_range_expression(c_AST_node_expression *expression) {
	m_range_expression.reset(expression);
}

c_AST_node_scope *c_AST_node_for_loop::get_loop_scope() const {
	return m_loop_scope.get();
}

void c_AST_node_for_loop::set_loop_scope(c_AST_node_scope *loop_scope) {
	m_loop_scope.reset(loop_scope);
}

c_AST_node *c_AST_node_for_loop::copy_internal() const {
	c_AST_node_for_loop *node_copy = new c_AST_node_for_loop();
	if (m_value_declaration) {
		node_copy->m_value_declaration.reset(m_value_declaration->copy());
	}
	if (m_range_expression) {
		node_copy->m_range_expression.reset(m_range_expression->copy());
	}
	if (m_loop_scope) {
		node_copy->m_loop_scope.reset(m_loop_scope->copy());
	}

	return node_copy;
}
