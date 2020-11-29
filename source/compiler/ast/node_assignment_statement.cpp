#include "compiler/ast/node_assignment_statement.h"

c_ast_node_assignment_statement::c_ast_node_assignment_statement()
	: c_ast_node_scope_item(k_ast_node_type) {}

c_ast_node_value_declaration *c_ast_node_assignment_statement::get_lhs_value_declaration() const {
	return m_lhs_value_declaration.get();
}

void c_ast_node_assignment_statement::set_lhs_value_declaration(c_ast_node_value_declaration *value_declaration) {
	m_lhs_value_declaration.reset(value_declaration);
}

c_ast_node_expression *c_ast_node_assignment_statement::get_lhs_index_expression() const {
	return m_lhs_index_expression.get();
}

void c_ast_node_assignment_statement::set_lhs_index_expression(c_ast_node_expression *index_expression) {
	m_lhs_index_expression.reset(index_expression);
}

c_ast_node_expression *c_ast_node_assignment_statement::get_rhs_expression() const {
	return m_rhs_expression.get();
}

void c_ast_node_assignment_statement::set_rhs_expression(c_ast_node_expression *expression) {
	m_rhs_expression.reset(expression);
}

c_ast_node *c_ast_node_assignment_statement::copy_internal() const {
	c_ast_node_assignment_statement *node_copy = new c_ast_node_assignment_statement();
	if (m_lhs_value_declaration) {
		node_copy->m_lhs_value_declaration.reset(m_lhs_value_declaration->copy());
	}
	if (m_lhs_index_expression) {
		node_copy->m_lhs_index_expression.reset(m_lhs_index_expression->copy());
	}
	if (m_rhs_expression) {
		node_copy->m_rhs_expression.reset(m_rhs_expression->copy());
	}

	return node_copy;
}
