#include "compiler/ast/node.h"

c_ast_node::c_ast_node(c_ast_node_type type)
	: m_type(type) {}

const s_compiler_source_location &c_ast_node::get_source_location() const {
	return m_source_location;
}

void c_ast_node::set_source_location(const s_compiler_source_location &source_location) {
	m_source_location = source_location;
}
