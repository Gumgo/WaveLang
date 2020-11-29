#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_ast_node_access : public c_ast_node_expression {
public:
	AST_NODE_TYPE(c_ast_node_access, k_access);
	c_ast_node_access();

protected:
	c_ast_node *copy_internal() const override;
};
