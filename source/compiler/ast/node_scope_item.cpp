#include "compiler/ast/node_scope_item.h"

c_AST_node_scope_item::c_AST_node_scope_item(c_ast_node_type type)
	: c_AST_node_scope_item(type + k_ast_node_type) {}

e_ast_visibility c_AST_node_scope_item::get_visibility() const {
	return m_visibility;
}

void c_AST_node_scope_item::set_visibility(e_ast_visibility visibility) {
	m_visibility = visibility;
}
