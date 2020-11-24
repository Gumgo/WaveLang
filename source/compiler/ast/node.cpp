#include "compiler/ast/node.h"

c_AST_node::c_AST_node(c_ast_node_type type)
	: m_type(type) {}

const s_compiler_source_location &c_AST_node::get_source_location() const {
	return m_source_location;
}

void c_AST_node::set_source_location(const s_compiler_source_location &source_location) {
	m_source_location = source_location;
}
