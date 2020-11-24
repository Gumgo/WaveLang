#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_AST_node_access : public c_AST_node_expression {
public:
	AST_NODE_TYPE(k_access);
	c_AST_node_access();
};
