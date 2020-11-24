#include "compiler/ast/node_convert.h"

c_AST_node_convert::c_AST_node_convert()
	: c_AST_node_expression(k_ast_node_type) {}

void c_AST_node_convert::set_data_type(const c_AST_qualified_data_type &data_type) {
	c_AST_node_expression::set_data_type(data_type);
}
