#pragma once

#include "common/common.h"

#include "compiler/ast/data_type.h"
#include "compiler/ast/node.h"

class c_AST_node_expression : public c_AST_node {
public:
	AST_NODE_TYPE_DESCRIPTION(k_expression, "expression");
	c_AST_node_expression(c_ast_node_type type);

	const c_AST_qualified_data_type &get_data_type() const;

protected:
	void set_data_type(const c_AST_qualified_data_type &data_type);

private:
	c_AST_qualified_data_type m_data_type;
};
