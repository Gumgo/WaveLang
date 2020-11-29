#pragma once

#include "common/common.h"

#include "compiler/ast/node_scope_item.h"

class c_ast_node_declaration : public c_ast_node_scope_item {
public:
	AST_NODE_TYPE(c_ast_node_declaration, k_declaration);
	c_ast_node_declaration(c_ast_node_type type);

	const char *get_name() const;
	void set_name(const char *name);

private:
	std::string m_name;
};
