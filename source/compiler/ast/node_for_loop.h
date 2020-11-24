#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_scope.h"
#include "compiler/ast/node_scope_item.h"
#include "compiler/ast/node_value_declaration.h"

#include <memory>

class c_AST_node_for_loop : public c_AST_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(k_for_loop, "for loop");
	c_AST_node_for_loop();

	c_AST_node_value_declaration *get_value_declaration() const;
	void set_value_declaration(c_AST_node_value_declaration *value_declaration);

	c_AST_node_expression *get_range_expression() const;
	void set_range_expression(c_AST_node_expression *expression);

	c_AST_node_scope *get_loop_scope() const;
	void set_loop_scope(c_AST_node_scope *loop_scope);

private:
	std::unique_ptr<c_AST_node_value_declaration> m_value_declaration;
	std::unique_ptr<c_AST_node_expression> m_range_expression;
	std::unique_ptr<c_AST_node_scope> m_loop_scope;
};
