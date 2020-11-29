#pragma once

#include "common/common.h"

#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_expression.h"

#include <vector>

class c_ast_node_declaration_reference : public c_ast_node_expression {
public:
	AST_NODE_TYPE(c_ast_node_declaration_reference, k_declaration_reference);
	c_ast_node_declaration_reference();

	// If this is false, the data type is the error type.
	bool is_valid() const;

	// We may only reference multiple declarations in the case of overloaded modules, other situations are illegal. When
	// this node references a value, the data type automatically gets set. Otherwise, the data type is invalid.
	void add_reference(c_ast_node_declaration *reference);
	c_wrapped_array<c_ast_node_declaration *> get_references() const;

protected:
	c_ast_node *copy_internal() const override;

private:
	std::vector<c_ast_node_declaration *> m_references;
};
