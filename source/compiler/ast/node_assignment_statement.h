#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_scope_item.h"
#include "compiler/ast/node_value_declaration.h"

#include <memory>

class c_AST_node_assignment_statement : public c_AST_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_AST_node_assignment_statement, k_assignment_statement, "assignment statement");
	c_AST_node_assignment_statement();

	c_AST_node_value_declaration *get_lhs_value_declaration() const;
	void set_lhs_value_declaration(c_AST_node_value_declaration *value_declaration);

	c_AST_node_expression *get_lhs_index_expression() const;
	void set_lhs_index_expression(c_AST_node_expression *index_expression);

	c_AST_node_expression *get_rhs_expression() const;
	void set_rhs_expression(c_AST_node_expression *expression);

protected:
	c_AST_node *copy_internal() const override;

private:
	std::unique_ptr<c_AST_node_value_declaration> m_lhs_value_declaration;
	std::unique_ptr<c_AST_node_expression> m_lhs_index_expression;
	std::unique_ptr<c_AST_node_expression> m_rhs_expression;
};
