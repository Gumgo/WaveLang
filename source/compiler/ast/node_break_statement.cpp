#include "compiler/ast/node_break_statement.h"

c_AST_node_break_statement::c_AST_node_break_statement()
	: c_AST_node_scope_item(k_ast_node_type) {}

c_AST_node *c_AST_node_break_statement::copy_internal() const {
	c_AST_node_break_statement *node_copy = new c_AST_node_break_statement();
	return node_copy;
}
