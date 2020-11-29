#pragma once

#include "common/common.h"

#include "compiler/ast/node_declaration_reference.h"
#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_module_call.h"

#include <memory>

// This node is created when we detect a subscript operation on a reference. This node is legal to place on the LHS of
// an assignment operation, whereas an ordinary subscript module call is not.
class c_ast_node_subscript : public c_ast_node_expression {
public:
	AST_NODE_TYPE(c_ast_node_subscript, k_subscript);
	c_ast_node_subscript();

	// If this subscript operation is on the RHS of an expression, it is executed as a module call
	c_ast_node_module_call *get_module_call() const;
	void set_module_call(c_ast_node_module_call *module_call);

	// If this subscript operation is on the LHS of an expression (e.g. x[3] = 5), we treat it not as a module call but
	// as a declaration reference with an index
	c_ast_node_declaration_reference *get_reference() const;
	c_ast_node_expression *get_index_expression() const;

protected:
	c_ast_node *copy_internal() const override;

private:
	std::unique_ptr<c_ast_node_module_call> m_module_call;
};
