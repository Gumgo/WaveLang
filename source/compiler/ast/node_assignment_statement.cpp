#include "compiler/ast/node_assignment_statement.h"

c_AST_node_assignment_statement::c_AST_node_assignment_statement()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node_value_declaration *c_AST_node_assignment_statement::get_lhs_value_declaration() const {
	return m_lhs_value_declaration.get();
}

void c_AST_node_assignment_statement::set_lhs_value_declaration(c_AST_node_value_declaration *value_declaration) {
	m_lhs_value_declaration.reset(value_declaration);
}

c_AST_node_expression *c_AST_node_assignment_statement::get_rhs_expression() const {
	return m_rhs_expression.get();
}

void c_AST_node_assignment_statement::set_rhs_expression(c_AST_node_expression *expression) {
	m_rhs_expression.reset(expression);
}
