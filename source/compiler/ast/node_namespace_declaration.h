#pragma once

#include "common/common.h"

#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_scope.h"

class c_ast_node_namespace_declaration : public c_ast_node_declaration {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_namespace_declaration, k_namespace_declaration, "namespace");
	c_ast_node_namespace_declaration();

	c_ast_node_scope *get_scope() const;

protected:
	c_ast_node *copy_internal() const override;

private:
	std::unique_ptr<c_ast_node_scope> m_scope;
};
