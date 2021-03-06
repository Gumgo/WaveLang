#pragma once

#include "common/common.h"

#include "compiler/ast/data_type.h"
#include "compiler/ast/node.h"
#include "compiler/ast/node_expression.h"

#include <memory>

class c_ast_node_module_call_argument : public c_ast_node {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_module_call_argument, k_module_call_argument, "module call argument");
	c_ast_node_module_call_argument();

	e_ast_argument_direction get_argument_direction() const;
	void set_argument_direction(e_ast_argument_direction argument_direction);

	const char *get_name() const;
	void set_name(const char *name);

	c_ast_node_expression *get_value_expression() const;
	void set_value_expression(c_ast_node_expression *value_expression);

protected:
	c_ast_node *copy_internal() const override;

private:
	e_ast_argument_direction m_argument_direction = e_ast_argument_direction::k_invalid;
	std::string m_name; // Empty if no name was specified
	std::unique_ptr<c_ast_node_expression> m_value_expression;
};
