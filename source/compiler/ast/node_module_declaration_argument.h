#pragma once

#include "common/common.h"

#include "compiler/ast/data_type.h"
#include "compiler/ast/node.h"
#include "compiler/ast/node_value_declaration.h"

#include <memory>

class c_ast_node_module_declaration_argument : public c_ast_node {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_module_declaration_argument, k_module_declaration_argument, "module argument");
	c_ast_node_module_declaration_argument();

	e_ast_argument_direction get_argument_direction() const;
	void set_argument_direction(e_ast_argument_direction argument_direction);

	c_ast_node_value_declaration *get_value_declaration() const;
	void set_value_declaration(c_ast_node_value_declaration *value_declaration);

	// Convenience accessors:
	const char *get_name() const;
	const c_ast_qualified_data_type &get_data_type() const;
	c_ast_node_expression *get_initialization_expression() const;

protected:
	c_ast_node *copy_internal() const override;

private:
	e_ast_argument_direction m_argument_direction = e_ast_argument_direction::k_invalid;

	// We use a value declaration because it includes type, name, and an optional default initializer
	std::unique_ptr<c_ast_node_value_declaration> m_value_declaration;
};
