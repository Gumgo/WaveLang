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
