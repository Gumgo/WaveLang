#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_scope_item.h"

#include <memory>

class c_ast_node_return_statement : public c_ast_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_return_statement, k_return_statement, "return statement");
	c_ast_node_return_statement();

	c_ast_node_expression *get_expression() const;
	void set_expression(c_ast_node_expression *expression);

protected:
	c_ast_node *copy_internal() const override;

private:
	std::unique_ptr<c_ast_node_expression> m_expression;
};
