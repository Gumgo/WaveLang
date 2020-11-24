#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_scope_item.h"

#include <memory>

class c_AST_node_expression_statement : public c_AST_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(k_expression_statement, "expression statement");
	c_AST_node_expression_statement();

	c_AST_node_expression *get_expression() const;
	void set_expression(c_AST_node_expression *expression);

private:
	std::unique_ptr<c_AST_node_expression> m_expression;
};
