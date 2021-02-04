#include "compiler/ast/node_module_call.h"

c_ast_node_module_call::c_ast_node_module_call()
	: c_ast_node_expression(k_ast_node_type) {
	// Before the module call is resolved, its return type is set to the error type so that if resolution fails we don't
	// keep reporting a chain of type errors.
	set_data_type(c_ast_qualified_data_type::error());
}

void c_ast_node_module_call::add_argument(c_ast_node_module_call_argument *argument) {
	m_arguments.emplace_back(argument);
}

size_t c_ast_node_module_call::get_argument_count() const {
	return m_arguments.size();
}

c_ast_node_module_call_argument *c_ast_node_module_call::get_argument(size_t index) const {
	return m_arguments[index].get();
}

c_ast_node_module_declaration *c_ast_node_module_call::get_resolved_module_declaration() const {
	return m_resolved_module_declaration;
}

void c_ast_node_module_call::set_resolved_module_declaration(
	c_ast_node_module_declaration *resolved_module_declaration,
	e_ast_data_mutability dependent_constant_data_mutability,
	uint32 upsample_factor) {
	wl_assert(upsample_factor > 0);
	m_resolved_module_declaration = resolved_module_declaration;
	m_dependent_constant_data_mutability = dependent_constant_data_mutability;
	m_upsample_factor = upsample_factor;

	c_ast_qualified_data_type return_type = resolved_module_declaration->get_return_type();
	if (return_type.get_data_mutability() == e_ast_data_mutability::k_dependent_constant) {
		return_type = c_ast_qualified_data_type(
			return_type.get_data_type(),
			dependent_constant_data_mutability);
	}

	set_data_type(return_type.get_upsampled_type(upsample_factor));
}

e_ast_data_mutability c_ast_node_module_call::get_dependent_constant_data_mutability() const {
	return m_dependent_constant_data_mutability;
}

uint32 c_ast_node_module_call::get_upsample_factor() const {
	return m_upsample_factor;
}

c_ast_node_expression *c_ast_node_module_call::get_resolved_module_argument_expression(size_t argument_index) const {
	wl_assert(m_resolved_module_declaration);
	// $PERF could pre-compute this

	c_ast_node_module_declaration_argument *argument = m_resolved_module_declaration->get_argument(argument_index);
	for (size_t call_argument_index = 0; call_argument_index < m_arguments.size(); call_argument_index++) {
		c_ast_node_module_call_argument *call_argument = m_arguments[call_argument_index].get();
		if (!call_argument->get_name()) {
			// Unnamed arguments are ordered
			if (argument_index == call_argument_index) {
				return call_argument->get_value_expression();
			}
		} else if (strcmp(call_argument->get_name(), argument->get_name()) == 0) {
			return call_argument->get_value_expression();
		}
	}

	// No call argument was provided so the default expression is used
	wl_assert(argument->get_initialization_expression());
	return argument->get_initialization_expression();
}

c_ast_qualified_data_type c_ast_node_module_call::get_resolved_module_argument_data_type(size_t argument_index) const {
	wl_assert(m_resolved_module_declaration);

	c_ast_node_module_declaration_argument *argument = m_resolved_module_declaration->get_argument(argument_index);
	c_ast_qualified_data_type data_type = argument->get_data_type();
	if (data_type.get_data_mutability() == e_ast_data_mutability::k_dependent_constant) {
		data_type = data_type.change_data_mutability(m_dependent_constant_data_mutability);
	}

	return data_type.get_upsampled_type(m_upsample_factor);
}

c_ast_node *c_ast_node_module_call::copy_internal() const {
	c_ast_node_module_call *node_copy = new c_ast_node_module_call();
	node_copy->set_data_type(get_data_type());
	node_copy->m_arguments.reserve(m_arguments.size());
	for (const std::unique_ptr<c_ast_node_module_call_argument> &argument : m_arguments) {
		node_copy->m_arguments.emplace_back(argument->copy());
	}
	node_copy->m_resolved_module_declaration = m_resolved_module_declaration;
	node_copy->m_dependent_constant_data_mutability = m_dependent_constant_data_mutability;

	return node_copy;
}
