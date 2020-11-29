#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_scope.h"
#include "compiler/ast/node_scope_item.h"

#include <memory>

class c_ast_node_if_statement : public c_ast_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_if_statement, k_if_statement, "if statement");
	c_ast_node_if_statement();

	c_ast_node_expression *get_expression() const;
	void set_expression(c_ast_node_expression *expression);

	c_ast_node_scope *get_true_scope() const;
	void set_true_scope(c_ast_node_scope *true_scope);

	c_ast_node_scope *get_false_scope() const;
	void set_false_scope(c_ast_node_scope *true_scope);

protected:
	c_ast_node *copy_internal() const override;

private:
	std::unique_ptr<c_ast_node_expression> m_expression;
	std::unique_ptr<c_ast_node_scope> m_true_scope;
	std::unique_ptr<c_ast_node_scope> m_false_scope;
};
