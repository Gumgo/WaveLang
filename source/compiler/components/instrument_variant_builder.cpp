#include "compiler/components/instrument_variant_builder.h"
#include "compiler/graph_trimmer.h"
#include "compiler/tracked_scope.h"
#include "compiler/try_call_native_module.h"

#include "instrument/execution_graph.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static constexpr size_t k_max_module_call_depth = 100;

static c_native_module_data_type native_module_data_type_from_ast_data_type(c_ast_data_type ast_data_type);

class c_execution_graph_builder {
public:
	c_execution_graph_builder(
		c_compiler_context &context,
		c_execution_graph &execution_graph,
		const s_instrument_globals &instrument_globals,
		c_ast_node_module_declaration *entry_point);

	bool build();

private:
	enum class e_scope_action {
		k_none,
		k_return,
		k_break,
		k_continue
	};

	// We use a manually constructed stack rather than recursion to avoid blowing up the stack. Each stack entry is
	// visited as many times as necessary. The state value is initialized to 0 and is used in a custom manner for each
	// node type.
	struct s_ast_node_stack_entry {
		c_ast_node *ast_node;
		uint64 state;
	};

#if IS_TRUE(ASSERTS_ENABLED)
	struct s_validation_token_stack_entry {
		bool is_validation_token;
		union {
			void *pointer_validation_token;
			size_t value_validation_token;
		};
	};
#endif // IS_TRUE(ASSERTS_ENABLED)

	void build_tracked_scope(c_ast_node_scope *scope_node, c_tracked_scope *tracked_scope);
	bool initialize_global_scope_constants(c_ast_node_scope *scope);
	bool initialize_global_scope_constant(c_ast_node_value_declaration *value_declaration);

	// Pushes a node handle onto the expression result stack and adds a temporary reference to it
	void push_expression_result(h_graph_node node_handle);

	// Returns an iterable list of expression results of the range [end-count, end). Iteration ends at the last element
	// in the stack.
	c_wrapped_array<h_graph_node> get_expression_results(size_t count);

	// Pops the last "count" expression results from the stack, removing a temporary reference from each and trimming
	// the graph
	void pop_and_trim_expression_results(size_t count);

	// These are used to validate that the expression result stack is in the order we expect it to be in. When a
	// validation token is pushed, a token with the same value must later be popped. These functions do nothing if
	// asserts are disabled.
	void push_validation_token(void *validation_token);
	void push_validation_token(size_t validation_token);
	void pop_validation_token(void *validation_token);
	void pop_validation_token(size_t validation_token);

	void push_ast_node(c_ast_node *node);
	bool evaluate_ast_node_stack();
	bool evaluate_ast_node(c_ast_node *node, uint64 &state, bool &pop_node_out);

	bool evaluate_ast_node(c_ast_node_scope *node, uint64 &state, bool &pop_node_out);

	// Scope items:
	bool evaluate_ast_node(c_ast_node_value_declaration *value_declaration, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_expression_statement *expression_statement, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_assignment_statement *assignment_statement, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_return_statement *return_statement, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_if_statement *if_statement, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_for_loop *for_loop, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_break_statement *break_statement, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_continue_statement *continue_statement, uint64 &state, bool &pop_node_out);

	// Expressions:
	bool evaluate_ast_node(c_ast_node_literal *literal, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_array *array, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_declaration_reference *declaration_reference, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_module_call *module_call, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_convert *convert, uint64 &state, bool &pop_node_out);
	bool evaluate_ast_node(c_ast_node_subscript *subscript, uint64 &state, bool &pop_node_out);

	bool assign_expression_value(
		c_tracked_declaration *tracked_declaration,
		h_graph_node expression_node_handle,
		h_graph_node index_expression_node_handle,
		const s_compiler_source_location &index_expression_source_location);

	bool issue_native_module_call(
		c_ast_node_module_declaration *native_module_declaration,
		c_ast_node_module_call *native_module_call);

	c_compiler_context &m_context;
	c_execution_graph &m_execution_graph;
	const s_instrument_globals &m_instrument_globals;
	c_ast_node_module_declaration *m_entry_point;

	c_graph_trimmer m_graph_trimmer;

	// Stack of tracked scopes - global is at the bottom
	std::vector<std::unique_ptr<c_tracked_scope>> m_tracked_scopes;

	// Stack of AST nodes to visit
	std::vector<s_ast_node_stack_entry> m_ast_node_stack;

	// Stack of graph node handles returned when expressions are evaluated
	std::vector<h_graph_node> m_expression_result_stack;

#if IS_TRUE(ASSERTS_ENABLED)
	// Each time a node handle is pushed on the expression result stack, an empty entry is pushed here. Additionally,
	// validation tokens can be pushed and popped on this stack. Validation tokens are used to ensure that the state of
	// the expression result stack is valid.
	std::vector<s_validation_token_stack_entry> m_validation_token_stack;
#endif // IS_TRUE(ASSERTS_ENABLED)

	h_graph_node m_return_value_node_handle = h_graph_node::invalid();
	bool m_return_statement_issued = false;
	bool m_break_statement_issued = false;
	bool m_continue_statement_issued = false;
	size_t m_module_call_depth = 0;

	// When we are initializing a constant we add it to this set so we can detect self-reference errors
	std::unordered_set<c_ast_node_value_declaration *> m_constants_being_initialized;
};

c_instrument_variant *c_instrument_variant_builder::build_instrument_variant(
	c_compiler_context &context,
	const s_instrument_globals &instrument_globals,
	c_ast_node_module_declaration *voice_entry_point,
	c_ast_node_module_declaration *fx_entry_point) {
	std::unique_ptr<c_instrument_variant> instrument_variant(new c_instrument_variant());
	instrument_variant->set_instrument_globals(instrument_globals);

	if (voice_entry_point) {
		std::unique_ptr<c_execution_graph> execution_graph = std::make_unique<c_execution_graph>();
		c_execution_graph_builder execution_graph_builder(
			context,
			*execution_graph.get(),
			instrument_globals,
			voice_entry_point);
		if (!execution_graph_builder.build()) {
			return nullptr;
		}

		instrument_variant->set_voice_execution_graph(execution_graph.release());
	}

	if (fx_entry_point) {
		std::unique_ptr<c_execution_graph> execution_graph = std::make_unique<c_execution_graph>();
		c_execution_graph_builder execution_graph_builder(
			context,
			*execution_graph.get(),
			instrument_globals,
			fx_entry_point);
		if (!execution_graph_builder.build()) {
			return nullptr;
		}

		instrument_variant->set_fx_execution_graph(execution_graph.release());
	}

	return instrument_variant.release();
}

c_execution_graph_builder::c_execution_graph_builder(
	c_compiler_context &context,
	c_execution_graph &execution_graph,
	const s_instrument_globals &instrument_globals,
	c_ast_node_module_declaration *entry_point)
	: m_context(context)
	, m_execution_graph(execution_graph)
	, m_instrument_globals(instrument_globals)
	, m_entry_point(entry_point)
	, m_graph_trimmer(execution_graph) {}

bool c_execution_graph_builder::build() {
	// The first thing we need to do is initialize all global scope and namespace scope constants
	h_compiler_source_file source_file_handle = h_compiler_source_file::construct(0);
	const s_compiler_source_file &source_file = m_context.get_source_file(source_file_handle);
	c_ast_node_scope *global_scope = source_file.ast->get_as<c_ast_node_scope>();

	c_tracked_scope *tracked_global_scope =
		new c_tracked_scope(nullptr, e_tracked_scope_type::k_namespace, &m_graph_trimmer);
	m_tracked_scopes.emplace_back(tracked_global_scope);
	build_tracked_scope(global_scope, tracked_global_scope);

	if (!initialize_global_scope_constants(global_scope)) {
		return false;
	}

	// Start by adding nodes for all inputs and outputs
	std::vector<h_graph_node> output_node_handles;
	std::vector<h_graph_node> input_node_handles;
	{
		uint32 input_index = 0;
		uint32 output_index = 0;
		for (size_t argument_index = 0; argument_index < m_entry_point->get_argument_count(); argument_index++) {
			c_ast_node_module_declaration_argument *argument = m_entry_point->get_argument(argument_index);
			if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
				input_node_handles.push_back(m_execution_graph.add_input_node(input_index++));
			} else {
				wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);
				output_node_handles.push_back(m_execution_graph.add_output_node(output_index++));
			}
		}
	}

	h_graph_node return_value_node_handle =
		m_execution_graph.add_output_node(c_execution_graph::k_remain_active_output_index);

	// Construct a module call node for the entry point
	std::unique_ptr<c_ast_node_module_call> entry_point_module_call(new c_ast_node_module_call());
	entry_point_module_call->set_resolved_module_declaration(m_entry_point, e_ast_data_mutability::k_variable);

	// Push input nodes onto the stack so they're accessible to the module call
	for (h_graph_node node_handle : input_node_handles) {
		push_expression_result(node_handle);
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t argument_index = 0; argument_index < m_entry_point->get_argument_count(); argument_index++) {
		push_validation_token(argument_index);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	push_ast_node(entry_point_module_call.get());
	if (!evaluate_ast_node_stack()) {
		return false;
	}

	// Hook up the output nodes. There is special-case logic to push the out argument nodes onto the result stack for
	// the entry point module calls.
	wl_assert(m_expression_result_stack.size() == output_node_handles.size() + 1);
	for (uint32 output_index = 0; output_index < output_node_handles.size(); output_index++) {
		pop_validation_token(output_index);
		m_execution_graph.add_edge(m_expression_result_stack.back(), output_node_handles[output_index]);
		pop_and_trim_expression_results(1);
	}

	// Hook up the remain-active output
	pop_validation_token(c_execution_graph::k_remain_active_output_index);
	m_execution_graph.add_edge(m_expression_result_stack.back(), return_value_node_handle);
	pop_and_trim_expression_results(1);
	wl_assert(m_expression_result_stack.empty());

	wl_assert(m_tracked_scopes.size() == 1);
	m_tracked_scopes.pop_back();

#if IS_TRUE(ASSERTS_ENABLED)
	// Validate that there are no temporary reference nodes remaining
	for (h_graph_node node_handle = m_execution_graph.nodes_begin();
		node_handle.is_valid();
		node_handle = m_execution_graph.nodes_next(node_handle)) {
		wl_assert(
			m_execution_graph.get_node_type(node_handle) != e_execution_graph_node_type::k_temporary_reference);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	m_execution_graph.remove_unused_nodes_and_reassign_node_indices();
	return true;
}

void c_execution_graph_builder::build_tracked_scope(c_ast_node_scope *scope_node, c_tracked_scope *tracked_scope) {
	for (size_t item_index = 0; item_index < scope_node->get_scope_item_count(); item_index++) {
		c_ast_node_declaration *declaration_node =
			scope_node->get_scope_item(item_index)->try_get_as<c_ast_node_declaration>();

		c_ast_node_namespace_declaration *namespace_declaration_node =
			declaration_node->try_get_as<c_ast_node_namespace_declaration>();
		if (namespace_declaration_node) {
			// Recursively build namespace scope. Don't make a new tracked scope for this, add everything to the same
			// global scope because we've already resolved namespace lookups.
			build_tracked_scope(namespace_declaration_node->get_scope(), tracked_scope);
		} else if (!tracked_scope->get_tracked_declaration(declaration_node)) {
			// If this node isn't already tracked, add it. We check for this because the same declarations can be
			// imported multiple times under different namespaces, but in terms of tracking, we simply put everything
			// global into the global scope (rather than a scope per-namespace) because namespace access has already
			// been resolved for node handles.
			tracked_scope->add_declaration(declaration_node);
		}
	}
}

bool c_execution_graph_builder::initialize_global_scope_constants(c_ast_node_scope *scope_node) {
	// We've put all global declarations into the global namespace already so simply iterate over them all
	for (size_t item_index = 0; item_index < scope_node->get_scope_item_count(); item_index++) {
		c_ast_node_value_declaration *value_declaration_node =
			scope_node->get_scope_item(item_index)->try_get_as<c_ast_node_value_declaration>();
		if (value_declaration_node) {
			// This global-scope constant may have already been initialized from another global-scope constant which
			// depends on it
			c_tracked_declaration *tracked_declaration =
				m_tracked_scopes.back()->get_tracked_declaration(value_declaration_node);
			if (!tracked_declaration->get_node_handle().is_valid()
				&& !initialize_global_scope_constant(value_declaration_node)) {
				return false;
			}

			wl_assert(tracked_declaration->get_node_handle().is_valid());
		}
	}

	return true;
}

bool c_execution_graph_builder::initialize_global_scope_constant(c_ast_node_value_declaration *value_declaration) {
	// Make sure this is a global-scope constant
	wl_assert(!value_declaration->is_modifiable());
	wl_assert(value_declaration->get_initialization_expression());

	if (m_constants_being_initialized.contains(value_declaration)) {
		m_context.error(
			e_compiler_error::k_self_referential_constant,
			value_declaration->get_source_location(),
			"Failed to initialize value '%s' because its initialization expression refers to itself",
			value_declaration->get_name());
		return false;
	}

	m_constants_being_initialized.insert(value_declaration);

	// We may be evaluating this constant in the middle of evaluating a different constant so we need to back up all
	// relevant state and restore it at the end
	std::vector<std::unique_ptr<c_tracked_scope>> tracked_scopes_backup(std::move(m_tracked_scopes));
	std::vector<s_ast_node_stack_entry> ast_node_stack_backup(std::move(m_ast_node_stack));
	std::vector<h_graph_node> expression_result_stack_backup(std::move(m_expression_result_stack));
#if IS_TRUE(ASSERTS_ENABLED)
	std::vector<s_validation_token_stack_entry> validation_token_stack_backup(std::move(m_validation_token_stack));
#endif // IS_TRUE(ASSERTS_ENABLED)
	h_graph_node return_value_node_handle_backup = m_return_value_node_handle;
	m_return_value_node_handle = h_graph_node::invalid();
	bool return_statement_issued_backup = m_return_statement_issued;
	m_return_statement_issued = false;
	bool break_statement_issued_backup = m_break_statement_issued;
	m_break_statement_issued = false;
	bool continue_statement_issued_backup = m_continue_statement_issued;
	m_continue_statement_issued = false;
	size_t module_call_depth_backup = m_module_call_depth;
	m_module_call_depth = 0;

	// Swap the global scope back onto the tracked scopes list
	wl_assert(m_tracked_scopes.empty());
	m_tracked_scopes.emplace_back(std::move(tracked_scopes_backup[0]));
	wl_assert(!tracked_scopes_backup[0]);

	push_ast_node(value_declaration->get_initialization_expression());
	if (!evaluate_ast_node_stack()) {
		return false;
	}

	// There should be only a single result on the stack
	wl_assert(m_expression_result_stack.size() == 1);
	c_tracked_declaration *tracked_declaration = m_tracked_scopes.back()->get_tracked_declaration(value_declaration);
	wl_assert(!tracked_declaration->get_node_handle().is_valid());
	tracked_declaration->set_node_handle(m_expression_result_stack.back());
	pop_and_trim_expression_results(1);

	wl_assert(m_tracked_scopes.size() == 1);

	// Restore our previous state
	std::swap(m_tracked_scopes[0], tracked_scopes_backup[0]);
	std::swap(tracked_scopes_backup, m_tracked_scopes);
	std::swap(ast_node_stack_backup, m_ast_node_stack);
	std::swap(expression_result_stack_backup, m_expression_result_stack);
#if IS_TRUE(ASSERTS_ENABLED)
	std::swap(validation_token_stack_backup, m_validation_token_stack);
#endif // IS_TRUE(ASSERTS_ENABLED)
	m_return_value_node_handle = return_value_node_handle_backup;
	m_return_statement_issued = return_statement_issued_backup;
	m_break_statement_issued = break_statement_issued_backup;
	m_continue_statement_issued = continue_statement_issued_backup;
	m_module_call_depth = module_call_depth_backup;

	m_constants_being_initialized.erase(value_declaration);
	return true;
}

void c_execution_graph_builder::push_expression_result(h_graph_node node_handle) {
	wl_assert(node_handle.is_valid());
	m_graph_trimmer.add_temporary_reference(node_handle);
	m_expression_result_stack.push_back(node_handle);
#if IS_TRUE(ASSERTS_ENABLED)
	m_validation_token_stack.push_back({ false });
#endif // IS_TRUE(ASSERTS_ENABLED)
}

c_wrapped_array<h_graph_node> c_execution_graph_builder::get_expression_results(size_t count) {
	wl_assert(m_expression_result_stack.size() >= count);
	return c_wrapped_array<h_graph_node>(
		m_expression_result_stack,
		m_expression_result_stack.size() - count,
		count);
}

void c_execution_graph_builder::pop_and_trim_expression_results(size_t count) {
	wl_assert(m_expression_result_stack.size() >= count);
	for (size_t index = 0; index < count; index++) {
#if IS_TRUE(ASSERTS_ENABLED)
		// If we're popping a node handle, the back of the validation token stack should not be a validation token
		wl_assert(!m_validation_token_stack.back().is_validation_token);
		m_validation_token_stack.pop_back();
#endif // IS_TRUE(ASSERTS_ENABLED)

		m_graph_trimmer.remove_temporary_reference(m_expression_result_stack.back());
		m_expression_result_stack.pop_back();
	}
}

void c_execution_graph_builder::push_validation_token(void *validation_token) {
#if IS_TRUE(ASSERTS_ENABLED)
	s_validation_token_stack_entry entry;
	entry.is_validation_token = true;
	entry.pointer_validation_token = validation_token;
	m_validation_token_stack.push_back(entry);
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_execution_graph_builder::push_validation_token(size_t validation_token) {
#if IS_TRUE(ASSERTS_ENABLED)
	s_validation_token_stack_entry entry;
	entry.is_validation_token = true;
	entry.value_validation_token = validation_token;
	m_validation_token_stack.push_back(entry);
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_execution_graph_builder::pop_validation_token(void *validation_token) {
#if IS_TRUE(ASSERTS_ENABLED)
	wl_assert(m_validation_token_stack.back().is_validation_token);
	wl_assert(m_validation_token_stack.back().pointer_validation_token == validation_token);
	m_validation_token_stack.pop_back();
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_execution_graph_builder::pop_validation_token(size_t validation_token) {
#if IS_TRUE(ASSERTS_ENABLED)
	wl_assert(m_validation_token_stack.back().is_validation_token);
	wl_assert(m_validation_token_stack.back().value_validation_token == validation_token);
	m_validation_token_stack.pop_back();
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_execution_graph_builder::push_ast_node(c_ast_node *node) {
	m_ast_node_stack.push_back({ node, 0 });
}

bool c_execution_graph_builder::evaluate_ast_node_stack() {
	while (!m_ast_node_stack.empty()) {
		size_t index = m_ast_node_stack.size() - 1;
		uint64 state = m_ast_node_stack.back().state;
		bool pop_node;
		if (!evaluate_ast_node(m_ast_node_stack.back().ast_node, state, pop_node)) {
			return false;
		}

		if (pop_node) {
			// Don't call pop_back() because the function may have pushed additional nodes
			m_ast_node_stack.erase(m_ast_node_stack.begin() + index);
		} else {
			// Update state
			m_ast_node_stack[index].state = state;
		}
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(c_ast_node *node, uint64 &state, bool &pop_node_out) {
	// We should never encounter these types of nodes:
	wl_assert(!node->try_get_as<c_ast_node_expression_placeholder>());
	wl_assert(!node->try_get_as<c_ast_node_access>());

	switch (node->get_type()) {
	case e_ast_node_type::k_scope:
		return evaluate_ast_node(node->get_as<c_ast_node_scope>(), state, pop_node_out);

	case e_ast_node_type::k_value_declaration:
		return evaluate_ast_node(node->get_as<c_ast_node_value_declaration>(), state, pop_node_out);

	case e_ast_node_type::k_expression_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_expression_statement>(), state, pop_node_out);

	case e_ast_node_type::k_assignment_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_assignment_statement>(), state, pop_node_out);

	case e_ast_node_type::k_return_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_return_statement>(), state, pop_node_out);

	case e_ast_node_type::k_if_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_if_statement>(), state, pop_node_out);

	case e_ast_node_type::k_for_loop:
		return evaluate_ast_node(node->get_as<c_ast_node_for_loop>(), state, pop_node_out);

	case e_ast_node_type::k_break_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_break_statement>(), state, pop_node_out);

	case e_ast_node_type::k_continue_statement:
		return evaluate_ast_node(node->get_as<c_ast_node_continue_statement>(), state, pop_node_out);

	case e_ast_node_type::k_literal:
		return evaluate_ast_node(node->get_as<c_ast_node_literal>(), state, pop_node_out);

	case e_ast_node_type::k_array:
		return evaluate_ast_node(node->get_as<c_ast_node_array>(), state, pop_node_out);

	case e_ast_node_type::k_declaration_reference:
		return evaluate_ast_node(node->get_as<c_ast_node_declaration_reference>(), state, pop_node_out);

	case e_ast_node_type::k_module_call:
		return evaluate_ast_node(node->get_as<c_ast_node_module_call>(), state, pop_node_out);

	case e_ast_node_type::k_convert:
		return evaluate_ast_node(node->get_as<c_ast_node_convert>(), state, pop_node_out);

	case e_ast_node_type::k_subscript:
		return evaluate_ast_node(node->get_as<c_ast_node_subscript>(), state, pop_node_out);

	default:
		pop_node_out = true;
		return true;
	}
}

bool c_execution_graph_builder::evaluate_ast_node(c_ast_node_scope *node, uint64 &state, bool &pop_node_out) {
	// If return, break, or continue has been issued, we immediately stop handling scope items. The surrounding module
	// or for-loop will handle the return/break/continue state.
	if (state < node->get_scope_item_count()
		&& !m_return_statement_issued
		&& !m_break_statement_issued
		&& !m_continue_statement_issued) {
		if (state > 0) {
			pop_validation_token(node);
		}

		push_validation_token(node);
		push_ast_node(node->get_scope_item(cast_integer_verify<size_t>(state)));

		state++;
		pop_node_out = false;
	} else {
		wl_assert(state == node->get_scope_item_count());

		if (state > 0) {
			pop_validation_token(node);
		}

		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_value_declaration *value_declaration,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(value_declaration);

		if (value_declaration->get_initialization_expression()) {
			// Push the initialization expression to be evaluated
			push_ast_node(value_declaration->get_initialization_expression());
		}

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1);

		// Add the declaration, unassigned initially
		c_tracked_declaration *tracked_declaration = m_tracked_scopes.back()->add_declaration(value_declaration);
		wl_assert(tracked_declaration);
		if (value_declaration->get_initialization_expression()) {
			// Assign the initial value
			tracked_declaration->set_node_handle(m_expression_result_stack.back());
			pop_and_trim_expression_results(1);
		}

		pop_validation_token(value_declaration);
		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_expression_statement *expression_statement,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(expression_statement);

		// Push the expression to be evaluated
		push_ast_node(expression_statement->get_expression());

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1);

		if (!expression_statement->get_expression()->get_data_type().is_void()) {
			// If this is a non-void expression, we're throwing away the result
			pop_and_trim_expression_results(1);
		}

		pop_validation_token(expression_statement);
		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_assignment_statement *assignment_statement,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(assignment_statement);

		// Push both RHS expression and LHS index expression (if it exists). Note: make sure to push the LHS index
		// expression last so that it gets evaluated first because it must match the AST build/validation order. This is
		// important because the LHS index expression could assign to a value declaration using an out argument that the
		// RHS expression depends on.
		push_ast_node(assignment_statement->get_rhs_expression());
		if (assignment_statement->get_lhs_index_expression()) {
			push_ast_node(assignment_statement->get_lhs_index_expression());
		}

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1);

		h_graph_node expression_node_handle = m_expression_result_stack.back();
		m_graph_trimmer.add_temporary_reference(expression_node_handle);
		pop_and_trim_expression_results(1);

		h_graph_node index_expression_node_handle = h_graph_node::invalid();
		s_compiler_source_location index_expression_source_location;
		if (assignment_statement->get_lhs_index_expression()) {
			index_expression_node_handle = m_expression_result_stack.back();
			m_graph_trimmer.add_temporary_reference(index_expression_node_handle);
			pop_and_trim_expression_results(1);
			index_expression_source_location = assignment_statement->get_lhs_index_expression()->get_source_location();
		}

		// Update the node pointed to by the declaration
		c_tracked_declaration *tracked_declaration =
			m_tracked_scopes.back()->get_tracked_declaration(assignment_statement->get_lhs_value_declaration());
		wl_assert(tracked_declaration);

		if (!assign_expression_value(
			tracked_declaration,
			expression_node_handle,
			index_expression_node_handle,
			index_expression_source_location)) {
			return false;
		}

		m_graph_trimmer.remove_temporary_reference(expression_node_handle);
		m_graph_trimmer.remove_temporary_reference(index_expression_node_handle);

		pop_validation_token(assignment_statement);
		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_return_statement *return_statement,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(return_statement);

		if (return_statement->get_expression()) {
			push_ast_node(return_statement->get_expression());
		}

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1);

		m_return_statement_issued = true;
		if (return_statement->get_expression()) {
			m_return_value_node_handle = m_expression_result_stack.back();
			m_graph_trimmer.add_temporary_reference(m_return_value_node_handle);
			pop_and_trim_expression_results(1);
		}

		pop_validation_token(return_statement);

		pop_node_out = true;
	}

	return true;
}


bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_if_statement *if_statement,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(if_statement);

		push_ast_node(if_statement->get_expression());

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1 || state == 2);

		// Don't remove this yet - we'll remove it when we pop the if statement
		h_graph_node expression_node_handle = m_expression_result_stack.back();
		bool if_statement_value = m_execution_graph.get_constant_node_bool_value(expression_node_handle);

		// Depending on the result, push the "true" or "false" scope (if it exists)
		c_ast_node_scope *scope = nullptr;
		if (if_statement_value) {
			scope = if_statement->get_true_scope();
		} else if (if_statement->get_false_scope()) {
			scope = if_statement->get_false_scope();
		}

		if (state == 1 && scope) {
			c_tracked_scope *tracked_scope = new c_tracked_scope(
				m_tracked_scopes.back().get(),
				e_tracked_scope_type::k_if_statement,
				&m_graph_trimmer);
			m_tracked_scopes.emplace_back(tracked_scope);

			push_ast_node(scope);

			state = 2;
			pop_node_out = false;
		} else {
			// Pop the expression result
			pop_and_trim_expression_results(1);

			if (state == 2) {
				m_tracked_scopes.pop_back();
			}

			pop_validation_token(if_statement);
			pop_node_out = true;
		}
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_for_loop *for_loop,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(for_loop);

		push_ast_node(for_loop->get_range_expression());

		state = 1;
		pop_node_out = false;
	} else {
		// State represents the current array index - subtract 1 because state 0 is used for initialization
		size_t array_index = cast_integer_verify<size_t>(state - 1);

		if (array_index > 0) {
			// Pop the old scope
			pop_validation_token(array_index - 1);
			m_tracked_scopes.pop_back();
		}

		// Don't pop this node handle from the stack until we're done looping - we need to keep referring to it
		h_graph_node array_node_handle = m_expression_result_stack.back();
		size_t array_count = m_execution_graph.get_node_incoming_edge_count(array_node_handle);

		m_continue_statement_issued = false;
		if (array_index < array_count && !m_break_statement_issued) {
			push_validation_token(array_index);

			c_tracked_scope *tracked_scope = new c_tracked_scope(
				m_tracked_scopes.back().get(),
				e_tracked_scope_type::k_for_loop,
				&m_graph_trimmer);
			m_tracked_scopes.emplace_back(tracked_scope);

			// Add the loop value to the scope
			h_graph_node element_node_handle =
				m_execution_graph.get_node_indexed_input_incoming_edge_handle(array_node_handle, array_index, 0);
			c_tracked_declaration *tracked_declaration =
				m_tracked_scopes.back()->add_declaration(for_loop->get_value_declaration());
			tracked_declaration->set_node_handle(element_node_handle);

			push_ast_node(for_loop->get_loop_scope());

			state++;
			pop_node_out = false;
		} else {
			// We're done looping, pop the array we looped over
			pop_and_trim_expression_results(1);

			m_break_statement_issued = false;
			m_continue_statement_issued = false;

			pop_validation_token(for_loop);
			pop_node_out = true;
		}
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_break_statement *break_statement,
	uint64 &state,
	bool &pop_node_out) {
	m_break_statement_issued = true;
	pop_node_out = true;
	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_continue_statement *continue_statement,
	uint64 &state,
	bool &pop_node_out) {
	m_continue_statement_issued = true;
	pop_node_out = true;
	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_literal *literal,
	uint64 &state,
	bool &pop_node_out) {
	wl_assert(state == 0);

	// Construct a constant node
	h_graph_node constant_node_handle = h_graph_node::invalid();
	wl_assert(literal->get_data_type().get_data_mutability() == e_ast_data_mutability::k_constant);
	switch (literal->get_data_type().get_primitive_type()) {
	case e_ast_primitive_type::k_real:
		constant_node_handle = m_execution_graph.add_constant_node(literal->get_real_value());
		break;

	case e_ast_primitive_type::k_bool:
		constant_node_handle = m_execution_graph.add_constant_node(literal->get_bool_value());
		break;

	case e_ast_primitive_type::k_string:
		constant_node_handle = m_execution_graph.add_constant_node(literal->get_string_value());
		break;

	default:
		wl_unreachable();
	}

	wl_assert(constant_node_handle.is_valid());
	push_expression_result(constant_node_handle);

	pop_node_out = true;
	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_array *array,
	uint64 &state,
	bool &pop_node_out) {
	if (state == 0) {
		push_validation_token(array);

		// Push each element expression in reverse order
		for (size_t reverse_element_index = 0;
			reverse_element_index < array->get_element_count();
			reverse_element_index++) {
			push_ast_node(array->get_element(array->get_element_count() - reverse_element_index - 1));
		}

		state = 1;
		pop_node_out = false;
	} else {
		wl_assert(state == 1);

		h_graph_node array_node_handle = m_execution_graph.add_array_node(
			native_module_data_type_from_ast_data_type(array->get_data_type().get_element_type().get_data_type()));
		for (h_graph_node element_node_handle : get_expression_results(array->get_element_count())) {
			m_execution_graph.add_array_value(array_node_handle, element_node_handle);
		}

		pop_and_trim_expression_results(array->get_element_count());

		pop_validation_token(array);

		push_expression_result(array_node_handle);
		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_declaration_reference *declaration_reference,
	uint64 &state,
	bool &pop_node_out) {
	wl_assert(state == 0);

	// All declaration references should resolve to a single node
	wl_assert(declaration_reference->get_references().get_count() == 1);
	c_ast_node_value_declaration *value_declaration =
		declaration_reference->get_references()[0]->get_as<c_ast_node_value_declaration>();
	c_tracked_declaration *tracked_declaration = m_tracked_scopes.back()->get_tracked_declaration(value_declaration);

	if (!tracked_declaration->get_node_handle().is_valid()) {
		// If this value doesn't have an associated node, it means we're initializing global-scope constants and one of
		// the constants refers to another global-scope constant which hasn't yet been initialized. In this case, we
		// pause evaluation of the first constant and evaluate the dependent one, then swap back and continue
		// initializing the first constant. Self-referential constants are automatically detected and an error is
		// raised.
		if (!initialize_global_scope_constant(value_declaration)) {
			return false;
		}
	}

	wl_assert(tracked_declaration);
	wl_assert(tracked_declaration->get_node_handle().is_valid());
	push_expression_result(tracked_declaration->get_node_handle());

	pop_node_out = true;
	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_module_call *module_call,
	uint64 &state,
	bool &pop_node_out) {
	// There is some special logic if this is the entry point module call
	bool is_entry_point_module_call = m_constants_being_initialized.empty() && m_ast_node_stack.size() == 1;

	c_ast_node_module_declaration *module_declaration = module_call->get_resolved_module_declaration();
	size_t argument_count = module_declaration->get_argument_count();

	if (state == 0) {
		if (m_module_call_depth >= k_max_module_call_depth) {
			m_context.error(
				e_compiler_error::k_module_call_depth_limit_exceeded,
				"Module call depth limit of %llu has been exceeded",
				k_max_module_call_depth);
			return false;
		}

		m_module_call_depth++;

		// Start by pushing the input argument expressions in reverse order so they get evaluated in forward order
		for (size_t reverse_argument_index = 0; reverse_argument_index < argument_count; reverse_argument_index++) {
			size_t argument_index = argument_count - reverse_argument_index - 1;
			if (is_entry_point_module_call) {
				// The input graph nodes are already on the result stack for the entry point case
				pop_validation_token(argument_index);
			} else {
				c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
				c_ast_node_expression *argument_expression =
					module_call->get_resolved_module_argument_expression(argument_index);
				if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
					push_ast_node(argument_expression);
				} else {
					wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);
					c_ast_node_subscript *subscript = argument_expression->try_get_as<c_ast_node_subscript>();
					if (subscript) {
						// Push the subscript index expression
						push_ast_node(subscript->get_index_expression());
					}
				}
			}
		}

		push_validation_token(module_call);

		state = 1;
		pop_node_out = false;
	} else if (state == 1) {
		// All input arguments have been evaluated, add them to the module scope and start evaluating. Store off any out
		// argument subscript index expression results for later.
		c_tracked_scope *tracked_scope = new c_tracked_scope(
			m_tracked_scopes.back().get(),
			e_tracked_scope_type::k_module,
			&m_graph_trimmer);
		m_tracked_scopes.emplace_back(tracked_scope);

		if (is_entry_point_module_call) {
			// The input argument nodes were already on the stack before we pushed this validation token so pop it first
			pop_validation_token(module_call);
		}

		std::vector<h_graph_node> subscript_index_node_handles;
		for (size_t reverse_argument_index = 0; reverse_argument_index < argument_count; reverse_argument_index++) {
			size_t argument_index = argument_count - reverse_argument_index - 1;
			c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
			c_tracked_declaration *tracked_declaration =
				tracked_scope->add_declaration(argument->get_value_declaration());
			wl_assert(tracked_declaration);
			if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
				// Grab input arguments off the result stack
				tracked_declaration->set_node_handle(m_expression_result_stack.back());
				pop_and_trim_expression_results(1);
			} else {
				wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);

				// We don't pass in any module call arguments if this is the entry point, so don't try to look at them
				if (is_entry_point_module_call) {
					wl_assert(module_call->get_argument_count() == 0);
				} else {
					c_ast_node_expression *argument_expression =
						module_call->get_resolved_module_argument_expression(argument_index);
					c_ast_node_subscript *subscript = argument_expression->try_get_as<c_ast_node_subscript>();
					if (subscript) {
						// Grab the subscript index off the result stack
						subscript_index_node_handles.push_back(m_expression_result_stack.back());
						m_graph_trimmer.add_temporary_reference(m_expression_result_stack.back());
						pop_and_trim_expression_results(1);
					}
				}
			}
		}

		if (!is_entry_point_module_call) {
			// This validation token was pushed before the input arguments were evaluated so pop it last
			pop_validation_token(module_call);
		}

		push_validation_token(module_call);

		// Push subscript index node handles back onto the stack so we don't lose them
		for (h_graph_node subscript_index_node_handle : subscript_index_node_handles) {
			push_expression_result(subscript_index_node_handle);
			m_graph_trimmer.remove_temporary_reference(subscript_index_node_handle);
		}

		if (module_declaration->get_native_module_uid().is_valid()) {
			// Call the native module which will fill in the tracked declarations in the module scope
			if (!issue_native_module_call(module_declaration, module_call)) {
				return false;
			}
		} else {
			// Push the scope if this is a script-defined module
			push_ast_node(module_declaration->get_body_scope());
		}

		state = 2;
		pop_node_out = false;
	} else {
		wl_assert(state == 2);

		m_return_statement_issued = false;

		// A statement related to a for-loop should never make it down to the module body scope
		wl_assert(!m_break_statement_issued);
		wl_assert(!m_continue_statement_issued);

		// If this is the entry point module call, we have different logic for out arguments
		if (!is_entry_point_module_call) {
			// Assign any out arguments to the declarations provided. For array subscripting, we'll grab the index node
			// handles from the stack. Since we pulled them into subscript_index_node_handles in reverse and then pushed
			// them back onto the stack in that same order, we can grab them off the end of the stack in forward order,
			// which is the order we expect for assignment.
			c_tracked_scope *tracked_scope = m_tracked_scopes.back().get();
			c_tracked_scope *parent_tracked_scope = m_tracked_scopes[m_tracked_scopes.size() - 2].get();
			for (size_t argument_index = 0; argument_index < argument_count; argument_index++) {
				c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
				if (argument->get_argument_direction() != e_ast_argument_direction::k_out) {
					continue;
				}

				// The provided out argument is either a declaration handle or an array subscript, which consists of
				// a declaration reference plus an index expression (which we already evaluated). Find the associated
				// tracked declaration.
				c_tracked_declaration *tracked_declaration =
					tracked_scope->get_tracked_declaration(argument->get_value_declaration());
				c_ast_node_expression *argument_expression =
					module_call->get_resolved_module_argument_expression(argument_index);
				c_ast_node_declaration_reference *declaration_reference =
					argument_expression->try_get_as<c_ast_node_declaration_reference>();

				h_graph_node subscript_index_node_handle = h_graph_node::invalid();
				s_compiler_source_location subscript_index_expression_source_location;
				if (!declaration_reference) {
					c_ast_node_subscript *subscript = argument_expression->get_as<c_ast_node_subscript>();
					declaration_reference = subscript->get_reference();

					// Grab the subscript index off the result stack
					subscript_index_node_handle = m_expression_result_stack.back();
					m_graph_trimmer.add_temporary_reference(subscript_index_node_handle);
					pop_and_trim_expression_results(1);

					subscript_index_expression_source_location =
						subscript->get_index_expression()->get_source_location();
				}

				// All declaration references should resolve to a single node
				wl_assert(declaration_reference->get_references().get_count() == 1);
				c_ast_node_value_declaration *value_declaration =
					declaration_reference->get_references()[0]->get_as<c_ast_node_value_declaration>();
				c_tracked_declaration *target_tracked_declaration =
					m_tracked_scopes.back()->get_tracked_declaration(value_declaration);

				if (!assign_expression_value(
					target_tracked_declaration,
					tracked_declaration->get_node_handle(),
					subscript_index_node_handle,
					subscript_index_expression_source_location)) {
					return false;
				}

				m_graph_trimmer.remove_temporary_reference(subscript_index_node_handle);
			}
		}

		pop_validation_token(module_call);

		// If this is a non-void module, push the return value onto the stack
		wl_assert(m_return_value_node_handle.is_valid() != module_declaration->get_return_type().is_void());
		if (m_return_value_node_handle.is_valid()) {
			push_expression_result(m_return_value_node_handle);
			m_graph_trimmer.remove_temporary_reference(m_return_value_node_handle);
			m_return_value_node_handle = h_graph_node::invalid();

			if (is_entry_point_module_call) {
				push_validation_token(c_execution_graph::k_remain_active_output_index);
			}
		}

		// Special-case: if this is the entry point module, also add the out arguments to the result stack
		if (is_entry_point_module_call) {
#if IS_TRUE(ASSERTS_ENABLED)
			size_t output_count = 0;
			for (size_t argument_index = 0; argument_index < argument_count; argument_index++) {
				c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
				if (argument->get_argument_direction() == e_ast_argument_direction::k_out) {
					output_count++;
				}
			}

			size_t reverse_output_index = 0;
#endif // IS_TRUE(ASSERTS_ENABLED)

			c_tracked_scope *tracked_scope = m_tracked_scopes.back().get();
			for (size_t reverse_argument_index = 0; reverse_argument_index < argument_count; reverse_argument_index++) {
				size_t argument_index = argument_count - reverse_argument_index - 1;
				c_ast_node_module_declaration_argument *argument = module_declaration->get_argument(argument_index);
				c_tracked_declaration *tracked_declaration =
					tracked_scope->get_tracked_declaration(argument->get_value_declaration());
				wl_assert(tracked_declaration);
				if (argument->get_argument_direction() == e_ast_argument_direction::k_out) {
					push_expression_result(tracked_declaration->get_node_handle());

#if IS_TRUE(ASSERTS_ENABLED)
					push_validation_token(output_count - reverse_output_index - 1);
					reverse_output_index++;
#endif // IS_TRUE(ASSERTS_ENABLED)
				}
			}
		}

		m_tracked_scopes.pop_back();
		m_module_call_depth--;

		pop_node_out = true;
	}

	return true;
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_convert *convert,
	uint64 &state,
	bool &pop_node_out) {
	// Convert is a no-op at this stage
	return evaluate_ast_node(convert->get_expression(), state, pop_node_out);
}

bool c_execution_graph_builder::evaluate_ast_node(
	c_ast_node_subscript *subscript,
	uint64 &state,
	bool &pop_node_out) {
	// When used in an expression (as opposed to an assignment or out parameter), subscript is just a module call
	return evaluate_ast_node(subscript->get_module_call(), state, pop_node_out);
}

bool c_execution_graph_builder::assign_expression_value(
	c_tracked_declaration *tracked_declaration,
	h_graph_node expression_node_handle,
	h_graph_node index_expression_node_handle,
	const s_compiler_source_location &index_expression_source_location) {
	// Update the node pointed to by the declaration
	if (index_expression_node_handle.is_valid()) {
		// This is an array subscript assignment
		h_graph_node array_node_handle = tracked_declaration->get_node_handle();
		size_t array_count = m_execution_graph.get_node_incoming_edge_count(array_node_handle);
		real32 array_index_real = m_execution_graph.get_constant_node_real_value(index_expression_node_handle);
		size_t array_index;
		if (!get_and_validate_native_module_array_index(array_index_real, array_count, array_index)) {
			m_context.error(
				e_compiler_error::k_array_index_out_of_bounds,
				index_expression_source_location,
				"Array index '%f' is out of bounds for array of size '%llu'",
				array_index_real,
				array_count);
			return false;
		}

		if (m_execution_graph.get_node_outgoing_edge_count(array_node_handle) == 1) {
			// Optimization: if we're the only thing referencing this array, just swap out a single node rather than
			// copying the entire array
			h_graph_node old_element_node_handle = m_execution_graph.set_array_value_at_index(
				array_node_handle,
				cast_integer_verify<uint32>(array_index),
				expression_node_handle);
			m_graph_trimmer.try_trim_node(old_element_node_handle);
		} else {
			// Copy the entire array and redirect the declaration to point at the copy
			h_graph_node new_array_node_handle = m_execution_graph.add_array_node(
				m_execution_graph.get_node_data_type(array_node_handle).get_element_type());
			for (size_t index = 0; index < array_count; index++) {
				h_graph_node element_node_handle = (index == array_index)
					? expression_node_handle
					: m_execution_graph.get_node_indexed_input_incoming_edge_handle(
						array_node_handle,
						array_index,
						0);
				m_execution_graph.add_array_value(new_array_node_handle, element_node_handle);
			}
		}
	} else {
		// This is an ordinary assignment
		tracked_declaration->set_node_handle(expression_node_handle);
	}

	return true;
}

bool c_execution_graph_builder::issue_native_module_call(
	c_ast_node_module_declaration *native_module_declaration,
	c_ast_node_module_call *native_module_call) {
	wl_assert(native_module_declaration->get_native_module_uid().is_valid());
	h_native_module native_module_handle =
		c_native_module_registry::get_native_module_handle(native_module_declaration->get_native_module_uid());
	const s_native_module &native_module =
		c_native_module_registry::get_native_module(native_module_handle);

	h_graph_node native_module_call_node_handle =
		m_execution_graph.add_native_module_call_node(native_module_handle);

	// Pass in the inputs
	size_t input_index = 0;
	size_t argument_index = 0;
	for (size_t native_module_argument_index = 0;
		native_module_argument_index < native_module.argument_count;
		native_module_argument_index++) {
		if (native_module_argument_index == native_module.return_argument_index) {
			// Nothing to do, this is an output
		} else {
			c_ast_node_module_declaration_argument *argument = native_module_declaration->get_argument(argument_index);
			c_tracked_declaration *tracked_declaration =
				m_tracked_scopes.back()->get_tracked_declaration(argument->get_value_declaration());
			if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
				// Hook up the input source node
				h_graph_node input_node_handle =
					m_execution_graph.get_node_incoming_edge_handle(native_module_call_node_handle, input_index);
				m_execution_graph.add_edge(tracked_declaration->get_node_handle(), input_node_handle);
				input_index++;
			} else {
				wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);
			}
		}

		argument_index++;
	}

	// Attempt to call the native module. If it can't be called at compile-time, the native module node will remain
	// untouched. Otherise, the native module node will be removed and the output nodes will be replaced with the
	// results of the call.
	bool did_call;
	std::vector<h_graph_node> output_node_handles;
	if (!try_call_native_module(
		m_context,
		m_graph_trimmer,
		m_instrument_globals,
		native_module_call_node_handle,
		native_module_call->get_source_location(),
		did_call,
		&output_node_handles)) {
		return false;
	}

	// Store off the outputs, which will be native module output nodes (if the native module couldn't be called) or the
	// native module call results.
	size_t output_index = 0;
	argument_index = 0;
	for (size_t native_module_argument_index = 0;
		native_module_argument_index < native_module.argument_count;
		native_module_argument_index++) {
		if (native_module_argument_index == native_module.return_argument_index) {
			// Hook up the return output
			m_return_value_node_handle = did_call
				? output_node_handles[output_index]
				: m_execution_graph.get_node_outgoing_edge_handle(native_module_call_node_handle, output_index);
			m_graph_trimmer.add_temporary_reference(m_return_value_node_handle);
			output_index++;
		} else {
			c_ast_node_module_declaration_argument *argument = native_module_declaration->get_argument(argument_index);
			c_tracked_declaration *tracked_declaration =
				m_tracked_scopes.back()->get_tracked_declaration(argument->get_value_declaration());
			if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
				// Nothing to do
			} else {
				wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);
				// Store off the output node
				h_graph_node output_node_handle = did_call
					? output_node_handles[output_index]
					: m_execution_graph.get_node_outgoing_edge_handle(
						native_module_call_node_handle,
						output_index);
				tracked_declaration->set_node_handle(output_node_handle);
				output_index++;
			}
		}

		argument_index++;
	}

	return true;
}

static c_native_module_data_type native_module_data_type_from_ast_data_type(c_ast_data_type ast_data_type) {
	switch (ast_data_type.get_primitive_type()) {
	case e_ast_primitive_type::k_real:
		return c_native_module_data_type(e_native_module_primitive_type::k_real, ast_data_type.is_array());

	case e_ast_primitive_type::k_bool:
		return c_native_module_data_type(e_native_module_primitive_type::k_bool, ast_data_type.is_array());

	case e_ast_primitive_type::k_string:
		return c_native_module_data_type(e_native_module_primitive_type::k_string, ast_data_type.is_array());

	default:
		wl_unreachable();
		return c_native_module_data_type::invalid();
	}
}
