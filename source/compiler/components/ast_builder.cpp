#include "compiler/ast/nodes.h"
#include "compiler/components/ast_builder.h"
#include "compiler/tracked_scope.h"

#include "generated/wavelang_grammar.h"

#include <memory>
#include <vector>

enum class e_ast_builder_pass {
	k_declarations,	// Scrape the declarations first
	k_definitions,	// Build up definitions second so that we have access to all declarations

	k_count
};

class c_ast_builder_visitor : public c_wavelang_lr_parse_tree_visitor {
public:
	c_ast_builder_visitor(
		c_compiler_context &context,
		h_compiler_source_file source_file_handle,
		e_ast_builder_pass pass);

protected:
	bool enter_import_list() override { return false; }
	bool enter_instrument_global(const s_token &command) override { return false; }

	bool enter_global_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_ast_node_scope *&item_list) override;
	void exit_global_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_ast_node_scope *&item_list) override;
	bool enter_global_scope_item_list(
		c_ast_node_scope *&rule_head_context,
		c_ast_node_scope *&list_child,
		c_temporary_reference<c_ast_node_scope_item> &item) override;
	void exit_global_scope_item_list(
		c_ast_node_scope *&rule_head_context,
		c_ast_node_scope *&list_child,
		c_temporary_reference<c_ast_node_scope_item> &item) override;
	bool enter_global_scope_item_value_declaration(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		e_ast_visibility &visibility,
		c_temporary_reference<c_ast_node_value_declaration> &value_declaration) override;
	void exit_global_scope_item_value_declaration(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		e_ast_visibility &visibility,
		c_temporary_reference<c_ast_node_value_declaration> &value_declaration) override;
	bool enter_global_scope_item_module_declaration(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		e_ast_visibility &visibility,
		c_temporary_reference<c_ast_node_module_declaration> &module_declaration) override;
	void exit_global_scope_item_module_declaration(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		e_ast_visibility &visibility,
		c_temporary_reference<c_ast_node_module_declaration> &module_declaration) override;
	void exit_default_visibility(
		e_ast_visibility &rule_head_context) override;
	void exit_visibility_specifier(
		e_ast_visibility &rule_head_context,
		const s_token &visibility) override;

	bool enter_forward_value_declaration(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration) override;
	void exit_forward_value_declaration(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration) override;
	bool enter_value_declaration_with_initialization(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_value_declaration_with_initialization(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	bool enter_value_declaration_without_initialization(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration) override;
	void exit_value_declaration_without_initialization(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &declaration) override;
	void exit_value_declaration_data_type_name(
		c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
		s_value_with_source_location<c_ast_qualified_data_type> &data_type,
		const s_token &name) override;

	bool enter_module_declaration(
		c_temporary_reference<c_ast_node_module_declaration> &rule_head_context,
		s_value_with_source_location<c_ast_qualified_data_type> &return_type,
		const s_token &name,
		c_ast_node_module_declaration *&argument_list,
		c_ast_node_module_declaration *&body) override;
	void exit_module_declaration(
		c_temporary_reference<c_ast_node_module_declaration> &rule_head_context,
		s_value_with_source_location<c_ast_qualified_data_type> &return_type,
		const s_token &name,
		c_ast_node_module_declaration *&argument_list,
		c_ast_node_module_declaration *&body) override;
	bool enter_module_declaration_argument_list(
		c_ast_node_module_declaration *&rule_head_context,
		c_ast_node_module_declaration *&argument_list) override;
	void exit_module_declaration_argument_list(
		c_ast_node_module_declaration *&rule_head_context,
		c_ast_node_module_declaration *&argument_list) override;
	bool enter_module_declaration_single_item_argument_list_item(
		c_ast_node_module_declaration *&rule_head_context,
		c_temporary_reference<c_ast_node_module_declaration_argument> &argument) override;
	void exit_module_declaration_single_item_argument_list_item(
		c_ast_node_module_declaration *&rule_head_context,
		c_temporary_reference<c_ast_node_module_declaration_argument> &argument) override;
	bool enter_module_declaration_nonempty_argument_list_item(
		c_ast_node_module_declaration *&rule_head_context,
		c_ast_node_module_declaration *&list_child,
		c_temporary_reference<c_ast_node_module_declaration_argument> &argument) override;
	void exit_module_declaration_nonempty_argument_list_item(
		c_ast_node_module_declaration *&rule_head_context,
		c_ast_node_module_declaration *&list_child,
		c_temporary_reference<c_ast_node_module_declaration_argument> &argument) override;
	void exit_module_declaration_argument(
		c_temporary_reference<c_ast_node_module_declaration_argument> &rule_head_context,
		s_value_with_source_location<e_ast_argument_direction> &argument_direction,
		c_temporary_reference<c_ast_node_value_declaration> &value_declaration) override;
	void exit_argument_qualifier(
		s_value_with_source_location<e_ast_argument_direction> &rule_head_context,
		const s_token &direction) override;
	bool enter_module_body(
		c_ast_node_module_declaration *&rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_module_body(
		c_ast_node_module_declaration *&rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;

	void exit_qualified_data_type(
		s_value_with_source_location<c_ast_qualified_data_type> &rule_head_context,
		s_value_with_source_location<e_ast_data_mutability> &mutability,
		s_value_with_source_location<c_ast_data_type> &data_type) override;
	void exit_no_qualifier(
		s_value_with_source_location<e_ast_data_mutability> &rule_head_context) override;
	void exit_const_qualifier(
		s_value_with_source_location<e_ast_data_mutability> &rule_head_context,
		const s_token &const_keyword) override;
	void exit_dependent_const_qualifier(
		s_value_with_source_location<e_ast_data_mutability> &rule_head_context,
		const s_token &const_keyword) override;
	void exit_data_type(
		s_value_with_source_location<c_ast_data_type> &rule_head_context,
		s_value_with_source_location<e_ast_primitive_type> &primitive_type,
		bool &is_array) override;
	void exit_primitive_type(
		s_value_with_source_location<e_ast_primitive_type> &rule_head_context,
		const s_token &type) override;
	void exit_is_not_array(
		bool &rule_head_context) override;
	void exit_is_array(
		bool &rule_head_context) override;

	bool enter_expression(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_expression(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_binary_operator(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &lhs,
		const s_token &op,
		c_temporary_reference<c_ast_node_expression> &rhs) override;
	void exit_forward_expression(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_unary_operator(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		const s_token &op,
		c_temporary_reference<c_ast_node_expression> &rhs) override;
	bool enter_module_call(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &module,
		c_ast_node_module_call *&argument_list) override;
	void exit_module_call(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &module,
		c_ast_node_module_call *&argument_list) override;
	void exit_convert(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		s_value_with_source_location<c_ast_qualified_data_type> &data_type,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_access(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &lhs,
		const s_token &identifier) override;
	void exit_identifier(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		const s_token &identifier) override;
	void exit_literal(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		const s_token &literal) override;
	bool enter_array(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		const s_token &brace,
		c_ast_node_array *&values) override;
	void exit_array(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		const s_token &brace,
		c_ast_node_array *&values) override;
	bool enter_array_value_list(
		c_ast_node_array *&rule_head_context,
		c_ast_node_array *&value_list) override;
	void exit_array_single_item_value_list_item(
		c_ast_node_array *&rule_head_context,
		c_temporary_reference<c_ast_node_expression> &value) override;
	bool enter_array_nonempty_value_list_item(
		c_ast_node_array *&rule_head_context,
		c_ast_node_array *&list_child,
		c_temporary_reference<c_ast_node_expression> &value) override;
	void exit_array_nonempty_value_list_item(
		c_ast_node_array *&rule_head_context,
		c_ast_node_array *&list_child,
		c_temporary_reference<c_ast_node_expression> &value) override;
	bool enter_module_call_argument_list(
		c_ast_node_module_call *&rule_head_context,
		c_ast_node_module_call *&argument_list) override;
	void exit_module_call_single_item_argument_list_item(
		c_ast_node_module_call *&rule_head_context,
		c_temporary_reference<c_ast_node_module_call_argument> &argument) override;
	bool enter_module_call_nonempty_argument_list_item(
		c_ast_node_module_call *&rule_head_context,
		c_ast_node_module_call *&list_child,
		c_temporary_reference<c_ast_node_module_call_argument> &argument) override;
	void exit_module_call_nonempty_argument_list_item(
		c_ast_node_module_call *&rule_head_context,
		c_ast_node_module_call *&list_child,
		c_temporary_reference<c_ast_node_module_call_argument> &argument) override;
	void exit_module_call_unnamed_argument(
		c_temporary_reference<c_ast_node_module_call_argument> &rule_head_context,
		s_value_with_source_location<e_ast_argument_direction> &direction,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_module_call_named_argument(
		c_temporary_reference<c_ast_node_module_call_argument> &rule_head_context,
		s_value_with_source_location<e_ast_argument_direction> &direction,
		const s_token &name,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_in_call_argument_qualifier(
		s_value_with_source_location<e_ast_argument_direction> &rule_head_context) override;
	void exit_out_call_argument_qualifier(
		s_value_with_source_location<e_ast_argument_direction> &rule_head_context,
		const s_token &direction) override;

	bool enter_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_ast_node_scope *&statement_list) override;
	bool enter_scope_item_list(
		c_ast_node_scope *&rule_head_context,
		c_ast_node_scope *&list_child,
		c_temporary_reference<c_ast_node_scope_item> &scope_item) override;
	void exit_scope_item_list(
		c_ast_node_scope *&rule_head_context,
		c_ast_node_scope *&list_child,
		c_temporary_reference<c_ast_node_scope_item> &scope_item) override;
	void exit_scope_item_expression(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_scope_item_value_declaration(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &value_declaration) override;
	void exit_scope_item_assignment(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &lhs,
		s_value_with_source_location<e_native_operator> &assignment_type,
		c_temporary_reference<c_ast_node_expression> &rhs) override;
	void exit_scope_item_return_void_statement(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &return_keyword) override;
	void exit_scope_item_return_statement(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &return_keyword,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	void exit_scope_item_if_statement(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		c_temporary_reference<c_ast_node_if_statement> &if_statement) override;
	bool enter_scope_item_for_loop(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &for_keyword,
		c_ast_node_for_loop *&iterator,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_scope_item_for_loop(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &for_keyword,
		c_ast_node_for_loop *&iterator,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_break_statement(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &break_keyword) override;
	void exit_continue_statement(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &continue_keyword) override;
	bool enter_scope_item_scope(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_scope_item_scope(
		c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_if_statement(
		c_temporary_reference<c_ast_node_if_statement> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression,
		c_temporary_reference<c_ast_node_scope> &true_scope) override;
	void exit_if_else_statement(
		c_temporary_reference<c_ast_node_if_statement> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression,
		c_temporary_reference<c_ast_node_scope> &true_scope,
		c_temporary_reference<c_ast_node_scope> &false_scope) override;
	void exit_if_statement_expression(
		c_temporary_reference<c_ast_node_expression> &rule_head_context,
		c_temporary_reference<c_ast_node_expression> &expression) override;
	bool enter_if_statement_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	void exit_if_statement_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		const s_token &brace,
		c_temporary_reference<c_ast_node_scope> &scope) override;
	bool enter_else_if(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_temporary_reference<c_ast_node_if_statement> &if_statement) override;
	void exit_else_if(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_temporary_reference<c_ast_node_if_statement> &if_statement) override;
	void exit_for_loop_iterator(
		c_ast_node_for_loop *&rule_head_context,
		c_temporary_reference<c_ast_node_value_declaration> &value,
		c_temporary_reference<c_ast_node_expression> &range) override;

	void exit_assignment(
		s_value_with_source_location<e_native_operator> &rule_head_context,
		const s_token &op) override;

private:
	void begin_left_recursive_list(size_t list_size) {
		m_left_recursive_list_stack.push_back(list_size);
	}

	size_t get_and_advance_left_recursive_list_index() {
		return --m_left_recursive_list_stack.back();
	}

	void end_left_recursive_list() {
		wl_assert(m_left_recursive_list_stack.back() == 0);
		m_left_recursive_list_stack.pop_back();
	}

	template<typename t_type>
	t_type *forward_on_definition_pass(t_type *value) const {
		if (m_pass == e_ast_builder_pass::k_declarations) {
			// On the declaration pass, we have nothing to forward because the object hasn't been created yet
			wl_assert(!value);
		} else {
			// On the definition pass, we expect that we're forwarding an object created during the declaration pass
			wl_assert(m_pass == e_ast_builder_pass::k_definitions);
			wl_assert(value);
		}

		return value;
	}

	// Builds the scope and recursively builds namespace scopes as well
	void build_tracked_scope(c_ast_node_scope *scope_node, c_tracked_scope *tracked_scope);

	// Adds a declaration to the specified tracked scope and reports conflict errors
	c_tracked_declaration *add_declaration_to_tracked_scope(
		c_tracked_scope *tracked_scope,
		c_ast_node_declaration *declaration);

	// Attempt to resolve the identifier using the scope provided, storing results in the provided reference node and
	// reporting errors
	void resolve_identifier(
		c_ast_node_declaration_reference *reference_node,
		c_tracked_scope *tracked_scope,
		const char *identifier,
		bool recurse);

	void resolve_module_call(
		c_ast_node_module_call *module_call,
		c_wrapped_array<c_ast_node_module_declaration *> module_candidates);

	void resolve_module_call_argument(
		c_ast_node_module_declaration *module_declaration,
		c_ast_node_module_declaration_argument *argument,
		c_ast_node_module_call *module_call,
		c_ast_node_expression *argument_expression,
		e_ast_data_mutability dependent_constant_output_mutability);

	void resolve_native_operator_call(
		c_ast_node_module_call *module_call,
		e_native_operator native_operator,
		const s_compiler_source_location &operator_source_location);

	c_wrapped_array<c_ast_node_declaration *const> try_get_typed_references(
		const c_ast_node_expression *expression_node,
		e_ast_node_type reference_type,
		e_compiler_error failure_error,
		const char *error_action,
		const char *error_reason);

	void validate_current_module_return_type(
		const c_ast_qualified_data_type &data_type,
		const s_compiler_source_location &source_location);

	bool is_loop_in_scope_stack() const;

	c_compiler_context &m_context;
	h_compiler_source_file m_source_file_handle;
	e_ast_builder_pass m_pass;

	// Used to walk the indices of a left-recursive list. This is used to grab nodes by index that were previously build
	// in the declaration pass.
	std::vector<size_t> m_left_recursive_list_stack;

	// Associates a tracked scope with a scope AST node
	std::unordered_map<c_ast_node_scope *, std::unique_ptr<c_tracked_scope>> m_tracked_scopes_from_scope_nodes;

	// Stack of tracked scopes - global is at the bottom
	std::vector<c_tracked_scope *> m_tracked_scopes;

	// Current module being built. If we ever support nested module declarations, we'll need to turn this into a stack.
	c_ast_node_module_declaration *m_current_module = nullptr;
};

static const char *get_argument_direction_string(e_ast_argument_direction argument_direction);

void c_ast_builder::build_ast_declarations(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	c_ast_builder_visitor visitor(context, source_file_handle, e_ast_builder_pass::k_declarations);
	visitor.visit();
}

void c_ast_builder::build_ast_definitions(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	c_ast_builder_visitor visitor(context, source_file_handle, e_ast_builder_pass::k_definitions);
	visitor.visit();
}

c_ast_builder_visitor::c_ast_builder_visitor(
	c_compiler_context &context,
	h_compiler_source_file source_file_handle,
	e_ast_builder_pass pass)
	: c_wavelang_lr_parse_tree_visitor(
		context.get_source_file(source_file_handle).parse_tree,
		context.get_source_file(source_file_handle).tokens)
	, m_context(context)
	, m_source_file_handle(source_file_handle)
	, m_pass(pass) {}

bool c_ast_builder_visitor::enter_global_scope(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	c_ast_node_scope *&item_list) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Create a new scope node
		rule_head_context.initialize(new c_ast_node_scope());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		// Grab the previously created scope node
		s_compiler_source_file &source_file = m_context.get_source_file(m_source_file_handle);
		rule_head_context.initialize(source_file.ast->get_as<c_ast_node_scope>());
		wl_assert(rule_head_context.get());

		// Create a tracked scope and add all declared items up-front. The global scope is considered a namespace. Note:
		// consider adding a namespace node as the AST root.
		c_tracked_scope *tracked_scope = new c_tracked_scope(nullptr, e_tracked_scope_type::k_namespace, nullptr);
		m_tracked_scopes.push_back(tracked_scope);
		build_tracked_scope(rule_head_context.get(), tracked_scope);

		// Since global scope items use a left-recursive list, we enter item nodes in reverse order (and exit them in
		// forward order). We use this counter in the enter functions, so start at the end of the list.
		begin_left_recursive_list(rule_head_context->get_scope_item_count());
	}

	// Pass along the global scope to each item
	item_list = rule_head_context.get();
	return true;
}

void c_ast_builder_visitor::exit_global_scope(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	c_ast_node_scope *&item_list) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Store the scope node as the AST root
		s_compiler_source_file &source_file = m_context.get_source_file(m_source_file_handle);
		wl_assert(!source_file.ast);
		source_file.ast.reset(rule_head_context.release());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		wl_assert(m_tracked_scopes.size() == 1);
		m_tracked_scopes_from_scope_nodes.erase(rule_head_context.release());
		m_tracked_scopes.pop_back();

		end_left_recursive_list();
	}
}

bool c_ast_builder_visitor::enter_global_scope_item_list(
	c_ast_node_scope *&rule_head_context,
	c_ast_node_scope *&list_child,
	c_temporary_reference<c_ast_node_scope_item> &item) {

	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Nothing to do during the declarations pass
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		// Find the Nth previously declared item and pass it along - since this is a left-recursive list, we enter nodes
		// in reverse order so we index from the end
		size_t item_index = get_and_advance_left_recursive_list_index();
		item.initialize(rule_head_context->get_scope_item(item_index));
	}

	list_child = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_global_scope_item_list(
	c_ast_node_scope *&rule_head_context,
	c_ast_node_scope *&list_child,
	c_temporary_reference<c_ast_node_scope_item> &item) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context->add_scope_item(item.release());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		item.release(); // Declaration was already added in the declaration pass
	}
}

bool c_ast_builder_visitor::enter_global_scope_item_value_declaration(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	e_ast_visibility &visibility,
	c_temporary_reference<c_ast_node_value_declaration> &value_declaration) {
	// Pass along the item
	value_declaration.initialize(
		forward_on_definition_pass(
			get_ast_node_as_or_null<c_ast_node_value_declaration>(rule_head_context.release())));
	return true;
}

void c_ast_builder_visitor::exit_global_scope_item_value_declaration(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	e_ast_visibility &visibility,
	c_temporary_reference<c_ast_node_value_declaration> &value_declaration) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Global scope values can't be changed after they're initialized
		value_declaration->set_modifiable(false);

		if (value_declaration->get_data_type().get_data_mutability() != e_ast_data_mutability::k_constant) {
			m_context.error(
				e_compiler_error::k_illegal_global_scope_value_data_type,
				value_declaration->get_source_location(),
				"Value '%s' declared at global scope is not const",
				value_declaration->get_name());
		}

		if (!value_declaration->get_initialization_expression()) {
			m_context.error(
				e_compiler_error::k_missing_global_scope_value_initializer,
				value_declaration->get_source_location(),
				"Value '%s' declared at global scope is not initialized",
				value_declaration->get_name());
		}
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
	}

	// Pass the item up to the generic parent
	rule_head_context.initialize(value_declaration.release());

	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context->set_visibility(visibility);
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		wl_assert(rule_head_context->get_visibility() == visibility);
	}
}

bool c_ast_builder_visitor::enter_global_scope_item_module_declaration(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	e_ast_visibility &visibility,
	c_temporary_reference<c_ast_node_module_declaration> &module_declaration) {
	// Pass along the item
	module_declaration.initialize(
		forward_on_definition_pass(
			get_ast_node_as_or_null<c_ast_node_module_declaration>(rule_head_context.release())));
	return true;
}

void c_ast_builder_visitor::exit_global_scope_item_module_declaration(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	e_ast_visibility &visibility,
	c_temporary_reference<c_ast_node_module_declaration> &module_declaration) {
	// Pass the item up to the generic parent
	rule_head_context.initialize(module_declaration.release());

	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context->set_visibility(visibility);
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		wl_assert(rule_head_context->get_visibility() == visibility);
	}
}

void c_ast_builder_visitor::exit_default_visibility(
	e_ast_visibility &rule_head_context) {
	rule_head_context = e_ast_visibility::k_default;
}

void c_ast_builder_visitor::exit_visibility_specifier(
	e_ast_visibility &rule_head_context,
	const s_token &visibility) {
	switch (visibility.token_type) {
	case e_token_type::k_keyword_private:
		rule_head_context = e_ast_visibility::k_private;
		break;

	case e_token_type::k_keyword_public:
		rule_head_context = e_ast_visibility::k_public;
		break;

	default:
		wl_unreachable();
	}
}

bool c_ast_builder_visitor::enter_forward_value_declaration(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration) {
	declaration.initialize(forward_on_definition_pass(rule_head_context.release()));
	return true;
}

void c_ast_builder_visitor::exit_forward_value_declaration(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration) {
	rule_head_context.initialize(declaration.release());
}

bool c_ast_builder_visitor::enter_value_declaration_with_initialization(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration,
	c_temporary_reference<c_ast_node_expression> &expression) {
	declaration.initialize(forward_on_definition_pass(rule_head_context.release()));
	return true;
}

void c_ast_builder_visitor::exit_value_declaration_with_initialization(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration,
	c_temporary_reference<c_ast_node_expression> &expression) {
	rule_head_context.initialize(declaration.release());

	if (m_pass == e_ast_builder_pass::k_declarations) {
		wl_assert(expression->is_type(e_ast_node_type::k_expression_placeholder));
		rule_head_context->set_initialization_expression(expression.release());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		// Make sure the default initializer is of a compatible type
		const c_ast_qualified_data_type &declaration_data_type = rule_head_context->get_data_type();
		const c_ast_qualified_data_type &initialization_expression_data_type =
			rule_head_context->get_initialization_expression()->get_data_type();
		if (!is_ast_data_type_assignable(initialization_expression_data_type, declaration_data_type)) {
			m_context.error(
				e_compiler_error::k_type_mismatch,
				rule_head_context->get_initialization_expression()->get_source_location(),
				"Cannot initialize value '%s' of type '%s' with a value of type '%s'",
				rule_head_context->get_name(),
				declaration_data_type.to_string().c_str(),
				initialization_expression_data_type.to_string().c_str());
		}

		// Replace the placeholder expression with the real one
		wl_assert(
			rule_head_context->get_initialization_expression()->is_type(e_ast_node_type::k_expression_placeholder));
		rule_head_context->set_initialization_expression(expression.release());
	}
}

bool c_ast_builder_visitor::enter_value_declaration_without_initialization(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration) {
	declaration.initialize(forward_on_definition_pass(rule_head_context.release()));
	return true;
}

void c_ast_builder_visitor::exit_value_declaration_without_initialization(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &declaration) {
	rule_head_context.initialize(declaration.release());
}

void c_ast_builder_visitor::exit_value_declaration_data_type_name(
	c_temporary_reference<c_ast_node_value_declaration> &rule_head_context,
	s_value_with_source_location<c_ast_qualified_data_type> &data_type,
	const s_token &name) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context.initialize(new c_ast_node_value_declaration());
		rule_head_context->set_name(std::string(name.token_string).c_str());
		rule_head_context->set_source_location(data_type.source_location);

		if (!data_type.value.get_primitive_type_traits().allows_values) {
			m_context.error(
				e_compiler_error::k_illegal_value_data_type,
				data_type.source_location,
				"Illegal data type '%s' for value '%s'",
				data_type.value.to_string().c_str(),
				std::string(name.token_string).c_str());
			rule_head_context->set_data_type(c_ast_qualified_data_type::error());
		} else {
			rule_head_context->set_data_type(data_type.value);
		}
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		// We should have been provided the declaration from the declarations pass
		wl_assert(rule_head_context.get());
		wl_assert(rule_head_context->get_name() == name.token_string);
	}
}

bool c_ast_builder_visitor::enter_module_declaration(
	c_temporary_reference<c_ast_node_module_declaration> &rule_head_context,
	s_value_with_source_location<c_ast_qualified_data_type> &return_type,
	const s_token &name,
	c_ast_node_module_declaration *&argument_list,
	c_ast_node_module_declaration *&body) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Create a new module declaration node so we have a location to store arguments as they're parsed
		rule_head_context.initialize(new c_ast_node_module_declaration());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		wl_assert(!m_current_module);
		m_current_module = rule_head_context.get();
	}

	argument_list = rule_head_context.get();
	body = rule_head_context.get();
	return true;
}

void c_ast_builder_visitor::exit_module_declaration(
	c_temporary_reference<c_ast_node_module_declaration> &rule_head_context,
	s_value_with_source_location<c_ast_qualified_data_type> &return_type,
	const s_token &name,
	c_ast_node_module_declaration *&argument_list,
	c_ast_node_module_declaration *&body) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Validate all arguments
		size_t argument_count = rule_head_context->get_argument_count();

		// Detect ordering issues (default initializer expressions must come last)
		bool found_argument_with_default_value = false;
		for (size_t argument_index = 0; argument_index < argument_count; argument_index++) {
			const c_ast_node_module_declaration_argument *argument = rule_head_context->get_argument(argument_index);
			if (argument->get_initialization_expression()) {
				found_argument_with_default_value = true;
			} else if (found_argument_with_default_value) {
				m_context.error(
					e_compiler_error::k_illegal_argument_ordering,
					argument->get_source_location(),
					"Argument '%s' in module '%s' must come before all arguments with default values",
					rule_head_context->get_name(),
					argument->get_name());
				break; // Just report the first argument ordering issue
			}
		}

		// Detect duplicate arguments
		for (size_t argument_index_a = 0; argument_index_a < argument_count; argument_index_a++) {
			const c_ast_node_module_declaration_argument *argument_a =
				rule_head_context->get_argument(argument_index_a);

			for (size_t argument_index_b = 0; argument_index_b < argument_index_a; argument_index_b++) {
				const c_ast_node_module_declaration_argument *argument_b =
					rule_head_context->get_argument(argument_index_b);

				if (strcmp(argument_a->get_name(), argument_b->get_name()) == 0) {
					m_context.error(
						e_compiler_error::k_duplicate_argument,
						argument_a->get_source_location(),
						"Multiple arguments in module '%s' share the same name '%s'",
						rule_head_context->get_name(),
						argument_a->get_name());
				}
			}
		}
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		// We should have been provided the declaration from the declarations pass. If a default initializer expression
		// exists, it has already had its type validated.
		wl_assert(rule_head_context.get());
		wl_assert(rule_head_context->get_name() == name.token_string);

		m_current_module = nullptr;
	}
}

bool c_ast_builder_visitor::enter_module_declaration_argument_list(
	c_ast_node_module_declaration *&rule_head_context,
	c_ast_node_module_declaration *&argument_list) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Nothing to do
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		begin_left_recursive_list(rule_head_context->get_argument_count());
	}

	argument_list = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_module_declaration_argument_list(
	c_ast_node_module_declaration *&rule_head_context,
	c_ast_node_module_declaration *&argument_list) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Nothing to do
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		end_left_recursive_list();
	}
}

bool c_ast_builder_visitor::enter_module_declaration_single_item_argument_list_item(
	c_ast_node_module_declaration *&rule_head_context,
	c_temporary_reference<c_ast_node_module_declaration_argument> &argument) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Nothing to do
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		size_t item_index = get_and_advance_left_recursive_list_index();
		argument.initialize(rule_head_context->get_argument(item_index));
	}

	return true;
}

void c_ast_builder_visitor::exit_module_declaration_single_item_argument_list_item(
	c_ast_node_module_declaration *&rule_head_context,
	c_temporary_reference<c_ast_node_module_declaration_argument> &argument) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context->add_argument(argument.release());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		argument.release(); // Argument was already added in the declaration pass
	}
}

bool c_ast_builder_visitor::enter_module_declaration_nonempty_argument_list_item(
	c_ast_node_module_declaration *&rule_head_context,
	c_ast_node_module_declaration *&list_child,
	c_temporary_reference<c_ast_node_module_declaration_argument> &argument) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Nothing to do
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		size_t item_index = get_and_advance_left_recursive_list_index();
		argument.initialize(rule_head_context->get_argument(item_index));
	}

	list_child = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_module_declaration_nonempty_argument_list_item(
	c_ast_node_module_declaration *&rule_head_context,
	c_ast_node_module_declaration *&list_child,
	c_temporary_reference<c_ast_node_module_declaration_argument> &argument) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context->add_argument(argument.release());
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		argument.release(); // Argument was already added in the declaration pass
	}
}

void c_ast_builder_visitor::exit_module_declaration_argument(
	c_temporary_reference<c_ast_node_module_declaration_argument> &rule_head_context,
	s_value_with_source_location<e_ast_argument_direction> &argument_direction,
	c_temporary_reference<c_ast_node_value_declaration> &value_declaration) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		rule_head_context.initialize(new c_ast_node_module_declaration_argument());
		rule_head_context->set_source_location(argument_direction.source_location);
		rule_head_context->set_argument_direction(argument_direction.value);
		rule_head_context->set_value_declaration(value_declaration.release());

		if (argument_direction.value == e_ast_argument_direction::k_out
			&& value_declaration->get_initialization_expression()) {
			m_context.error(
				e_compiler_error::k_illegal_out_argument,
				argument_direction.source_location,
				"Argument '%s' with 'out' qualifier cannot have a default value",
				value_declaration->get_name());
		}
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
	}
}

void c_ast_builder_visitor::exit_argument_qualifier(
	s_value_with_source_location<e_ast_argument_direction> &rule_head_context,
	const s_token &direction) {
	switch (direction.token_type) {
	case e_token_type::k_keyword_in:
		rule_head_context.value = e_ast_argument_direction::k_in;
		break;

	case e_token_type::k_keyword_out:
		rule_head_context.value = e_ast_argument_direction::k_out;
		break;

	default:
		wl_unreachable();
	}

	rule_head_context.source_location = direction.source_location;
}

bool c_ast_builder_visitor::enter_module_body(
	c_ast_node_module_declaration *&rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		return false;
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);

		// Create the scope and add arguments in advance
		scope.initialize(new c_ast_node_scope());
		scope->set_source_location(brace.source_location);

		c_tracked_scope *tracked_scope =
			new c_tracked_scope(m_tracked_scopes.back(), e_tracked_scope_type::k_module, nullptr);
		m_tracked_scopes_from_scope_nodes.insert(std::make_pair(scope.get(), tracked_scope));
		m_tracked_scopes.push_back(tracked_scope);

		for (size_t argument_index = 0; argument_index < rule_head_context->get_argument_count(); argument_index++) {
			c_ast_node_module_declaration_argument *argument = rule_head_context->get_argument(argument_index);
			add_declaration_to_tracked_scope(tracked_scope, argument->get_value_declaration());

			// All input values start initialized
			if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
				m_tracked_scopes.back()->issue_declaration_assignment(argument->get_value_declaration());
			}
		}

		return true;
	}
}

void c_ast_builder_visitor::exit_module_body(
	c_ast_node_module_declaration *&rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	wl_assert(m_pass == e_ast_builder_pass::k_definitions);

	c_tracked_scope *tracked_scope = m_tracked_scopes.back();
	wl_assert(tracked_scope == m_tracked_scopes_from_scope_nodes.find(scope.get())->second.get());

	// Validate that all output arguments have been assigned
	for (size_t argument_index = 0; argument_index < rule_head_context->get_argument_count(); argument_index++) {
		c_ast_node_module_declaration_argument *argument = rule_head_context->get_argument(argument_index);

		if (argument->get_argument_direction() == e_ast_argument_direction::k_out) {
			switch (m_tracked_scopes.back()->get_declaration_assignment_state(argument->get_value_declaration())) {
			case e_tracked_event_state::k_did_not_occur:
				m_context.error(
					e_compiler_error::k_unassigned_out_argument,
					argument->get_source_location(),
					"The out argument '%s' for module '%s' is never assigned a value",
					argument->get_name(),
					rule_head_context->get_name());
				break;

			case e_tracked_event_state::k_maybe_occurred:
				m_context.error(
					e_compiler_error::k_unassigned_out_argument,
					argument->get_source_location(),
					"The out argument '%s' for module '%s' might not be assigned a value",
					argument->get_name(),
					rule_head_context->get_name());
				break;

			case e_tracked_event_state::k_occurred:
				// No issues to report
				break;

			default:
				wl_unreachable();
			}
		}
	}

	// Validate that the module properly returns if we expect it to
	if (!rule_head_context->get_return_type().is_void()) {
		switch (tracked_scope->get_return_state()) {
		case e_tracked_event_state::k_did_not_occur:
			m_context.error(
				e_compiler_error::k_missing_return_statement,
				rule_head_context->get_source_location(),
				"The out module '%s' never returns a value",
				rule_head_context->get_name());
			break;

		case e_tracked_event_state::k_maybe_occurred:
			m_context.error(
				e_compiler_error::k_missing_return_statement,
				rule_head_context->get_source_location(),
				"The module '%s' might not return a value",
				rule_head_context->get_name());
			break;

		case e_tracked_event_state::k_occurred:
			// No issues to report
			break;

		default:
			wl_unreachable();
		}
	}

	m_tracked_scopes.pop_back();
	m_tracked_scopes_from_scope_nodes.erase(scope.get());

	rule_head_context->set_body_scope(scope.release());
}

void c_ast_builder_visitor::exit_qualified_data_type(
	s_value_with_source_location<c_ast_qualified_data_type> &rule_head_context,
	s_value_with_source_location<e_ast_data_mutability> &mutability,
	s_value_with_source_location<c_ast_data_type> &data_type) {
	rule_head_context.value = c_ast_qualified_data_type(data_type.value, mutability.value);
	rule_head_context.source_location = mutability.source_location.is_valid()
		? mutability.source_location
		: data_type.source_location;

	// For convenience, upgrade constant-only types to constants
	if (data_type.value.get_primitive_type_traits().constant_only) {
		mutability.value = e_ast_data_mutability::k_constant;
	}

	if (!rule_head_context.value.is_legal_type_declaration()) {
		m_context.error(
			e_compiler_error::k_illegal_data_type,
			rule_head_context.source_location,
			"Illegal data type '%s'",
			rule_head_context.value.to_string().c_str());
		rule_head_context.value = c_ast_qualified_data_type::error();
	}
}

void c_ast_builder_visitor::exit_no_qualifier(
	s_value_with_source_location<e_ast_data_mutability> &rule_head_context) {
	rule_head_context.value = e_ast_data_mutability::k_variable;
	rule_head_context.source_location.clear();
}

void c_ast_builder_visitor::exit_const_qualifier(
	s_value_with_source_location<e_ast_data_mutability> &rule_head_context,
	const s_token &const_keyword) {
	rule_head_context.value = e_ast_data_mutability::k_constant;
	rule_head_context.source_location = const_keyword.source_location;
}

void c_ast_builder_visitor::exit_dependent_const_qualifier(
	s_value_with_source_location<e_ast_data_mutability> &rule_head_context,
	const s_token &const_keyword) {
	rule_head_context.value = e_ast_data_mutability::k_dependent_constant;
	rule_head_context.source_location = const_keyword.source_location;
}

void c_ast_builder_visitor::exit_data_type(
	s_value_with_source_location<c_ast_data_type> &rule_head_context,
	s_value_with_source_location<e_ast_primitive_type> &primitive_type,
	bool &is_array) {
	rule_head_context.value = c_ast_data_type(primitive_type.value, is_array);
	rule_head_context.source_location = primitive_type.source_location;
	if (!rule_head_context.value.is_legal_type_declaration()) {
		m_context.error(
			e_compiler_error::k_illegal_data_type,
			rule_head_context.source_location,
			"Illegal data type '%s'",
			rule_head_context.value.to_string().c_str());
		rule_head_context.value = c_ast_data_type::error();
	}
}

void c_ast_builder_visitor::exit_primitive_type(
	s_value_with_source_location<e_ast_primitive_type> &rule_head_context,
	const s_token &type) {
	switch (type.token_type) {
	case e_token_type::k_keyword_void:
		rule_head_context.value = e_ast_primitive_type::k_void;
		break;

	case e_token_type::k_keyword_real:
		rule_head_context.value = e_ast_primitive_type::k_real;
		break;

	case e_token_type::k_keyword_bool:
		rule_head_context.value = e_ast_primitive_type::k_bool;
		break;

	case e_token_type::k_keyword_string:
		rule_head_context.value = e_ast_primitive_type::k_string;
		break;

	default:
		wl_unreachable();
	}

	rule_head_context.source_location = type.source_location;
}

void c_ast_builder_visitor::exit_is_not_array(
	bool &rule_head_context) {
	rule_head_context = false;
}

void c_ast_builder_visitor::exit_is_array(
	bool &rule_head_context) {
	rule_head_context = true;
}

bool c_ast_builder_visitor::enter_expression(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression) {
	if (m_pass == e_ast_builder_pass::k_declarations) {
		// Store off a placeholder expression and don't enter the expression tree during the declaration pass
		rule_head_context.initialize(new c_ast_node_expression_placeholder());
		return false;
	} else {
		wl_assert(m_pass == e_ast_builder_pass::k_definitions);
		return true;
	}
}

void c_ast_builder_visitor::exit_expression(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression) {
	wl_assert(m_pass == e_ast_builder_pass::k_definitions);
	rule_head_context.initialize(forward_on_definition_pass(expression.release()));
}

void c_ast_builder_visitor::exit_binary_operator(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &lhs,
	const s_token &op,
	c_temporary_reference<c_ast_node_expression> &rhs) {
	c_ast_node_module_call *module_call_node = new c_ast_node_module_call();
	rule_head_context.initialize(module_call_node);
	module_call_node->set_source_location(op.source_location);

	e_native_operator native_operator = e_native_operator::k_invalid;
	switch (op.token_type) {
	case e_token_type::k_plus:
		native_operator = e_native_operator::k_addition;
		break;

	case e_token_type::k_minus:
		native_operator = e_native_operator::k_subtraction;
		break;

	case e_token_type::k_multiply:
		native_operator = e_native_operator::k_multiplication;
		break;

	case e_token_type::k_divide:
		native_operator = e_native_operator::k_division;
		break;

	case e_token_type::k_modulo:
		native_operator = e_native_operator::k_modulo;
		break;

	case e_token_type::k_equal:
		native_operator = e_native_operator::k_equal;
		break;

	case e_token_type::k_not_equal:
		native_operator = e_native_operator::k_not_equal;
		break;

	case e_token_type::k_less_than:
		native_operator = e_native_operator::k_less;
		break;

	case e_token_type::k_greater_than:
		native_operator = e_native_operator::k_greater;
		break;

	case e_token_type::k_less_than_equal:
		native_operator = e_native_operator::k_less_equal;
		break;

	case e_token_type::k_greater_than_equal:
		native_operator = e_native_operator::k_greater_equal;
		break;

	case e_token_type::k_and:
		native_operator = e_native_operator::k_and;
		break;

	case e_token_type::k_or:
		native_operator = e_native_operator::k_or;
		break;

	case e_token_type::k_left_bracket:
		native_operator = e_native_operator::k_subscript;
		break;
	}

	wl_assert(native_operator != e_native_operator::k_invalid);

	c_ast_node_module_call_argument *lhs_argument_node = new c_ast_node_module_call_argument();
	module_call_node->add_argument(lhs_argument_node);
	lhs_argument_node->set_source_location(lhs->get_source_location());
	lhs_argument_node->set_argument_direction(e_ast_argument_direction::k_in);
	lhs_argument_node->set_value_expression(lhs.release());

	c_ast_node_module_call_argument *rhs_argument_node = new c_ast_node_module_call_argument();
	module_call_node->add_argument(rhs_argument_node);
	rhs_argument_node->set_source_location(rhs->get_source_location());
	rhs_argument_node->set_argument_direction(e_ast_argument_direction::k_in);
	rhs_argument_node->set_value_expression(rhs.release());

	resolve_native_operator_call(module_call_node, native_operator, op.source_location);

	// Special case: if this is a subscript operation on a reference, swap this node out to be a subscript node. This
	// allows it to live on the LHS of assignment statements.
	if (native_operator == e_native_operator::k_subscript
		&& rhs_argument_node->get_value_expression()->is_type(e_ast_node_type::k_declaration_reference)) {
		c_ast_node_subscript *subscript_node = new c_ast_node_subscript();
		subscript_node->set_source_location(module_call_node->get_source_location());
		subscript_node->set_module_call(module_call_node);

		rule_head_context.release();
		rule_head_context.initialize(subscript_node);
	}
}

void c_ast_builder_visitor::exit_forward_expression(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression) {
	rule_head_context.initialize(forward_on_definition_pass(expression.release()));
}

void c_ast_builder_visitor::exit_unary_operator(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	const s_token &op,
	c_temporary_reference<c_ast_node_expression> &rhs) {
	c_ast_node_module_call *module_call_node = new c_ast_node_module_call();
	rule_head_context.initialize(module_call_node);
	module_call_node->set_source_location(op.source_location);

	e_native_operator native_operator = e_native_operator::k_invalid;
	switch (op.token_type) {
	case e_token_type::k_plus:
		native_operator = e_native_operator::k_noop;
		break;

	case e_token_type::k_minus:
		native_operator = e_native_operator::k_negation;
		break;

	case e_token_type::k_not:
		native_operator = e_native_operator::k_not;
		break;
	}

	wl_assert(native_operator != e_native_operator::k_invalid);

	c_ast_node_module_call_argument *rhs_argument_node = new c_ast_node_module_call_argument();
	module_call_node->add_argument(rhs_argument_node);
	rhs_argument_node->set_source_location(rhs->get_source_location());
	rhs_argument_node->set_argument_direction(e_ast_argument_direction::k_in);
	rhs_argument_node->set_value_expression(rhs.release());

	resolve_native_operator_call(module_call_node, native_operator, op.source_location);
}

bool c_ast_builder_visitor::enter_module_call(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &module,
	c_ast_node_module_call *&argument_list) {
	c_ast_node_module_call *module_call_node = new c_ast_node_module_call();
	rule_head_context.initialize(module_call_node);
	argument_list = module_call_node;
	return true;
}

void c_ast_builder_visitor::exit_module_call(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &module,
	c_ast_node_module_call *&argument_list) {
	c_ast_node_module_call *module_call_node = rule_head_context->get_as<c_ast_node_module_call>();
	module_call_node->set_source_location(module->get_source_location());

	// If this is a valid module call, the module argument is a reference to one or more modules.
	c_wrapped_array<c_ast_node_declaration *const> module_reference = try_get_typed_references(
		module.get(),
		e_ast_node_type::k_module_declaration,
		e_compiler_error::k_not_callable_type,
		"perform call on",
		"it is not a callable type");

	if (module_reference.get_count() > 0) {
		std::vector<c_ast_node_module_declaration *> candidates;
		candidates.reserve(module_reference.get_count());
		for (c_ast_node_declaration *reference : module_reference) {
			candidates.push_back(reference->get_as<c_ast_node_module_declaration>());
		}

		resolve_module_call(module_call_node, c_wrapped_array<c_ast_node_module_declaration *>(candidates));
	}

	module.release();
}

void c_ast_builder_visitor::exit_convert(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	s_value_with_source_location<c_ast_qualified_data_type> &data_type,
	c_temporary_reference<c_ast_node_expression> &expression) {
	c_ast_node_convert *convert_node = new c_ast_node_convert();
	rule_head_context.initialize(convert_node);
	convert_node->set_source_location(data_type.source_location);

	if (expression->get_data_type().is_error()) {
		convert_node->set_data_type(c_ast_qualified_data_type::error());
	} else if (is_ast_data_type_assignable(expression->get_data_type(), data_type.value)) {
		convert_node->set_data_type(data_type.value);
	} else {
		convert_node->set_data_type(c_ast_qualified_data_type::error());
		m_context.error(
			e_compiler_error::k_illegal_type_conversion,
			data_type.source_location,
			"Cannot convert from type '%s' to type '%s'",
			expression->get_data_type().to_string().c_str(),
			data_type.value.to_string().c_str());
	}

	expression.release();
}

void c_ast_builder_visitor::exit_access(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &lhs,
	const s_token &identifier) {
	c_ast_node_declaration_reference *declaration_reference_node = new c_ast_node_declaration_reference();
	rule_head_context.initialize(declaration_reference_node);
	declaration_reference_node->set_source_location(identifier.source_location);

	// Only namespace declarations currently support the access operator
	c_ast_node_declaration_reference *lhs_reference_node = lhs->try_get_as<c_ast_node_declaration_reference>();
	bool report_error = true;
	if (lhs_reference_node) {
		if (!lhs_reference_node->is_valid()) {
			// If the reference is invalid, don't report errors because we already reported one for the resolve failure
			report_error = false;
		} else {
			c_ast_node_namespace_declaration *namespace_declaration_node =
				lhs_reference_node->get_references()[0]->try_get_as<c_ast_node_namespace_declaration>();
			if (namespace_declaration_node) {
				report_error = false;
				c_tracked_scope *tracked_scope =
					m_tracked_scopes_from_scope_nodes.find(namespace_declaration_node->get_scope())->second.get();
				resolve_identifier(
					declaration_reference_node,
					tracked_scope,
					std::string(identifier.token_string).c_str(),
					true);
			}
		}
	}

	if (report_error) {
		m_context.error(
			e_compiler_error::k_identifier_resolution_not_allowed,
			identifier.source_location,
			"Failed to perform resolution for identifier '%s' because the context doesn't support named lookup",
			std::string(identifier.token_string).c_str());
	}

	lhs.release();
}

void c_ast_builder_visitor::exit_identifier(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	const s_token &identifier) {
	c_ast_node_declaration_reference *declaration_reference_node = new c_ast_node_declaration_reference();
	rule_head_context.initialize(declaration_reference_node);
	declaration_reference_node->set_source_location(identifier.source_location);
	resolve_identifier(
		declaration_reference_node,
		m_tracked_scopes.back(),
		std::string(identifier.token_string).c_str(),
		true);
}

void c_ast_builder_visitor::exit_literal(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	const s_token &literal) {
	c_ast_node_literal *literal_node = new c_ast_node_literal();
	rule_head_context.initialize(literal_node);
	rule_head_context->set_source_location(literal.source_location);

	switch (literal.token_type) {
	case e_token_type::k_literal_real:
		literal_node->set_real_value(literal.value.real_value);
		break;

	case e_token_type::k_literal_bool:
		literal_node->set_bool_value(literal.value.bool_value);
		break;

	case e_token_type::k_literal_string:
		literal_node->set_string_value(get_unescaped_token_string(literal).c_str());
		break;

	default:
		wl_unreachable();
	}
}

bool c_ast_builder_visitor::enter_array(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	const s_token &brace,
	c_ast_node_array *&values) {
	c_ast_node_array *array_node = new c_ast_node_array();
	rule_head_context.initialize(array_node);
	values = array_node;
	return true;
}

void c_ast_builder_visitor::exit_array(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	const s_token &brace,
	c_ast_node_array *&values) {
	c_ast_node_array *array_node = rule_head_context->get_as<c_ast_node_array>();
	array_node->set_source_location(brace.source_location);

	if (array_node->get_element_count() == 0) {
		// Array nodes should default to this type when created
		wl_assert(array_node->get_data_type().is_empty_array());
	} else {
		c_ast_qualified_data_type array_element_data_type;
		for (size_t element_index = 0; element_index < array_node->get_element_count(); element_index++) {
			const c_ast_qualified_data_type &element_data_type =
				array_node->get_element(element_index)->get_data_type();

			if (element_data_type.is_error()) {
				// We already reported an error previously
				array_element_data_type = c_ast_qualified_data_type::error();
				break;
			}

			// The array element data type is "downgraded" to the least-constant element type
			if (!array_element_data_type.is_valid()
				|| is_ast_data_type_assignable(array_element_data_type, element_data_type)) {
				array_element_data_type = element_data_type;
			} else {
				m_context.error(
					e_compiler_error::k_inconsistent_array_element_data_types,
					array_node->get_element(element_index)->get_source_location(),
					"Array element of type '%s' is incompatible previous element of type '%s'",
					element_data_type.to_string().c_str(),
					array_element_data_type.to_string().c_str());
				array_element_data_type = c_ast_qualified_data_type::error();
				break;
			}
		}

		wl_assert(array_element_data_type.is_valid());
		array_node->set_data_type(array_element_data_type.get_array_type());
	}
}

bool c_ast_builder_visitor::enter_array_value_list(
	c_ast_node_array *&rule_head_context,
	c_ast_node_array *&value_list) {
	value_list = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_array_single_item_value_list_item(
	c_ast_node_array *&rule_head_context,
	c_temporary_reference<c_ast_node_expression> &value) {
	rule_head_context->add_element(value.release());
}

bool c_ast_builder_visitor::enter_array_nonempty_value_list_item(
	c_ast_node_array *&rule_head_context,
	c_ast_node_array *&list_child,
	c_temporary_reference<c_ast_node_expression> &value) {
	list_child = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_array_nonempty_value_list_item(
	c_ast_node_array *&rule_head_context,
	c_ast_node_array *&list_child,
	c_temporary_reference<c_ast_node_expression> &value) {
	rule_head_context->add_element(value.release());
}

bool c_ast_builder_visitor::enter_module_call_argument_list(
	c_ast_node_module_call *&rule_head_context,
	c_ast_node_module_call *&argument_list) {
	argument_list = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_module_call_single_item_argument_list_item(
	c_ast_node_module_call *&rule_head_context,
	c_temporary_reference<c_ast_node_module_call_argument> &argument) {
	rule_head_context->add_argument(argument.release());
}

bool c_ast_builder_visitor::enter_module_call_nonempty_argument_list_item(
	c_ast_node_module_call *&rule_head_context,
	c_ast_node_module_call *&list_child,
	c_temporary_reference<c_ast_node_module_call_argument> &argument) {
	list_child = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_module_call_nonempty_argument_list_item(
	c_ast_node_module_call *&rule_head_context,
	c_ast_node_module_call *&list_child,
	c_temporary_reference<c_ast_node_module_call_argument> &argument) {
	rule_head_context->add_argument(argument.release());
}

void c_ast_builder_visitor::exit_module_call_unnamed_argument(
	c_temporary_reference<c_ast_node_module_call_argument> &rule_head_context,
	s_value_with_source_location<e_ast_argument_direction> &direction,
	c_temporary_reference<c_ast_node_expression> &expression) {
	rule_head_context.initialize(new c_ast_node_module_call_argument());
	rule_head_context->set_source_location(
		direction.source_location.is_valid() ? direction.source_location : expression->get_source_location());
	rule_head_context->set_argument_direction(direction.value);
	rule_head_context->set_value_expression(expression.release());
}

void c_ast_builder_visitor::exit_module_call_named_argument(
	c_temporary_reference<c_ast_node_module_call_argument> &rule_head_context,
	s_value_with_source_location<e_ast_argument_direction> &direction,
	const s_token &name,
	c_temporary_reference<c_ast_node_expression> &expression) {
	rule_head_context.initialize(new c_ast_node_module_call_argument());
	rule_head_context->set_source_location(
		direction.source_location.is_valid() ? direction.source_location : name.source_location);
	rule_head_context->set_argument_direction(direction.value);
	rule_head_context->set_name(get_unescaped_token_string(name).c_str());
	rule_head_context->set_value_expression(expression.release());
}

void c_ast_builder_visitor::exit_in_call_argument_qualifier(
	s_value_with_source_location<e_ast_argument_direction> &rule_head_context) {
	rule_head_context.value = e_ast_argument_direction::k_in;
}

void c_ast_builder_visitor::exit_out_call_argument_qualifier(
	s_value_with_source_location<e_ast_argument_direction> &rule_head_context,
	const s_token &qualifier) {
	rule_head_context.source_location = qualifier.source_location;
	rule_head_context.value = e_ast_argument_direction::k_out;
}

bool c_ast_builder_visitor::enter_scope(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	c_ast_node_scope *&statement_list) {
	statement_list = rule_head_context.get();
	return true;
}

bool c_ast_builder_visitor::enter_scope_item_list(
	c_ast_node_scope *&rule_head_context,
	c_ast_node_scope *&list_child,
	c_temporary_reference<c_ast_node_scope_item> &scope_item) {
	list_child = rule_head_context;
	return true;
}

void c_ast_builder_visitor::exit_scope_item_list(
	c_ast_node_scope *&rule_head_context,
	c_ast_node_scope *&list_child,
	c_temporary_reference<c_ast_node_scope_item> &scope_item) {
	rule_head_context->add_scope_item(scope_item.release());
}

void c_ast_builder_visitor::exit_scope_item_expression(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression) {
	c_ast_node_expression_statement *expression_statement_node = new c_ast_node_expression_statement();
	rule_head_context.initialize(expression_statement_node);
	expression_statement_node->set_source_location(expression->get_source_location());
	expression_statement_node->set_expression(expression.release());
}

void c_ast_builder_visitor::exit_scope_item_value_declaration(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &value_declaration) {
	// Start tracking this declaration
	add_declaration_to_tracked_scope(m_tracked_scopes.back(), value_declaration.get());

	// If it has an initializer then it's been assigned
	if (value_declaration->get_initialization_expression()) {
		m_tracked_scopes.back()->issue_declaration_assignment(value_declaration.get());
	}

	rule_head_context.initialize(value_declaration.release());
}

void c_ast_builder_visitor::exit_scope_item_assignment(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &lhs,
	s_value_with_source_location<e_native_operator> &assignment_type,
	c_temporary_reference<c_ast_node_expression> &rhs) {
	c_ast_node_assignment_statement *assignment_statement_node = new c_ast_node_assignment_statement();
	rule_head_context.initialize(assignment_statement_node);

	// The LHS might be a subscript node
	c_ast_node_subscript *subscript_node = lhs->try_get_as<c_ast_node_subscript>();

	// Make sure that the LHS points to a value that we can assign to
	c_wrapped_array<c_ast_node_declaration *const> value_reference = try_get_typed_references(
		subscript_node ? subscript_node->get_reference() : lhs.get(),
		e_ast_node_type::k_value_declaration,
		e_compiler_error::k_invalid_assignment,
		"assign to",
		"it is not an assignable type");

	c_ast_node_value_declaration *lhs_value_declaration = nullptr;
	if (value_reference.get_count() > 0) {
		c_ast_node_value_declaration *lhs_value_declaration =
			value_reference[0]->get_as<c_ast_node_value_declaration>();

		// Track the assignment of this value declaration
		m_tracked_scopes.back()->issue_declaration_assignment(lhs_value_declaration);

		if (!lhs_value_declaration->is_modifiable()) {
			m_context.error(
				e_compiler_error::k_invalid_assignment,
				assignment_type.source_location,
				"Cannot assign to value '%s' it is not a modifiable value",
				lhs_value_declaration->get_name());
		}

		assignment_statement_node->set_lhs_value_declaration(lhs_value_declaration);

		if (subscript_node) {
			// We can only do subscript assignments with a constant index
			c_ast_qualified_data_type index_data_type = subscript_node->get_index_expression()->get_data_type();
			if (!index_data_type.is_error()
				&& index_data_type.get_data_mutability() != e_ast_data_mutability::k_constant) {
				c_ast_qualified_data_type const_real_data_type(
					c_ast_data_type(e_ast_primitive_type::k_real),
					e_ast_data_mutability::k_constant);
				m_context.error(
					e_compiler_error::k_illegal_variable_subscript_assignment,
					subscript_node->get_index_expression()->get_source_location(),
					"Subscript assignment of value '%s' must use an index of type '%s', not '%s'",
					lhs_value_declaration->get_name(),
					const_real_data_type.to_string().c_str(),
					index_data_type.to_string().c_str());
			}

			// Copy the index expression into the assignment statement's LHS. We can't just swap it because if we're
			// using an assignment operator like +=, the expression gets transformed into x[i] = x[i] + y so the
			// expression gets duplicated.
			assignment_statement_node->set_lhs_index_expression(subscript_node->get_index_expression()->copy());
		}
	}

	if (assignment_type.value == e_native_operator::k_invalid) {
		// No additional operation, just assignment, so make sure the RHS type matches
		if (lhs_value_declaration) {
			// If this is a subscript operation, we're assigning to an element, not the array itself. The safest way to
			// grab that element type is to see what the subscript module call would have returned if this expression
			// were on the RHS. If the subscript call failed to resolve, we'll get an error type back.
			c_ast_qualified_data_type lhs_data_type = subscript_node
				? subscript_node->get_module_call()->get_resolved_module_declaration()->get_return_type()
				: lhs_value_declaration->get_data_type();

			if (!is_ast_data_type_assignable(rhs->get_data_type(), lhs_data_type)) {
				m_context.error(
					e_compiler_error::k_type_mismatch,
					assignment_type.source_location,
					"Cannot assign to value '%s' of type '%s' with an expression of type '%s'",
					lhs_value_declaration->get_name(),
					lhs_value_declaration->get_data_type().to_string().c_str(),
					rhs->get_data_type().to_string().c_str());
			}
		}

		assignment_statement_node->set_rhs_expression(rhs.release());

		// We grabbed everything we needed from the LHS already
		lhs.release();
	} else {
		// We convert a statement like "x += y" to the form "x = x + y" by creating an intermediate expression
		c_ast_node_module_call *module_call_node = new c_ast_node_module_call();
		assignment_statement_node->set_rhs_expression(module_call_node);
		module_call_node->set_source_location(assignment_type.source_location);

		c_ast_node_module_call_argument *lhs_argument_node = new c_ast_node_module_call_argument();
		module_call_node->add_argument(lhs_argument_node);
		lhs_argument_node->set_source_location(lhs->get_source_location());
		lhs_argument_node->set_argument_direction(e_ast_argument_direction::k_in);
		lhs_argument_node->set_value_expression(lhs.release());

		c_ast_node_module_call_argument *rhs_argument_node = new c_ast_node_module_call_argument();
		module_call_node->add_argument(rhs_argument_node);
		rhs_argument_node->set_source_location(rhs->get_source_location());
		rhs_argument_node->set_argument_direction(e_ast_argument_direction::k_in);
		rhs_argument_node->set_value_expression(rhs.release());

		resolve_native_operator_call(module_call_node, assignment_type.value, assignment_type.source_location);
	}
}

void c_ast_builder_visitor::exit_scope_item_return_void_statement(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &return_keyword) {
	c_ast_node_return_statement *return_statement_node = new c_ast_node_return_statement();
	rule_head_context.initialize(return_statement_node);
	return_statement_node->set_source_location(return_keyword.source_location);

	// Keep track of where we return in the current scope
	m_tracked_scopes.back()->issue_return_statement();

	validate_current_module_return_type(
		c_ast_qualified_data_type(c_ast_data_type(e_ast_primitive_type::k_void), e_ast_data_mutability::k_variable),
		return_keyword.source_location);
}

void c_ast_builder_visitor::exit_scope_item_return_statement(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &return_keyword,
	c_temporary_reference<c_ast_node_expression> &expression) {
	c_ast_node_return_statement *return_statement_node = new c_ast_node_return_statement();
	rule_head_context.initialize(return_statement_node);
	return_statement_node->set_source_location(return_keyword.source_location);
	return_statement_node->set_expression(expression.release());

	// Keep track of where we return in the current scope
	m_tracked_scopes.back()->issue_return_statement();

	validate_current_module_return_type(
		c_ast_qualified_data_type(c_ast_data_type(e_ast_primitive_type::k_void), e_ast_data_mutability::k_variable),
		return_keyword.source_location);
}

void c_ast_builder_visitor::exit_scope_item_if_statement(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	c_temporary_reference<c_ast_node_if_statement> &if_statement) {
	rule_head_context.initialize(if_statement.release());
}

bool c_ast_builder_visitor::enter_scope_item_for_loop(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &for_keyword,
	c_ast_node_for_loop *&iterator,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	c_ast_node_for_loop *for_loop_node = new c_ast_node_for_loop();
	rule_head_context.initialize(for_loop_node);
	for_loop_node->set_source_location(for_keyword.source_location);

	// Initialize the scope here so that the loop value gets pulled in
	scope.initialize(new c_ast_node_scope());
	scope->set_source_location(brace.source_location);

	c_tracked_scope *tracked_scope =
		new c_tracked_scope(m_tracked_scopes.back(), e_tracked_scope_type::k_local, nullptr);
	m_tracked_scopes_from_scope_nodes.insert(std::make_pair(scope.get(), tracked_scope));
	m_tracked_scopes.push_back(tracked_scope);

	iterator = for_loop_node;
	return true;
}

void c_ast_builder_visitor::exit_scope_item_for_loop(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &for_keyword,
	c_ast_node_for_loop *&iterator,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	rule_head_context->get_as<c_ast_node_for_loop>()->set_loop_scope(scope.release());
}

void c_ast_builder_visitor::exit_break_statement(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &break_keyword) {
	c_ast_node_break_statement *break_statement_node = new c_ast_node_break_statement();
	rule_head_context.initialize(break_statement_node);
	break_statement_node->set_source_location(break_keyword.source_location);

	// Make sure we're in a for-loop somewhere down the scope stack
	if (!is_loop_in_scope_stack()) {
		m_context.error(
			e_compiler_error::k_illegal_break_statement,
			break_keyword.source_location,
			"Break statement is not within a loop");
	}

	m_tracked_scopes.back()->issue_break_statement();
}

void c_ast_builder_visitor::exit_continue_statement(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &continue_keyword) {
	c_ast_node_continue_statement *continue_statement_node = new c_ast_node_continue_statement();
	rule_head_context.initialize(continue_statement_node);
	continue_statement_node->set_source_location(continue_keyword.source_location);

	// Make sure we're in a for-loop somewhere down the scope stack
	if (!is_loop_in_scope_stack()) {
		m_context.error(
			e_compiler_error::k_illegal_continue_statement,
			continue_keyword.source_location,
			"Continue statement is not within a loop");
	}

	m_tracked_scopes.back()->issue_continue_statement();
}

bool c_ast_builder_visitor::enter_scope_item_scope(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	scope.initialize(new c_ast_node_scope());
	scope->set_source_location(brace.source_location);

	c_tracked_scope *tracked_scope =
		new c_tracked_scope(m_tracked_scopes.back(), e_tracked_scope_type::k_local, nullptr);
	m_tracked_scopes_from_scope_nodes.insert(std::make_pair(scope.get(), tracked_scope));
	m_tracked_scopes.push_back(tracked_scope);

	return true;
}

void c_ast_builder_visitor::exit_scope_item_scope(
	c_temporary_reference<c_ast_node_scope_item> &rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	c_tracked_scope *tracked_scope = m_tracked_scopes.back();
	wl_assert(tracked_scope == m_tracked_scopes_from_scope_nodes.find(scope.get())->second.get());
	m_tracked_scopes.pop_back();

	// Merge the child scope down into the parent scope
	m_tracked_scopes.back()->merge_child_scope(tracked_scope);
	m_tracked_scopes_from_scope_nodes.erase(scope.get());

	rule_head_context.initialize(scope.release());
}

void c_ast_builder_visitor::exit_if_statement(
	c_temporary_reference<c_ast_node_if_statement> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression,
	c_temporary_reference<c_ast_node_scope> &true_scope) {
	// Merge the child scope down into the parent scope (we already popped the tracked scope stack)
	c_tracked_scope *tracked_true_scope = m_tracked_scopes_from_scope_nodes.find(true_scope.get())->second.get();
	m_tracked_scopes.back()->merge_child_if_statement_scopes(tracked_true_scope, nullptr);
	m_tracked_scopes_from_scope_nodes.erase(true_scope.get());

	rule_head_context->set_expression(expression.release());
	rule_head_context->set_true_scope(true_scope.release());
}

void c_ast_builder_visitor::exit_if_else_statement(
	c_temporary_reference<c_ast_node_if_statement> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression,
	c_temporary_reference<c_ast_node_scope> &true_scope,
	c_temporary_reference<c_ast_node_scope> &false_scope) {
	// Merge the child scopes down into the parent scope (we already popped the tracked scope stack)
	c_tracked_scope *tracked_true_scope = m_tracked_scopes_from_scope_nodes.find(true_scope.get())->second.get();
	c_tracked_scope *tracked_false_scope = m_tracked_scopes_from_scope_nodes.find(false_scope.get())->second.get();
	m_tracked_scopes.back()->merge_child_if_statement_scopes(tracked_true_scope, tracked_false_scope);
	m_tracked_scopes_from_scope_nodes.erase(true_scope.get());
	m_tracked_scopes_from_scope_nodes.erase(false_scope.get());

	rule_head_context->set_expression(expression.release());
	rule_head_context->set_true_scope(true_scope.release());
	rule_head_context->set_false_scope(false_scope.release());
}

void c_ast_builder_visitor::exit_if_statement_expression(
	c_temporary_reference<c_ast_node_expression> &rule_head_context,
	c_temporary_reference<c_ast_node_expression> &expression) {
	// Expressions must be const-bool
	c_ast_qualified_data_type const_bool_data_type(
		c_ast_data_type(e_ast_primitive_type::k_bool),
		e_ast_data_mutability::k_constant);
	if (!is_ast_data_type_assignable(expression->get_data_type(), const_bool_data_type)) {
		m_context.error(
			e_compiler_error::k_invalid_if_statement_data_type,
			expression->get_source_location(),
			"Expected if-statement expression of type '%s' but got type '%s'",
			expression->get_data_type().to_string().c_str(),
			const_bool_data_type.to_string().c_str());
	}

	rule_head_context.initialize(expression.release());
}

bool c_ast_builder_visitor::enter_if_statement_scope(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	scope.initialize(new c_ast_node_scope());
	scope->set_source_location(brace.source_location);

	c_tracked_scope *tracked_scope =
		new c_tracked_scope(m_tracked_scopes.back(), e_tracked_scope_type::k_if_statement, nullptr);
	m_tracked_scopes_from_scope_nodes.insert(std::make_pair(scope.get(), tracked_scope));
	m_tracked_scopes.push_back(tracked_scope);
	return true;
}

void c_ast_builder_visitor::exit_if_statement_scope(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	const s_token &brace,
	c_temporary_reference<c_ast_node_scope> &scope) {
	rule_head_context.initialize(scope.release());

	// Don't free the tracked scope yet - we'll do that when we exit the if statement
	m_tracked_scopes.pop_back();
}

bool c_ast_builder_visitor::enter_else_if(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	c_temporary_reference<c_ast_node_if_statement> &if_statement) {
	// Convert "if { X } else if { Y }" to "if { X } else { if { Y } }" by wrapping if_statement in an "else" scope
	rule_head_context.initialize(new c_ast_node_scope());
	// This scope doesn't have a source location

	c_tracked_scope *tracked_scope =
		new c_tracked_scope(m_tracked_scopes.back(), e_tracked_scope_type::k_if_statement, nullptr);
	m_tracked_scopes_from_scope_nodes.insert(std::make_pair(rule_head_context.get(), tracked_scope));
	m_tracked_scopes.push_back(tracked_scope);
	return true;
}

void c_ast_builder_visitor::exit_else_if(
	c_temporary_reference<c_ast_node_scope> &rule_head_context,
	c_temporary_reference<c_ast_node_if_statement> &if_statement) {
	// Don't free the tracked scope yet - we'll do that when we exit the if statement
	m_tracked_scopes.pop_back();

	// The "else" scope only contains then if statement
	rule_head_context->add_scope_item(if_statement.release());
}

void c_ast_builder_visitor::exit_for_loop_iterator(
	c_ast_node_for_loop *&rule_head_context,
	c_temporary_reference<c_ast_node_value_declaration> &value,
	c_temporary_reference<c_ast_node_expression> &range) {
	// The grammar disallows initializer expressions on for loop values
	wl_assert(!value->get_initialization_expression());

	// The range expression must be an array type
	if (!range->get_data_type().is_error() && !range->get_data_type().is_array()) {
		m_context.error(
			e_compiler_error::k_illegal_for_loop_range_type,
			range->get_source_location(),
			"Illegal for-loop range type '%s'; for-loop range must be an array type",
			range->get_data_type().to_string().c_str());
	} else {
		// The value declaration must be assignable from a range element
		bool value_type_valid;
		if (range->get_data_type().is_empty_array()) {
			// Any non-array type is assignable from an empty array
			value_type_valid = value->get_data_type().is_error() || !value->get_data_type().is_array();
		} else {
			value_type_valid = is_ast_data_type_assignable(
				range->get_data_type().get_element_type(),
				value->get_data_type());
		}

		if (!value_type_valid) {
			m_context.error(
				e_compiler_error::k_type_mismatch,
				range->get_source_location(),
				"Cannot assign element values from for-loop range of type '%s' to value '%s' of type '%s'",
				range->get_data_type().to_string().c_str(),
				value->get_name(),
				value->get_data_type().to_string().c_str());
		}
	}

	rule_head_context->set_value_declaration(value.release());
	rule_head_context->set_range_expression(range.release());
}

void c_ast_builder_visitor::exit_assignment(
	s_value_with_source_location<e_native_operator> &rule_head_context,
	const s_token &op) {
	rule_head_context.source_location = op.source_location;
	rule_head_context.value = e_native_operator::k_invalid;

	switch (op.token_type) {
	case e_token_type::k_assign:
		// Regular assignment has no additional operation to run
		break;

	case e_token_type::k_assign_plus:
		rule_head_context.value = e_native_operator::k_addition;
		break;

	case e_token_type::k_assign_minus:
		rule_head_context.value = e_native_operator::k_subtraction;
		break;

	case e_token_type::k_assign_multiply:
		rule_head_context.value = e_native_operator::k_multiplication;
		break;

	case e_token_type::k_assign_divide:
		rule_head_context.value = e_native_operator::k_division;
		break;

	case e_token_type::k_assign_modulo:
		rule_head_context.value = e_native_operator::k_modulo;
		break;

	case e_token_type::k_assign_and:
		rule_head_context.value = e_native_operator::k_and;
		break;

	case e_token_type::k_assign_or:
		rule_head_context.value = e_native_operator::k_or;
		break;

	default:
		wl_unreachable();
	}
}

void c_ast_builder_visitor::build_tracked_scope(c_ast_node_scope *scope_node, c_tracked_scope *tracked_scope) {
	m_tracked_scopes_from_scope_nodes.insert(std::make_pair(scope_node, tracked_scope));

	for (size_t item_index = 0; item_index < scope_node->get_scope_item_count(); item_index++) {
		c_ast_node_declaration *declaration_node =
			scope_node->get_scope_item(item_index)->try_get_as<c_ast_node_declaration>();

		c_ast_node_namespace_declaration *namespace_declaration_node =
			declaration_node->try_get_as<c_ast_node_namespace_declaration>();
		if (namespace_declaration_node) {
			c_tracked_scope *tracked_namespace_scope =
				new c_tracked_scope(tracked_scope, e_tracked_scope_type::k_namespace, nullptr);

			// Recursively build namespace scope
			build_tracked_scope(namespace_declaration_node->get_scope(), tracked_namespace_scope);
		}
	}
}

c_tracked_declaration *c_ast_builder_visitor::add_declaration_to_tracked_scope(
	c_tracked_scope *tracked_scope,
	c_ast_node_declaration *declaration) {
	// Specifically handle module overloads
	const c_ast_node_module_declaration *module_declaration = declaration->try_get_as<c_ast_node_module_declaration>();

	std::vector<c_tracked_declaration *> lookup_buffer;
	tracked_scope->lookup_declarations_by_name(declaration->get_name(), lookup_buffer);
	for (const c_tracked_declaration *tracked_declaration : lookup_buffer) {
		// This could be a valid overload so check for that case
		const c_ast_node_module_declaration *existing_module_declaration =
			tracked_declaration->get_declaration()->try_get_as<c_ast_node_module_declaration>();
		if (module_declaration && existing_module_declaration) {
			if (do_module_overloads_conflict(module_declaration, existing_module_declaration)) {
				m_context.error(
					e_compiler_error::k_declaration_conflict,
					declaration->get_source_location(),
					"Conflict between overloaded modules '%s'",
					declaration->get_name());

				// Just report the first conflict
				break;
			}
		} else {
			m_context.error(
				e_compiler_error::k_declaration_conflict,
				declaration->get_source_location(),
				"Conflict between %s '%s' and existing %s of the same name",
				declaration->describe_type(),
				declaration->get_name(),
				declaration->describe_type());

			// Just report the first conflict
			break;
		}
	}

	return tracked_scope->add_declaration(declaration);
}

void c_ast_builder_visitor::resolve_identifier(
	c_ast_node_declaration_reference *reference_node,
	c_tracked_scope *tracked_scope,
	const char *identifier,
	bool recurse) {
	std::vector<c_tracked_declaration *> references;
	tracked_scope->lookup_declarations_by_name(identifier, references);

	bool error = false;
	if (references.empty()) {
		error = true;
		m_context.error(
			e_compiler_error::k_identifier_resolution_failed,
			reference_node->get_source_location(),
			"Failed to resolve identifier '%s'",
			identifier);
	} else if (references.size() > 1) {
		// If they're all modules, that's allowed because of overloading. Otherwise it's an error.
		for (const c_tracked_declaration *reference : references) {
			if (!reference->get_declaration()->is_type(e_ast_node_type::k_module_declaration)) {
				error = true;
				m_context.error(
					e_compiler_error::k_ambiguous_identifier_resolution,
					reference_node->get_source_location(),
					"Identifier '%s' resolved to multiple candidates",
					identifier);
				break;
			}
		}
	}

	if (!error) {
		for (const c_tracked_declaration *reference : references) {
			reference_node->add_reference(reference->get_declaration());
		}
	}
}

void c_ast_builder_visitor::resolve_module_call(
	c_ast_node_module_call *module_call,
	c_wrapped_array<c_ast_node_module_declaration *> module_candidates) {
	c_wrapped_array<const c_ast_node_module_declaration *const> const_module_candidates(
		module_candidates.get_pointer(),
		module_candidates.get_count());
	s_module_call_resolution_result result = ::resolve_module_call(module_call, const_module_candidates);
	switch (result.result) {
	case e_module_call_resolution_result::k_success:
		break;

	case e_module_call_resolution_result::k_invalid_named_argument:
		m_context.error(
			e_compiler_error::k_invalid_named_argument,
			module_call->get_argument(result.call_argument_index)->get_source_location(),
			"Module '%s' has no argument named '%s'",
			module_candidates[0]->get_name(),
			module_call->get_argument(result.call_argument_index)->get_name());
		return;

	case e_module_call_resolution_result::k_too_many_arguments_provided:
		m_context.error(
			e_compiler_error::k_too_many_arguments_provided,
			module_call->get_argument(result.call_argument_index)->get_source_location(),
			"Too many arguments provided for module '%s'",
			module_candidates[0]->get_name());
		return;

	case e_module_call_resolution_result::k_argument_provided_multiple_times:
		m_context.error(
			e_compiler_error::k_duplicate_argument_provided,
			module_call->get_argument(result.call_argument_index)->get_source_location(),
			"The argument '%s' for module '%s' was provided multiple times",
			module_candidates[0]->get_argument(result.declaration_argument_index)->get_name(),
			module_candidates[0]->get_name());
		return;

	case e_module_call_resolution_result::k_argument_direction_mismatch:
		m_context.error(
			e_compiler_error::k_argument_direction_mismatch,
			module_call->get_argument(result.call_argument_index)->get_source_location(),
			"The argument '%s' for module '%s' was declared as an '%s' argument but an '%s' argument was provided",
			module_candidates[0]->get_argument(result.declaration_argument_index)->get_name(),
			module_candidates[0]->get_name(),
			get_argument_direction_string(
				module_candidates[0]->get_argument(result.declaration_argument_index)->get_argument_direction()),
			get_argument_direction_string(
				module_call->get_argument(result.call_argument_index)->get_argument_direction()));
		return;

	case e_module_call_resolution_result::k_missing_argument:
		m_context.error(
			e_compiler_error::k_missing_argument,
			module_call->get_source_location(),
			"The argument '%s' for module '%s' was not provided",
			module_candidates[0]->get_argument(result.declaration_argument_index)->get_name(),
			module_candidates[0]->get_name());
		return;

	case e_module_call_resolution_result::k_multiple_matching_modules:
		m_context.error(
			e_compiler_error::k_ambiguous_module_overload_resolution,
			module_call->get_source_location(),
			"Call to overloaded module '%s' is ambiguous",
			module_candidates[0]->get_name());
		return;

	case e_module_call_resolution_result::k_no_matching_modules:
		m_context.error(
			e_compiler_error::k_empty_module_overload_resolution,
			module_call->get_source_location(),
			"Call to overloaded module '%s' matched none of the available candidates",
			module_candidates[0]->get_name());
		return;

	default:
		wl_unreachable();
	}

	// We've validated that a module was chosen, now make sure all the types are correct. Note that when there is more
	// than one module candidate, we need to validate type compatibility in order to select a candidate, so we should
	// never trigger any of those errors in this case.
	c_ast_node_module_declaration *module_declaration = module_candidates[result.selected_module_index];
	std::vector<c_ast_node_expression *> argument_expressions;
	get_argument_expressions(module_declaration, module_call, argument_expressions);

	// First check what we should do with dependent-constant outputs
	bool any_dependent_constant_inputs_variable = false;
	bool any_dependent_constant_inputs_dependent_constant = false;
	for (size_t argument_index = 0; argument_index < module_declaration->get_argument_count(); argument_index++) {
		const c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
		if (argument->get_argument_direction() != e_ast_argument_direction::k_in
			|| argument->get_data_type().get_data_mutability() != e_ast_data_mutability::k_dependent_constant) {
			continue;
		}

		c_ast_qualified_data_type data_type = argument_expressions[argument_index]->get_data_type();
		if (data_type.is_error() || data_type.get_data_mutability() == e_ast_data_mutability::k_variable) {
			// If any error types are provided, assume that they are not intended to be constant
			any_dependent_constant_inputs_variable = true;
		} else if (data_type.get_data_mutability() == e_ast_data_mutability::k_dependent_constant) {
			any_dependent_constant_inputs_dependent_constant = true;
		}
	}

	e_ast_data_mutability dependent_constant_output_mutability;
	if (any_dependent_constant_inputs_variable) {
		// If any dependent constant inputs were variable, then the outputs must all be variable
		dependent_constant_output_mutability = e_ast_data_mutability::k_variable;
	} else if (any_dependent_constant_inputs_dependent_constant) {
		// If any dependent constant inputs were dependent-constants, then the outputs must all be dependent-constants
		dependent_constant_output_mutability = e_ast_data_mutability::k_dependent_constant;
	} else {
		// Otherwise, the outputs are constants
		dependent_constant_output_mutability = e_ast_data_mutability::k_constant;
	}

	for (size_t argument_index = 0; argument_index < module_declaration->get_argument_count(); argument_index++) {
		c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
		c_ast_node_expression *argument_expression = argument_expressions[argument_index];

		resolve_module_call_argument(
			module_declaration,
			argument,
			module_call,
			argument_expression,
			dependent_constant_output_mutability);
	}

	module_call->set_resolved_module_declaration(module_declaration, dependent_constant_output_mutability);
}

void c_ast_builder_visitor::resolve_module_call_argument(
	c_ast_node_module_declaration *module_declaration,
	c_ast_node_module_declaration_argument *argument,
	c_ast_node_module_call *module_call,
	c_ast_node_expression *argument_expression,
	e_ast_data_mutability dependent_constant_output_mutability) {
	c_ast_qualified_data_type argument_data_type = argument->get_data_type();
	c_ast_qualified_data_type expression_data_type = argument_expression->get_data_type();

	if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
		// For inputs, the expression gets assigned to the argument
		if (!is_ast_data_type_assignable(expression_data_type, argument_data_type)) {
			m_context.error(
				e_compiler_error::k_type_mismatch,
				argument_expression->get_source_location(),
				"Cannot initialize module '%s' argument '%s' of type '%s' with a value of type '%s'",
				module_declaration->get_name(),
				argument->get_name(),
				argument->get_data_type().to_string(),
				expression_data_type.to_string().c_str());
		}
	} else {
		wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);

		// If the output argument is dependent-constant determine what its true type is based on the inputs
		if (argument_data_type.get_data_mutability() == e_ast_data_mutability::k_dependent_constant) {
			argument_data_type = c_ast_qualified_data_type(
				argument_data_type.get_data_type(),
				dependent_constant_output_mutability);
		}

		// The argument expression might be a subscript node
		c_ast_node_subscript *subscript_node = argument_expression->try_get_as<c_ast_node_subscript>();

		// An output argument should be a valid value reference or subscript
		std::string error_action =
			"initialize module '" + std::string(module_declaration->get_name())
			+ " out argument '" + argument->get_name() + "' with";
		c_wrapped_array<c_ast_node_declaration *const> value_reference = try_get_typed_references(
			subscript_node ? subscript_node->get_reference() : argument_expression,
			e_ast_node_type::k_value_declaration,
			e_compiler_error::k_invalid_out_argument,
			error_action.c_str(),
			"it is not a valid out argument type");

		if (value_reference.get_count() > 0) {
			c_ast_node_value_declaration *value_declaration =
				value_reference[0]->get_as<c_ast_node_value_declaration>();

			// Track the assignment of this value declaration
			m_tracked_scopes.back()->issue_declaration_assignment(value_declaration);

			// Make sure the value being referenced is both modifiable (i.e. not a global scope value) and is of the
			// correct type. Note that for output values, the argument data type must be assignable to the expression
			// data type, which is the reverse of input arguments.
			if (!value_declaration->is_modifiable()) {
				m_context.error(
					e_compiler_error::k_invalid_out_argument,
					argument_expression->get_source_location(),
					"Cannot initialize module '%s' out argument '%s' with value '%s' "
					"because it is not a modifiable value",
					module_declaration->get_name(),
					argument->get_name(),
					value_declaration->get_name());
			} else {
				if (!is_ast_data_type_assignable(argument_data_type, expression_data_type)) {
					m_context.error(
						e_compiler_error::k_type_mismatch,
						argument_expression->get_source_location(),
						"Cannot initialize module '%s' out argument '%s' of type '%s' with a value '%s' of type '%s'",
						module_declaration->get_name(),
						argument->get_name(),
						argument->get_data_type().to_string(),
						value_declaration->get_name(),
						expression_data_type.to_string().c_str());
				}
			}

			// We can only do subscript assignments with a constant index
			if (subscript_node) {
				c_ast_qualified_data_type index_data_type = subscript_node->get_index_expression()->get_data_type();
				if (!index_data_type.is_error()
					&& index_data_type.get_data_mutability() != e_ast_data_mutability::k_constant) {
					c_ast_qualified_data_type const_real_data_type(
						c_ast_data_type(e_ast_primitive_type::k_real),
						e_ast_data_mutability::k_constant);
					m_context.error(
						e_compiler_error::k_illegal_variable_subscript_assignment,
						subscript_node->get_index_expression()->get_source_location(),
						"Subscript assignment of value '%s' must use an index of type '%s', not '%s'",
						value_declaration->get_name(),
						const_real_data_type.to_string().c_str(),
						index_data_type.to_string().c_str());
				}
			}
		}
	}
}

void c_ast_builder_visitor::resolve_native_operator_call(
	c_ast_node_module_call *module_call,
	e_native_operator native_operator,
	const s_compiler_source_location &operator_source_location) {
	// Construct a temporary reference node to hold references to the backing modules for the resolved operator
	std::unique_ptr<c_ast_node_declaration_reference> operator_reference(new c_ast_node_declaration_reference());
	operator_reference->set_source_location(operator_source_location);
	resolve_identifier(
		operator_reference.get(),
		m_tracked_scopes.back(),
		get_native_operator_native_module_name(native_operator),
		true);

	// Operators should always resolve successfully
	wl_assert(operator_reference->is_valid());

	std::vector<c_ast_node_module_declaration *> candidates;
	candidates.reserve(operator_reference->get_references().get_count());
	for (c_ast_node_declaration *reference : operator_reference->get_references()) {
		candidates.push_back(reference->get_as<c_ast_node_module_declaration>());
	}

	resolve_module_call(module_call, c_wrapped_array<c_ast_node_module_declaration *>(candidates));
}

c_wrapped_array<c_ast_node_declaration *const> c_ast_builder_visitor::try_get_typed_references(
	const c_ast_node_expression *expression_node,
	e_ast_node_type reference_type,
	e_compiler_error failure_error,
	const char *error_action,
	const char *error_reason) {
	const c_ast_node_declaration_reference *reference_node =
		expression_node->try_get_as<c_ast_node_declaration_reference>();
	if (reference_node) {
		if (!reference_node->is_valid()) {
			// If the reference is invalid, don't report errors because we already reported one for the resolve failure
		} else {
			if (reference_node->get_references()[0]->is_type(reference_type)) {
				return reference_node->get_references();
			} else {
				m_context.error(
					failure_error,
					expression_node->get_source_location(),
					"Failed to %s %s '%s' because %s",
					error_action,
					reference_node->get_references()[0]->describe_type(),
					reference_node->get_references()[0]->get_name(),
					error_reason);
			}
		}
	} else {
		m_context.error(
			failure_error,
			expression_node->get_source_location(),
			"Failed to %s the expression provided because %s",
			error_action,
			reference_node->get_references()[0]->describe_type(),
			reference_node->get_references()[0]->get_name(),
			error_reason);
	}

	return {};
}

void c_ast_builder_visitor::validate_current_module_return_type(
	const c_ast_qualified_data_type &data_type,
	const s_compiler_source_location &source_location) {
	if (data_type.is_error()) {
		// Don't report on error types
		return;
	}

	if (is_ast_data_type_assignable(data_type, m_current_module->get_return_type())) {
		m_context.error(
			e_compiler_error::k_return_type_mismatch,
			source_location,
			"Expected return statement value of type '%s' for module '%s' but got type '%s'",
			data_type.to_string().c_str(),
			m_current_module->get_name(),
			m_current_module->get_return_type().to_string().c_str());
	}
}

bool c_ast_builder_visitor::is_loop_in_scope_stack() const {
	// Make sure we're in a for-loop somewhere down the scope stack
	const c_tracked_scope *current_scope = m_tracked_scopes.back();
	while (current_scope) {
		if (current_scope->get_scope_type() == e_tracked_scope_type::k_for_loop) {
			return true;
		} else if (current_scope->get_scope_type() == e_tracked_scope_type::k_module) {
			return false;
		} else {
			current_scope = current_scope->get_parent();
		}
	}

	wl_unreachable(); // We should always eventually hit a module scope
	return false;
}

static const char *get_argument_direction_string(e_ast_argument_direction argument_direction) {
	switch (argument_direction) {
	case e_ast_argument_direction::k_invalid:
		wl_unreachable();
		break;

	case e_ast_argument_direction::k_in:
		return "in";

	case e_ast_argument_direction::k_out:
		return "out";
	}

	return nullptr;
}
