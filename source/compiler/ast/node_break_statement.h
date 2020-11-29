#pragma once

#include "common/common.h"

#include "compiler/ast/node_scope_item.h"

class c_ast_node_break_statement : public c_ast_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_break_statement, k_break_statement, "break statement");
	c_ast_node_break_statement();

protected:
	c_ast_node *copy_internal() const override;
};
