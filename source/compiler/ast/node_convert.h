#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_ast_node_convert : public c_ast_node_expression {
public:
	AST_NODE_TYPE(c_ast_node_convert, k_convert);
	c_ast_node_convert();

	c_ast_node_expression *get_expression() const;
	void set_expression_and_data_type(c_ast_node_expression *expression, const c_ast_qualified_data_type &data_type);

protected:
	c_ast_node *copy_internal() const override;

	std::unique_ptr<c_ast_node_expression> m_expression;
};
