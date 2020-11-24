#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

// This node acts as a placeholder for expressions during the declaration pass. It is replaced with a real expression
// when definitions are parsed.
class c_AST_node_expression_placeholder : public c_AST_node_expression {
public:
	AST_NODE_TYPE(k_expression_placeholder);
	c_AST_node_expression_placeholder();
};
