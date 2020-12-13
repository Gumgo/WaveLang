#pragma once

#include "common/common.h"

#include "compiler/ast/data_type.h"
#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_module_declaration_argument.h"
#include "compiler/ast/node_scope.h"

#include "native_module/native_module.h"

#include <memory>
#include <vector>

class c_ast_node_module_call;

class c_ast_node_module_declaration : public c_ast_node_declaration {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_module_declaration, k_module_declaration, "module");
	c_ast_node_module_declaration();

	void add_argument(c_ast_node_module_declaration_argument *argument);
	size_t get_argument_count() const;
	c_ast_node_module_declaration_argument *get_argument(size_t index) const;

	const c_ast_qualified_data_type &get_return_type() const;
	void set_return_type(const c_ast_qualified_data_type &return_type);

	// Returns true if any arguments or the return type are dependent-const
	bool is_dependent_const() const;

	c_ast_node_scope *get_body_scope() const;
	void set_body_scope(c_ast_node_scope *body_scope);

	const s_native_module_uid &get_native_module_uid() const;
	void set_native_module_uid(const s_native_module_uid &native_module_uid);

protected:
	c_ast_node *copy_internal() const override;

private:
	std::vector<std::unique_ptr<c_ast_node_module_declaration_argument>> m_arguments;
	c_ast_qualified_data_type m_return_type;
	std::unique_ptr<c_ast_node_scope> m_body_scope;
	s_native_module_uid m_native_module_uid = s_native_module_uid::k_invalid;
};

enum class e_module_call_resolution_result {
	k_success,
	k_invalid_named_argument,
	k_too_many_arguments_provided,
	k_argument_provided_multiple_times,
	k_argument_direction_mismatch,
	k_missing_argument,
	k_multiple_matching_modules,
	k_no_matching_modules,

	k_count
};

struct s_module_call_resolution_result {
	e_module_call_resolution_result result;

	// On success, this contains the index of the module chosen from the candidate list
	size_t selected_module_index;

	// Some error results contain additional info about which arguments caused the issue
	size_t declaration_argument_index;
	size_t call_argument_index;
};

bool do_module_overloads_conflict(
	const c_ast_node_module_declaration *module_a,
	const c_ast_node_module_declaration *module_b);

// Determines which module in the list to call, if any. The list can consist of a single module. Note: this function
// does not validate that compatible types are provided for all arguments, this should be done after a successful call.
s_module_call_resolution_result resolve_module_call(
	const c_ast_node_module_call *module_call,
	c_wrapped_array<const c_ast_node_module_declaration *const> module_declaration_candidates);

// Matches each argument with the expression being passed to that argument. Note that resolve_module_call must have been
// called successfully as a prerequisite to calling this function.
void get_argument_expressions(
	const c_ast_node_module_declaration *module_declaration,
	const c_ast_node_module_call *module_call,
	std::vector<c_ast_node_expression *> argument_expressions_out);
