#include "compiler/ast/node_access.h"

c_ast_node_access::c_ast_node_access()
	: c_ast_node_expression(k_ast_node_type) {}

c_ast_node *c_ast_node_access::copy_internal() const {
	c_ast_node_access *node_copy = new c_ast_node_access();
	return node_copy;
}
