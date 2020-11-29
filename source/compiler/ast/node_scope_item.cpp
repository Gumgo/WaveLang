#include "compiler/ast/node_scope_item.h"

c_ast_node_scope_item::c_ast_node_scope_item(c_ast_node_type type)
	: c_ast_node(type + k_ast_node_type) {}

e_ast_visibility c_ast_node_scope_item::get_visibility() const {
	return m_visibility;
}

void c_ast_node_scope_item::set_visibility(e_ast_visibility visibility) {
	m_visibility = visibility;
}
