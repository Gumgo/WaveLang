#include "compiler/ast/node_subscript.h"

c_ast_node_subscript::c_ast_node_subscript()
	: c_ast_node_expression(k_ast_node_type) {}

c_ast_node_module_call *c_ast_node_subscript::get_module_call() const {
	return m_module_call.get();
}

void c_ast_node_subscript::set_module_call(c_ast_node_module_call *module_call) {
	wl_assert(module_call->get_argument(0)->get_value_expression()->is_type(e_ast_node_type::k_declaration_reference));
	m_module_call.reset(module_call);
	set_data_type(module_call->get_data_type());
}

c_ast_node_declaration_reference *c_ast_node_subscript::get_reference() const {
	return m_module_call->get_argument(0)->get_value_expression()->get_as<c_ast_node_declaration_reference>();
}

c_ast_node_expression *c_ast_node_subscript::get_index_expression() const {
	return m_module_call->get_argument(1)->get_value_expression();
}

c_ast_node *c_ast_node_subscript::copy_internal() const {
	c_ast_node_subscript *node_copy = new c_ast_node_subscript();
	node_copy->set_data_type(get_data_type());
	if (m_module_call) {
		node_copy->m_module_call.reset(m_module_call->copy());
	}

	return node_copy;
}
