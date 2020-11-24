#include "compiler/ast/node_declaration_reference.h"
#include "compiler/ast/node_value_declaration.h"

c_AST_node_declaration_reference::c_AST_node_declaration_reference()
	: c_AST_node_expression(k_ast_node_type) {
	// By default, this node is invalid and has an error data type
	set_data_type(c_AST_qualified_data_type::error());
}

bool c_AST_node_declaration_reference::is_valid() const {
	return !m_references.empty();
}

void c_AST_node_declaration_reference::add_reference(c_AST_node_declaration *reference) {
#if IS_TRUE(ASSERTS_ENABLED)
	// Make sure we're referencing overloaded modules if we have multiple references
	if (!m_references.empty()) {
		wl_assert(m_references.front()->is_type(e_AST_node_type::k_module_declaration));
		wl_assert(reference->is_type(e_AST_node_type::k_module_declaration));
		wl_assert(strcmp(m_references.front()->get_name(), reference->get_name()) == 0);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	m_references.push_back(reference);

	c_AST_node_value_declaration *value_reference = reference->try_get_as<c_AST_node_value_declaration>();
	if (value_reference) {
		set_data_type(value_reference->get_data_type());
	} else {
		set_data_type(c_AST_qualified_data_type());
	}
}

c_wrapped_array<c_AST_node_declaration *> c_AST_node_declaration_reference::get_references() const {
	return c_wrapped_array<c_AST_node_declaration *>(m_references);
}
