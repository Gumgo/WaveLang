#pragma once

#include "common/common.h"

#include "compiler/ast/node_scope_item.h"

class c_AST_node_continue_statement : public c_AST_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_AST_node_continue_statement, k_continue_statement, "continue statement");
	c_AST_node_continue_statement();

protected:
	c_AST_node *copy_internal() const override;
};
