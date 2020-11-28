#pragma once

#include "common/common.h"

#include "compiler/ast/data_type.h"
#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_expression.h"

#include <memory>

class c_AST_node_value_declaration : public c_AST_node_declaration {
public:
	AST_NODE_TYPE_DESCRIPTION(c_AST_node_value_declaration, k_value_declaration, "value");
	c_AST_node_value_declaration();

	const c_AST_qualified_data_type &get_data_type() const;
	void set_data_type(const c_AST_qualified_data_type &data_type);

	bool is_modifiable() const;
	void set_modifiable(bool modifiable);

	c_AST_node_expression *get_initialization_expression() const;
	void set_initialization_expression(c_AST_node_expression *initialization_expression);

protected:
	c_AST_node *copy_internal() const override;

private:
	c_AST_qualified_data_type m_data_type;
	bool m_modifiable = true;
	std::unique_ptr<c_AST_node_expression> m_initialization_expression;
};
