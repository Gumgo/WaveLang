#include "compiler/ast/node_namespace_declaration.h"

c_AST_node_namespace_declaration::c_AST_node_namespace_declaration()
	: c_AST_node_declaration(k_ast_node_type)
	, m_scope(new c_AST_node_scope()) {}

c_AST_node_scope *c_AST_node_namespace_declaration::get_scope() const {
	return m_scope.get();
}
