#include "compiler/ast/node_module_call.h"
#include "compiler/ast/node_module_declaration.h"

enum class e_module_argument_match_type {
	k_mismatch,			// One or more argument types doesn't match the native module's declared arguments
	k_inexact_match,	// The argument types match but some were coerced or default argument expressions were used
	k_exact_match,		// Each provided argument type matches exactly and no default arguments were used

	k_count
};

struct s_argument_info {
	e_AST_argument_direction direction;
	c_AST_qualified_data_type data_type;	// Invalid if default expression was used
	c_AST_node_expression *expression;		// Only gets filled out in construct_argument_list
};

static e_module_argument_match_type match_module_arguments(
	const c_AST_node_module_declaration *module_declaration,
	c_wrapped_array<const s_argument_info> provided_arguments);

static void construct_canonical_argument_list(
	const c_AST_node_module_declaration *module_declaration,
	e_AST_data_mutability replace_dependent_constant_with,
	std::vector<s_argument_info> &canonical_argument_list_out);

static s_module_call_resolution_result construct_argument_list(
	const c_AST_node_module_declaration *module_declaration,
	const c_AST_node_module_call *module_call,
	std::vector<s_argument_info> &argument_list_out);

c_AST_node_module_declaration::c_AST_node_module_declaration()
	: c_AST_node_declaration(k_ast_node_type) {}

void c_AST_node_module_declaration::add_argument(const c_AST_node_module_declaration_argument *argument) {
	m_arguments.emplace_back(argument);
}

size_t c_AST_node_module_declaration::get_argument_count() const {
	return m_arguments.size();
}

c_AST_node_module_declaration_argument *c_AST_node_module_declaration::get_argument(size_t index) const {
	return m_arguments[index].get();
}

const c_AST_qualified_data_type &c_AST_node_module_declaration::get_return_type() const {
	return m_return_type;
}

void c_AST_node_module_declaration::set_return_type(const c_AST_qualified_data_type &return_type) {
	m_return_type = return_type;
}

bool c_AST_node_module_declaration::is_dependent_const() const {
	for (size_t argument_index = 0; argument_index < m_arguments.size(); argument_index++) {
		e_AST_data_mutability data_mutability = m_arguments[argument_index]->get_data_type().get_data_mutability();
		if (data_mutability == e_AST_data_mutability::k_dependent_constant) {
			return true;
		}
	}

	return m_return_type.get_data_mutability() == e_AST_data_mutability::k_dependent_constant;
}

c_AST_node_scope *c_AST_node_module_declaration::get_body_scope() const {
	return m_body_scope.get();
}

void c_AST_node_module_declaration::set_body_scope(c_AST_node_scope *body_scope) {
	wl_assert(m_native_module_uid == s_native_module_uid::k_invalid);
	m_body_scope.reset(body_scope);
}

const s_native_module_uid &c_AST_node_module_declaration::get_native_module_uid() const {
	return m_native_module_uid;
}

void c_AST_node_module_declaration::set_native_module_uid(const s_native_module_uid &native_module_uid) {
	wl_assert(!m_body_scope);
	m_native_module_uid = native_module_uid;
}

bool do_module_overloads_conflict(
	const c_AST_node_module_declaration *module_a,
	const c_AST_node_module_declaration *module_b) {
	// Sanity check - if they're overloads, they have the same name
	wl_assert(strcmp(module_a->get_name(), module_b->get_name()) == 0);

	// Two modules conflict if it would be impossible to distinguish between them at the callsite (not taking argument
	// names into consideration). To determine if this is the case, we consider a set of arguments passed to each module
	// if no default arguments are used and the exact types are specified (e.g. real rather than const real). We call
	// this the canonical argument list. The canonical argument list will always be an exact match to that module's
	// signature, whereas a non-canonical argument list is an inexact match. For two modules A and B, if module A's
	// canonical argument list is an exact match to module B or if module B's canonical argument list is an exact match
	// to module A, we consider these modules to be in conflict because it is impossible to add any more detail to
	// resolve the ambiguity. Note that a module A_d with dependent-const arguments actually defines two additional
	// "hidden" modules, A_v which takes variable arguments and A_c which takes constant arguments, so we must check all
	// three (A, A_v, A_c) canonical argument lists against module B to ensure that modules A and B don't conflict.

	// When two modules don't conflict, at the call site we may need to specify more explicit arguments to avoid
	// ambiguity. Examples include:
	// foo(in real x, in bool y = true), foo(in real x, in real y = 0) requires specifying all arguments at the
	// call site.
	// foo(in real x, in const real y), foo(in const real x, in real y) requires exact input types and will not
	// resolve properly if both x and y are const.

	// Construct canonical argument lists for both modules and test against the other module. We need to construct two
	// canonical argument lists if the module is dependent-const, as described above.
	std::vector<s_argument_info> canonical_argument_list;

	static constexpr e_AST_data_mutability k_dependent_constant_replacements[] = {
		e_AST_data_mutability::k_dependent_constant,
		e_AST_data_mutability::k_variable,
		e_AST_data_mutability::k_constant
	};

	for (e_AST_data_mutability dependent_constant_replacement : k_dependent_constant_replacements) {
		construct_canonical_argument_list(module_a, dependent_constant_replacement, canonical_argument_list);
		e_module_argument_match_type match_type = match_module_arguments(
			module_b,
			c_wrapped_array<const s_argument_info>(canonical_argument_list));
		if (match_type == e_module_argument_match_type::k_exact_match) {
			return true;
		}

		construct_canonical_argument_list(module_b, dependent_constant_replacement, canonical_argument_list);
		e_module_argument_match_type match_type = match_module_arguments(
			module_a,
			c_wrapped_array<const s_argument_info>(canonical_argument_list));
		if (match_type == e_module_argument_match_type::k_exact_match) {
			return true;
		}
	}

	return false;
}

s_module_call_resolution_result resolve_module_call(
	const c_AST_node_module_call *module_call,
	c_wrapped_array<const c_AST_node_module_declaration *const> module_declaration_candidates) {
	wl_assert(module_declaration_candidates.get_count() > 0);

	std::vector<s_argument_info> argument_list;
	if (module_declaration_candidates.get_count() == 1) {
		return construct_argument_list(
			module_declaration_candidates[0],
			module_call,
			argument_list);
	} else {
		s_module_call_resolution_result result;

		static constexpr size_t k_invalid_module_index = static_cast<size_t>(-1);
		result.selected_module_index = k_invalid_module_index;

		static constexpr size_t k_invalid_argument_index = static_cast<size_t>(-1);
		result.declaration_argument_index = k_invalid_argument_index;
		result.call_argument_index = k_invalid_argument_index;

		size_t inexact_match_count = 0;
		size_t exact_match_count = 0;
		for (size_t index = 0; index < module_declaration_candidates.get_count(); index++) {
			s_module_call_resolution_result argument_list_result = construct_argument_list(
				module_declaration_candidates[0],
				module_call,
				argument_list);
			if (argument_list_result.result != e_module_call_resolution_result::k_success) {
				continue;
			}

			e_module_argument_match_type match_type = match_module_arguments(
				module_declaration_candidates[index],
				argument_list);
			switch (match_type) {
			case e_module_argument_match_type::k_mismatch:
				continue;

			case e_module_argument_match_type::k_inexact_match:
				inexact_match_count++;
				if (exact_match_count == 0) {
					result.selected_module_index = index;
				}
				break;

			case e_module_argument_match_type::k_exact_match:
				exact_match_count++;
				result.selected_module_index = index;
				break;

			default:
				wl_unreachable();
			}
		}

		// If there's a single exact match or no exact matches but a single inexact match, we're successful. Otherwise
		// the resolution failed.
		if (exact_match_count == 1) {
			result.result = e_module_call_resolution_result::k_success;
		} else if (exact_match_count > 1) {
			// If this occurs, it means the modules themselves conflict so we'll have previously thrown that error
			result.result = e_module_call_resolution_result::k_multiple_matching_modules;
		} else if (inexact_match_count == 1) {
			result.result = e_module_call_resolution_result::k_success;
		} else if (inexact_match_count > 1) {
			result.result = e_module_call_resolution_result::k_multiple_matching_modules;
		} else {
			result.result = e_module_call_resolution_result::k_no_matching_modules;
		}

		if (result.result != e_module_call_resolution_result::k_success) {
			result.selected_module_index = k_invalid_module_index;
		}

		return result;
	}
}

static e_module_argument_match_type match_module_arguments(
	const c_AST_node_module_declaration *module_declaration,
	c_wrapped_array<const s_argument_info> provided_arguments) {
	wl_assert(provided_arguments.get_count() == module_declaration->get_argument_count());

	bool any_dependent_constant_dependent_constant_arguments = false;
	bool any_constant_dependent_constant_arguments = false;
	bool any_variable_dependent_constant_arguments = false;
	e_module_argument_match_type match_type = e_module_argument_match_type::k_exact_match;
	for (size_t argument_index = 0; argument_index < module_declaration->get_argument_count(); argument_index++) {
		const c_AST_node_module_declaration_argument *module_argument =
			module_declaration->get_argument(argument_index);
		const s_argument_info &provided_argument = provided_arguments[argument_index];

		if (!provided_argument.data_type.is_valid()) {
			// An invalid type means this argument has a default initializer
			wl_assert(module_argument->get_initialization_expression());
			match_type = e_module_argument_match_type::k_inexact_match;
			continue;
		}

		if (provided_argument.direction != module_argument->get_argument_direction()) {
			match_type = e_module_argument_match_type::k_mismatch;
			break;
		}

		c_AST_qualified_data_type argument_data_type = module_argument->get_data_type();
		if (argument_data_type.get_data_mutability() != e_AST_data_mutability::k_dependent_constant) {
			if (argument_data_type == provided_argument.data_type) {
				// Exact match
			} else {
				if (module_argument->get_argument_direction() == e_AST_argument_direction::k_in
					&& is_ast_data_type_assignable(provided_argument.data_type, argument_data_type)) {
					// For inputs, the provided value gets assigned to the argument value
					match_type = e_module_argument_match_type::k_inexact_match;
				} else if (module_argument->get_argument_direction() == e_AST_argument_direction::k_out
					&& is_ast_data_type_assignable(argument_data_type, provided_argument.data_type)) {
					// For outputs, the argument value gets assigned to the provided value
					match_type = e_module_argument_match_type::k_inexact_match;
				} else {
					match_type = e_module_argument_match_type::k_mismatch;
					break;
				}
			}
		} else {
			e_AST_data_mutability provided_argument_data_mutability = provided_argument.data_type.get_data_mutability();
			any_variable_dependent_constant_arguments |=
				provided_argument_data_mutability == e_AST_data_mutability::k_variable;
			any_constant_dependent_constant_arguments |=
				provided_argument_data_mutability == e_AST_data_mutability::k_constant;
			any_dependent_constant_dependent_constant_arguments |=
				provided_argument_data_mutability == e_AST_data_mutability::k_dependent_constant;

			if (module_argument->get_argument_direction() == e_AST_argument_direction::k_in
				&& is_ast_data_type_assignable(provided_argument.data_type, argument_data_type)) {
				// For inputs, the provided value gets assigned to the argument value
				// This is considered an exact match as long as all dependent-constants have the same mutability
			} else if (module_argument->get_argument_direction() == e_AST_argument_direction::k_out
				&& is_ast_data_type_assignable(argument_data_type, provided_argument.data_type)) {
				// For outputs, the argument value gets assigned to the provided value
				// This is considered an exact match as long as all dependent-constants have the same mutability
			} else {
				match_type = e_module_argument_match_type::k_mismatch;
				break;
			}
		}
	}

	if (match_type == e_module_argument_match_type::k_exact_match) {
		// If a module is dependent-constant, there are really three modules, one with variable inputs/outputs, one with
		// constant inputs/outputs, and one with dependent-constant inputs/outputs. If we provided variables of
		// different data mutability types to different arguments, then none of those "true" underlying modules got
		// matched exactly, so this is not an exact match.
		int32 dependent_constant_type_count =
			any_variable_dependent_constant_arguments
			+ any_constant_dependent_constant_arguments
			+ any_dependent_constant_dependent_constant_arguments;
		if (dependent_constant_type_count > 1) {
			match_type = e_module_argument_match_type::k_inexact_match;
		}
	}

	return match_type;
}

static void construct_canonical_argument_list(
	const c_AST_node_module_declaration *module_declaration,
	e_AST_data_mutability replace_dependent_constant_with,
	std::vector<s_argument_info> &canonical_argument_list_out) {
	canonical_argument_list_out.clear();
	for (size_t argument_index = 0; argument_index < module_declaration->get_argument_count(); argument_index++) {
		const c_AST_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
		s_argument_info argument_direction_and_type;
		argument_direction_and_type.direction = argument->get_argument_direction();
		argument_direction_and_type.data_type = argument->get_data_type();

		e_AST_data_mutability data_mutability = argument_direction_and_type.data_type.get_data_mutability();
		if (data_mutability == e_AST_data_mutability::k_dependent_constant) {
			argument_direction_and_type.data_type = c_AST_qualified_data_type(
				argument_direction_and_type.data_type.get_data_type(),
				replace_dependent_constant_with);
		}

		canonical_argument_list_out.push_back(argument_direction_and_type);
	}
}

static s_module_call_resolution_result construct_argument_list(
	const c_AST_node_module_declaration *module_declaration,
	const c_AST_node_module_call *module_call,
	std::vector<s_argument_info> &argument_list_out) {
	s_module_call_resolution_result result;
	result.selected_module_index = static_cast<size_t>(-1); // Unused, there's only one

	static constexpr size_t k_invalid_argument_index = static_cast<size_t>(-1);
	result.declaration_argument_index = k_invalid_argument_index;
	result.call_argument_index = k_invalid_argument_index;

	argument_list_out.clear();
	argument_list_out.resize(
		module_declaration->get_argument_count(),
		{ e_AST_argument_direction::k_invalid, c_AST_qualified_data_type(), nullptr });


	IF_ASSERTS_ENABLED(bool any_named_arguments = false;)
		for (size_t call_argument_index = 0;
			call_argument_index < module_call->get_argument_count();
			call_argument_index++) {
		const c_AST_node_module_call_argument *call_argument = module_call->get_argument(call_argument_index);

		size_t matched_argument_index = k_invalid_argument_index;
		if (call_argument->get_name()) {
			IF_ASSERTS_ENABLED(any_named_arguments = true;)

				for (size_t declaration_argument_index = 0;
					declaration_argument_index < module_declaration->get_argument_count();
					declaration_argument_index++) {
				const c_AST_node_module_declaration_argument *declaration_argument =
					module_declaration->get_argument(declaration_argument_index);

				if (strcmp(call_argument->get_name(), declaration_argument->get_name()) == 0) {
					matched_argument_index = declaration_argument_index;
					break;
				}
			}

			if (!matched_argument_index == k_invalid_argument_index) {
				// This named argument didn't match one of the arguments in the declaration
				result.result = e_module_call_resolution_result::k_invalid_named_argument;
				result.call_argument_index = call_argument_index;
				return result;
			}
		} else {
			// The AST builder should validate that named arguments always come last
			wl_assert(!any_named_arguments);

			if (call_argument_index >= module_declaration->get_argument_count()) {
				// Too many arguments provided
				result.result = e_module_call_resolution_result::k_too_many_arguments_provided;
				return result;
			}
		}

		wl_assert(matched_argument_index != k_invalid_argument_index);
		if (argument_list_out[matched_argument_index].direction != e_AST_argument_direction::k_invalid) {
			// Argument provided multiple times
			result.result = e_module_call_resolution_result::k_argument_provided_multiple_times;
			result.declaration_argument_index = matched_argument_index;
			result.call_argument_index = call_argument_index;
			return result;
		}

		const c_AST_node_module_declaration_argument *declaration_argument =
			module_declaration->get_argument(matched_argument_index);
		if (call_argument->get_argument_direction() != declaration_argument->get_argument_direction()) {
			// Mismatched argument direction
			result.result = e_module_call_resolution_result::k_argument_direction_mismatch;
			result.declaration_argument_index = matched_argument_index;
			result.call_argument_index = call_argument_index;
			return result;
		}

		argument_list_out[matched_argument_index].direction = call_argument->get_argument_direction();
		argument_list_out[matched_argument_index].data_type = call_argument->get_value_expression()->get_data_type();
		argument_list_out[matched_argument_index].expression = call_argument->get_value_expression();
	}

	for (size_t declaration_argument_index = 0;
		declaration_argument_index < module_declaration->get_argument_count();
		declaration_argument_index++) {
		if (argument_list_out[declaration_argument_index].direction == e_AST_argument_direction::k_invalid) {
			c_AST_node_expression *initialization_expression =
				module_declaration->get_argument(declaration_argument_index)->get_initialization_expression();
			if (initialization_expression) {
				argument_list_out[declaration_argument_index].expression = initialization_expression;
			} else {
				result.result = e_module_call_resolution_result::k_missing_argument;
				result.declaration_argument_index = declaration_argument_index;
				return result;
			}
		}
	}

	// We're not responsible for checking types in this function - as long as all arguments were provided and their
	// directions match, we're successful.
	result.result = e_module_call_resolution_result::k_success;
	return result;
}

void get_argument_expressions(
	const c_AST_node_module_declaration *module_declaration,
	const c_AST_node_module_call *module_call,
	std::vector<c_AST_node_expression *> argument_expressions_out) {
	std::vector<s_argument_info> argument_list;
	IF_ASSERTS_ENABLED(s_module_call_resolution_result result = ) construct_argument_list(
		module_declaration,
		module_call,
		argument_list);
	wl_assert(result.result == e_module_call_resolution_result::k_success);

	argument_expressions_out.resize(argument_list.size());
	for (size_t index = 0; index < argument_list.size(); index++) {
		wl_assert(argument_list[index].expression);
		argument_expressions_out[index] = argument_list[index].expression;
	}
}
