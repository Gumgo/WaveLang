#include "compiler/ast/node_convert.h"

c_ast_node_convert::c_ast_node_convert()
	: c_ast_node_expression(k_ast_node_type) {}

void c_ast_node_convert::set_data_type(const c_ast_qualified_data_type &data_type) {
	c_ast_node_expression::set_data_type(data_type);
}

c_ast_node *c_ast_node_convert::copy_internal() const {
	c_ast_node_convert *node_copy = new c_ast_node_convert();
	node_copy->set_data_type(get_data_type());

	return node_copy;
}
