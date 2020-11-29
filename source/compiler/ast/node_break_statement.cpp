#include "compiler/ast/node_break_statement.h"

c_ast_node_break_statement::c_ast_node_break_statement()
	: c_ast_node_scope_item(k_ast_node_type) {}

c_ast_node *c_ast_node_break_statement::copy_internal() const {
	c_ast_node_break_statement *node_copy = new c_ast_node_break_statement();
	return node_copy;
}
