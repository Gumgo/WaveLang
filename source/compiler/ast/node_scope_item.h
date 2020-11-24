#pragma once

#include "common/common.h"

#include "compiler/ast/node.h"

enum class e_ast_visibility {
	k_invalid,	// Visibility does not apply to this item
	k_private,	// This item is visible only within this source file
	k_public,	// This item is visible to all source files that import the source file it was declared in

	k_count,

	k_default = k_private
};

class c_AST_node_scope_item : public c_AST_node {
public:
	AST_NODE_TYPE(k_scope_item);
	c_AST_node_scope_item(c_ast_node_type type);

	e_ast_visibility get_visibility() const;
	void set_visibility(e_ast_visibility visibility);

private:
	e_ast_visibility m_visibility = e_ast_visibility::k_invalid;
};
