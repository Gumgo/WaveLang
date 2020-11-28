#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_AST_node_access : public c_AST_node_expression {
public:
	AST_NODE_TYPE(c_AST_node_access, k_access);
	c_AST_node_access();

protected:
	c_AST_node *copy_internal() const override;
};
