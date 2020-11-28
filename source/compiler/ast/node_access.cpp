#include "compiler/ast/node_access.h"

c_AST_node_access::c_AST_node_access()
	: c_AST_node_expression(k_ast_node_type) {}

c_AST_node *c_AST_node_access::copy_internal() const {
	c_AST_node_access *node_copy = new c_AST_node_access();
	return node_copy;
}
