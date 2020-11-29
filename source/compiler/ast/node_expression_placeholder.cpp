#include "compiler/ast/node_expression_placeholder.h"

c_ast_node_expression_placeholder::c_ast_node_expression_placeholder()
	: c_ast_node_expression(k_ast_node_type) {}


c_ast_node *c_ast_node_expression_placeholder::copy_internal() const {
	c_ast_node_expression_placeholder *node_copy = new c_ast_node_expression_placeholder();
	node_copy->set_data_type(get_data_type());

	return node_copy;
}
