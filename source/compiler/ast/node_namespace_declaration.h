#pragma once

#include "common/common.h"

#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_scope.h"

class c_AST_node_namespace_declaration : public c_AST_node_declaration {
public:
	AST_NODE_TYPE_DESCRIPTION(c_AST_node_namespace_declaration, k_namespace_declaration, "namespace");
	c_AST_node_namespace_declaration();

	c_AST_node_scope *get_scope() const;

protected:
	c_AST_node *copy_internal() const override;

private:
	std::unique_ptr<c_AST_node_scope> m_scope;
};
