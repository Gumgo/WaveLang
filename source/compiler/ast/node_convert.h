#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_AST_node_convert : public c_AST_node_expression {
public:
	AST_NODE_TYPE(k_convert);
	c_AST_node_convert();

	void set_data_type(const c_AST_qualified_data_type &data_type);
};
