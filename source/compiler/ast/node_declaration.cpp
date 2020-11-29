#include "compiler/ast/node_declaration.h"

c_ast_node_declaration::c_ast_node_declaration(c_ast_node_type type)
	: c_ast_node_scope_item(type + k_ast_node_type) {}

const char *c_ast_node_declaration::get_name() const {
	return m_name.c_str();
}

void c_ast_node_declaration::set_name(const char *name) {
	m_name = name;
}
