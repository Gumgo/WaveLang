#include "compiler/ast/node_module_call.h"

c_AST_node_module_call::c_AST_node_module_call()
	: c_AST_node_expression(k_ast_node_type) {
	// Before the module call is resolved, its return type is set to the error type so that if resolution fails we don't
	// keep reporting a chain of type errors.
	set_data_type(c_AST_qualified_data_type::error());
}

void c_AST_node_module_call::add_argument(const c_AST_node_module_call_argument *argument) {
	m_arguments.emplace_back(argument);
}

size_t c_AST_node_module_call::get_argument_count() const {
	return m_arguments.size();
}

c_AST_node_module_call_argument *c_AST_node_module_call::get_argument(size_t index) const {
	return m_arguments[index].get();
}

c_AST_node_module_declaration *c_AST_node_module_call::get_resolved_module_declaration() const {
	return m_resolved_module_declaration;
}

void c_AST_node_module_call::set_resolved_module_declaration(
	c_AST_node_module_declaration *resolved_module_declaration,
	e_AST_data_mutability dependent_constant_output_mutability) {
	m_resolved_module_declaration = resolved_module_declaration;

	c_AST_qualified_data_type return_type = resolved_module_declaration->get_return_type();
	if (return_type.get_data_mutability() == e_AST_data_mutability::k_dependent_constant) {
		return_type = c_AST_qualified_data_type(
			return_type.get_data_type(),
			dependent_constant_output_mutability);
	}

	set_data_type(return_type);
}
