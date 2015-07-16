#include "compiler/ast_validator.h"
#include <deque>
#include <map>
#include <stack>
#include <algorithm>

class c_ast_validator_visitor : public c_ast_node_const_visitor {
public:
	enum e_pass {
		k_pass_register_global_identifiers,
		k_pass_validate_syntax,

		k_pass_count
	};

private:
	struct s_expression_result {
		// Type which the expression resolved to
		e_data_type type;
		// If non-empty, this result contains an assignable named value and can be used as an out argument
		std::string identifier_name;
	};

	struct s_named_value {
		// Last statement number this named value was assigned on, or -1 if it has not yet been assigned
		int32 last_statement_assigned;

		// Last statement number this named value was used on, or -1 if it has not yet been used
		int32 last_statement_used;

		// A named value cannot be both assigned and used in the same statement
		// A named value cannot be used without first being assigned
	};

	// $TODO remove
	enum e_expression_expectation {
		k_expression_expectation_value_or_valueless,	// This expression can either be a value or have no value
		k_expression_expectation_value,					// This expression must have a value
		k_expression_expectation_assignment,			// This expression must be a single named value identifier

		k_expression_expectation_count
	};

	struct s_scope {
		// The module this scope belongs to, or null
		const c_ast_node_module_declaration *module_for_scope;

		// The current statement
		int32 current_statement;

		// The statement number which return occurs on
		int32 return_statement;

		// Identifiers in this scope
		std::map<std::string, const c_ast_node *> identifiers;

		// Named values in this scope
		std::map<const c_ast_node *, s_named_value> named_values;

		s_scope() {
			module_for_scope = nullptr;
			current_statement = 0;
			return_statement = -1;
		}
	};

	struct s_module_call {
		bool visited;
		bool marked;
		const c_ast_node_module_declaration *module;
		// List of modules called by this module
		std::vector<const c_ast_node_module_declaration *> subcalls;

		s_module_call() {
			visited = false;
			marked = false;
			module = nullptr;
		}
	};

	// The current pass
	e_pass m_pass;

	// Vector into which we accumulate errors
	std::vector<s_compiler_result> *m_errors;

	// If this is set, then we're about to enter the body scope for this module
	const c_ast_node_module_declaration *m_module_for_next_scope;

	// Set to true when the entry point is found
	bool m_entry_point_found;

	// Stack of scopes - global scope is at the bottom
	std::deque<s_scope> m_scope_stack;
	size_t m_scope_depth;

	// When we build expressions, we return the results through this stack
	std::stack<s_expression_result> m_expression_stack;

	// $TODO remove
	// Used to validate that expressions resolve to their expected values
	// When sub-expressions are discovered, expecatations are pushed to this stack in reverse argument order
	std::stack<e_expression_expectation> expression_expectation_stack;

	// Keeps track of the graph of module calls to detect cycles
	std::vector<s_module_call> m_module_calls;

	void add_module_subcall(const c_ast_node_module_declaration *caller, const c_ast_node_module_declaration *callee) {
		// Add this module to the list of module calls made from the current module, if it has not yet appeared
		size_t module_call_index;
		for (module_call_index = 0; module_call_index < m_module_calls.size(); module_call_index++) {
			if (m_module_calls[module_call_index].module == caller) {
				break;
			}
		}
		wl_assert(module_call_index < m_module_calls.size());

		s_module_call &module_call = m_module_calls[module_call_index];
		wl_assert(module_call.module == caller);
		bool subcall_found = false;
		for (size_t subcall_index = 0; !subcall_found && subcall_index < module_call.subcalls.size(); subcall_index++) {
			if (module_call.subcalls[subcall_index] == callee) {
				subcall_found = true;
			}
		}

		if (!subcall_found) {
			module_call.subcalls.push_back(callee);
		}
	}

	bool visit_module(size_t index) {
		s_module_call &module_call = m_module_calls[index];

		if (module_call.marked) {
			// Cycle
			return false;
		}

		if (!module_call.visited) {
			module_call.marked = true;
			for (size_t edge = 0; edge < module_call.subcalls.size(); edge++) {
				// Find the index for this edge
				size_t edge_index;
				for (edge_index = 0; edge_index < m_module_calls.size(); edge_index++) {
					if (m_module_calls[edge_index].module == module_call.subcalls[edge]) {
						break;
					}
				}
				wl_assert(edge_index != m_module_calls.size());

				if (!visit_module(edge_index)) {
					return false;
				}
			}

			module_call.visited = true;
			module_call.marked = false;
		}

		return true;
	}

	// Adds an identifier to the top scope if it does not exist in this scope or lower
	bool add_identifier_to_scope(const std::string &name, const c_ast_node *node) {
		bool found = false;
		for (size_t index = 0; !found && index < m_scope_stack.size(); index++) {
			const s_scope &scope = m_scope_stack[index];
			found = (scope.identifiers.find(name) != scope.identifiers.end());
		}

		if (!found) {
			m_scope_stack.front().identifiers.insert(std::make_pair(name, node));
		} else {
			s_compiler_result error;
			error.result = k_compiler_result_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + name + "' is already in use";
			m_errors->push_back(error);
		}

		return !found;
	}

	// Returns the identifier associated with the given name, or null
	const c_ast_node *get_identifier(const std::string &name) const {
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			const s_scope &scope = m_scope_stack[index];
			auto match = scope.identifiers.find(name);
			if (match != scope.identifiers.end()) {
				return match->second;
			}
		}

		return nullptr;
	}

	void add_named_value_to_scope(const c_ast_node *node, const s_named_value &named_value) {
		s_scope &scope = m_scope_stack.front();
		wl_assert(scope.named_values.find(node) == scope.named_values.end());
		scope.named_values.insert(std::make_pair(node, named_value));
	}

	s_named_value *get_named_value(const c_ast_node *node) {
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			s_scope &scope = m_scope_stack[index];
			auto match = scope.named_values.find(node);
			if (match != scope.named_values.end()) {
				return &match->second;
			}
		}

		return nullptr;
	}

	void detect_statements_after_return(s_scope &scope, const c_ast_node *node) {
		// If a return statement exists and this statement immediately follows, emit an error
		// The second condition is so we only emit one error for this
		if (scope.return_statement >= 0 && scope.current_statement == scope.return_statement + 1) {
			s_compiler_result error;
			error.result = k_compiler_result_statements_after_return;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() +
				"' contains statements after the return statement";
			m_errors->push_back(error);
		}
	}

	void push_scope() {
		if (m_scope_depth == 0 && !m_scope_stack.empty()) {
			// On the second pass, the global scope already exists
			wl_assert(m_pass == k_pass_validate_syntax);
		} else {
			m_scope_stack.push_front(s_scope());
		}
		m_scope_depth++;
	}

	void undeclared_identifier_error(s_compiler_source_location source_location, const std::string &name) {
		s_compiler_result error;
		error.result = k_compiler_result_undeclared_identifier;
		error.source_location = source_location;
		error.message = "Identifier '" + name + "' has not been declared";
		m_errors->push_back(error);
	}

	void type_mismatch_error(s_compiler_source_location source_location, e_data_type expected, e_data_type actual) {
		s_compiler_result error;
		error.result = k_compiler_result_type_mismatch;
		error.source_location = source_location;
		error.message = "Type mismatch: expected '" +
			std::string(get_data_type_string(expected)) + "', got '" +
			std::string(get_data_type_string(actual)) + "'";
		m_errors->push_back(error);
	}

public:
	c_ast_validator_visitor(std::vector<s_compiler_result> *error_accumulator) {
		wl_assert(error_accumulator);

		m_pass = k_pass_count;
		m_errors = error_accumulator;
		m_module_for_next_scope = nullptr;
		m_entry_point_found = false;
		m_scope_depth = 0;
	}

	void set_pass(e_pass pass) {
		wl_assert(pass < k_pass_count);
		m_pass = pass;
	}

	void detect_module_call_cycles() {
		// Validate that module calls aren't cyclic by attempting to find a topological ordering
		while (true) {
			// Find an unvisited module
			size_t index;
			for (index = 0; index < m_module_calls.size(); index++) {
				if (!m_module_calls[index].visited) {
					break;
				}
			}

			if (index < m_module_calls.size()) {
				if (!visit_module(index)) {
					s_compiler_result error;
					error.result = k_compiler_result_cyclic_module_call;
					error.message = "Cyclic module call(s) detected";
					m_errors->push_back(error);
					break;
				}
			} else {
				break;
			}
		}
	}

	bool was_entry_point_found() const {
		return m_entry_point_found;
	}

	virtual bool begin_visit(const c_ast_node_scope *node) {
		push_scope();
		s_scope &scope = m_scope_stack.front();

		if (m_module_for_next_scope) {
			wl_assert(m_pass == k_pass_validate_syntax);
			scope.module_for_scope = m_module_for_next_scope;
			m_module_for_next_scope = nullptr;
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_scope *node) {
		s_scope &scope = m_scope_stack.front();

		// Process the end of modules here:
		if (scope.module_for_scope) {
			// Make sure a return statement was found if necessary
			if (scope.module_for_scope->get_has_return_value() &&
				scope.return_statement < 0) {
				s_compiler_result error;
				error.result = k_compiler_result_missing_return_statement;
				error.source_location = scope.module_for_scope->get_source_location();
				error.message = "Module '" + scope.module_for_scope->get_name() + "' is missing return statement";
				m_errors->push_back(error);
			}

			// Make sure all out arguments have been assigned
			for (size_t index = 0; index < scope.module_for_scope->get_argument_count(); index++) {
				const c_ast_node_named_value_declaration *argument = scope.module_for_scope->get_argument(index);

				if (argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out) {
					// Look up the context
					s_named_value *match = get_named_value(argument);
					// Context must already exist for this argument
					wl_assert(match);

					if (match->last_statement_assigned < 0) {
						s_compiler_result error;
						error.result = k_compiler_result_unassigned_output;
						error.source_location = argument->get_source_location();
						error.message = "Assignment for output argument '" + argument->get_name() + "' is missing";
						m_errors->push_back(error);
					}
				}
			}

			// Don't remove the module's name from the scope below, keep it around from the first pass
		}

		m_scope_depth--;
		if (m_scope_depth > 0) {
			// Don't pop global scope
			m_scope_stack.pop_front();
		}
	}

	virtual bool begin_visit(const c_ast_node_module_declaration *node) {
		if (m_pass == k_pass_register_global_identifiers) {
			// Add the module's name to the list of identifiers seen so far
			wl_assert(m_scope_stack.size() == 1);
			add_identifier_to_scope(node->get_name(), node);

			m_module_calls.push_back(s_module_call());
			m_module_calls.back().module = node;

			if (!m_entry_point_found && node->get_name() == k_entry_point_name) {
				m_entry_point_found = true;

				// Should have no return value and only out arguments
				if (node->get_has_return_value()) {
					s_compiler_result error;
					error.result = k_compiler_result_invalid_entry_point;
					error.source_location = node->get_source_location();
					error.message = "Entry point '" + node->get_name() + "' must not return a value";
					m_errors->push_back(error);
				}

				bool all_out = true;
				for (size_t arg = 0; all_out && arg < node->get_argument_count(); arg++) {
					if (node->get_argument(arg)->get_qualifier() !=
						c_ast_node_named_value_declaration::k_qualifier_out) {
						all_out = false;
					}
				}

				if (!all_out) {
					s_compiler_result error;
					error.result = k_compiler_result_invalid_entry_point;
					error.source_location = node->get_source_location();
					error.message = "Entry point '" + node->get_name() + "' must only take out arguments";
					m_errors->push_back(error);
				}
			}

			// Don't enter the scope in this pass
			return false;
		} else if (!node->get_is_native()) {
			wl_assert(m_pass == k_pass_validate_syntax);

			// Prepare for processing in scope visitor
			wl_assert(!m_module_for_next_scope);
			m_module_for_next_scope = node;
			return true;
		} else {
			wl_assert(m_pass == k_pass_validate_syntax);
			// Don't visit native modules
			return false;
		}
	}

	virtual void end_visit(const c_ast_node_module_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		s_scope &scope = m_scope_stack.front();

		detect_statements_after_return(scope, node);

		// Add the identifier to the scope
		if (add_identifier_to_scope(node->get_name(), node)) {
			s_named_value named_value;
			if (node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_none ||
				node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out) {
				// Locals and outputs start out being unassigned
				named_value.last_statement_assigned = -1;
			} else {
				// Module inputs are already assigned at the beginning of the module
				wl_assert(node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_in);
				named_value.last_statement_assigned = m_scope_stack.front().current_statement;
			}

			named_value.last_statement_used = -1;
			add_named_value_to_scope(node, named_value);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_declaration *node) {
		m_scope_stack.front().current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_assignment *node) {
		s_scope &scope = m_scope_stack.front();
		detect_statements_after_return(scope, node);

		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();

		// If a valid identifier has been provided, update its last used statement
		const c_ast_node *identifier = get_identifier(result.identifier_name);
		if (identifier) {
			s_named_value *named_value = get_named_value(identifier);
			wl_assert(named_value);
			named_value->last_statement_used = scope.current_statement;
		}

		if (node->get_named_value().empty()) {
			// If this assignment node has no name, it means it's just a placeholder for an expression; nothing to do.
		} else {
			// First make sure this is actually a named value
			const c_ast_node *identifier = get_identifier(node->get_named_value());
			if (!identifier) {
				undeclared_identifier_error(node->get_source_location(), node->get_named_value());
			} else if (identifier->get_type() != k_ast_node_type_named_value_declaration) {
				s_compiler_result error;
				error.result = k_compiler_result_type_mismatch;
				error.source_location = node->get_source_location();
				error.message = "Identifier '" + node->get_named_value() + "' is not a named value";
				m_errors->push_back(error);
			} else if (result.type != k_data_type_value) {
				type_mismatch_error(node->get_source_location(), k_data_type_value, result.type);
			} else {
				// Look up the context
				s_named_value *match = get_named_value(identifier);
				// Since we've verified that this is a named value, it must already have a context
				wl_assert(match);

				if (match->last_statement_assigned == scope.current_statement) {
					// Make sure we're not assigning to a value we used as an output parameter; this is ambiguous
					// However, it's okay if we used the named value on this same statement, e.g. x := x + 1;
					s_compiler_result error;
					error.result = k_compiler_result_ambiguous_named_value_assignment;
					error.source_location = node->get_source_location();
					error.message = "Ambiguous assignment for named value '" + node->get_named_value() + "'";
					m_errors->push_back(error);
				} else {
					match->last_statement_assigned = scope.current_statement;
				}
			}
		}

		wl_assert(m_expression_stack.empty());

		scope.current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		s_scope &scope = m_scope_stack.front();

		// Return statements will fail at the parser stage if they are outside of a module
		wl_assert(scope.module_for_scope);

		detect_statements_after_return(scope, node);

		if (scope.return_statement >= 0) {
			s_compiler_result error;
			error.result = k_compiler_result_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() + "' has multiple return statements";
			m_errors->push_back(error);
		}

		if (!scope.module_for_scope->get_has_return_value()) {
			s_compiler_result error;
			error.result = k_compiler_result_extraneous_return_statement;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() + "' has extraneous return statement";
			m_errors->push_back(error);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_return_statement *node) {
		s_scope &scope = m_scope_stack.front();
		detect_statements_after_return(scope, node);

		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();

		// If a valid identifier has been provided, update its last used statement
		const c_ast_node *identifier = get_identifier(result.identifier_name);
		if (identifier) {
			s_named_value *named_value = get_named_value(identifier);
			wl_assert(named_value);
			named_value->last_statement_used = scope.current_statement;
		}

		if (result.type != k_data_type_value) {
			s_compiler_result error;
			error.result = k_compiler_result_type_mismatch;
			error.source_location = node->get_source_location();
			error.message = "Attempting to return '" + std::string(get_data_type_string(result.type)) + "'";
			m_errors->push_back(error);
		}

		if (scope.return_statement < 0) {
			scope.return_statement = scope.current_statement;
		}

		wl_assert(m_expression_stack.empty());

		scope.current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_expression *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_expression *node) {
		// The expression sub-type should have returned something
		wl_assert(!m_expression_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_constant *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_constant *node) {
		s_expression_result result;
		result.type = k_data_type_value;
		m_expression_stack.push(result);
	}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {
		s_expression_result result;
		result.type = k_data_type_value;
		result.identifier_name = node->get_name();
		m_expression_stack.push(result);

		s_scope &scope = m_scope_stack.front();

		// First make sure this is actually a named value
		const c_ast_node *identifier = get_identifier(node->get_name());
		if (!identifier) {
			result.identifier_name = "$invalid";
			undeclared_identifier_error(node->get_source_location(), node->get_name());
		} else if (identifier->get_type() != k_ast_node_type_named_value_declaration) {
			result.identifier_name = "$invalid";
			s_compiler_result error;
			error.result = k_compiler_result_type_mismatch;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' is not a named value";
			m_errors->push_back(error);
		} else {
#if PREDEFINED(ASSERTS_ENABLED)
			// Look up the context
			s_named_value *match = get_named_value(identifier);
#endif // PREDEFINED(ASSERTS_ENABLED)
			// Since we've verified that this is a named value, it must already have a context
			wl_assert(match);
		}
	}

	virtual bool begin_visit(const c_ast_node_module_call *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_module_call *node) {
		// If there is an error, this is the default result we will push
		s_expression_result result;
		result.type = k_data_type_value;

		std::vector<s_expression_result> argument_expression_results(node->get_argument_count());

		// Iterate over each argument in reverse, since that is the order they appear on the stack
		for (size_t reverse_arg = 0; reverse_arg < node->get_argument_count(); reverse_arg++) {
			size_t arg = node->get_argument_count() - reverse_arg - 1;
			argument_expression_results[arg] = m_expression_stack.top();
			m_expression_stack.pop();
		}

		s_scope &scope = m_scope_stack.front();

		// First make sure this is actually a module value
		const c_ast_node *identifier = get_identifier(node->get_name());
		if (!identifier) {
			undeclared_identifier_error(node->get_source_location(), node->get_name());
		} else if (identifier->get_type() != k_ast_node_type_module_declaration) {
			s_compiler_result error;
			error.result = k_compiler_result_type_mismatch;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' is not a module";
			m_errors->push_back(error);
		} else {
			const c_ast_node_module_declaration *module_declaration =
				static_cast<const c_ast_node_module_declaration *>(identifier);

			add_module_subcall(scope.module_for_scope, module_declaration);

			// Set return type
			result.type = module_declaration->get_has_return_value() ? k_data_type_value : k_data_type_void;

			if (node->get_argument_count() != module_declaration->get_argument_count()) {
				s_compiler_result error;
				error.result = k_compiler_result_incorrect_argument_count;
				error.source_location = node->get_source_location();
				error.message = "Incorrect argument count for module call '" + module_declaration->get_name() +
					"': expected " + std::to_string(module_declaration->get_argument_count()) +
					", got " + std::to_string(node->get_argument_count());
				m_errors->push_back(error);
			}

			// Examine each argument to see if its result matches
			size_t args = std::min(node->get_argument_count(), module_declaration->get_argument_count());
			for (size_t arg = 0; arg < args; arg++) {
				const c_ast_node_named_value_declaration *argument = module_declaration->get_argument(arg);
				const s_expression_result &argument_result = argument_expression_results[arg];

				// We already validated the expression, which would have provided errors for things like invalid
				// identifier, so all we need to do here is validate that the type matches
				if (argument_result.type != k_data_type_value) {
					type_mismatch_error(node->get_argument(arg)->get_source_location(),
						k_data_type_value, argument_result.type);
				}

				// If a valid identifier has been provided, update its last assigned or last used statement
				const c_ast_node *identifier = get_identifier(result.identifier_name);
				if (identifier) {
					s_named_value *named_value = get_named_value(identifier);
					wl_assert(named_value);

					if (argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_in) {
						named_value->last_statement_used = scope.current_statement;
					} else {
						wl_assert(argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out);

						if (named_value->last_statement_assigned == scope.current_statement) {
							// We should not assign the same named value from two different outputs
							s_compiler_result error;
							error.result = k_compiler_result_ambiguous_named_value_assignment;
							error.source_location = node->get_source_location();
							error.message = "Ambiguous assignment for named value '" + result.identifier_name + "'";
							m_errors->push_back(error);
						}

						named_value->last_statement_assigned = scope.current_statement;
					}

					// It is an error to use a named value as both an input and output in the same statement
					if (named_value->last_statement_assigned == named_value->last_statement_used) {
						// Make sure we're not assigning to a value we used as an output parameter; this is ambiguous
						s_compiler_result error;
						error.result = k_compiler_result_ambiguous_named_value_assignment;
						error.source_location = node->get_source_location();
						error.message = "Used and assigned named value '" + result.identifier_name +
							"' in the same statement";
						m_errors->push_back(error);
					}
				}
			}
		}

		m_expression_stack.push(result);
	}
};

s_compiler_result c_ast_validator::validate(const c_ast_node *ast, std::vector<s_compiler_result> &out_errors) {
	wl_assert(ast);

	s_compiler_result result;
	result.clear();

	c_ast_validator_visitor visitor(&out_errors);
	visitor.set_pass(c_ast_validator_visitor::k_pass_register_global_identifiers);
	ast->iterate(&visitor);

	visitor.set_pass(c_ast_validator_visitor::k_pass_validate_syntax);
	ast->iterate(&visitor);

	// Validate that an entry point was found
	if (!visitor.was_entry_point_found()) {
		s_compiler_result error;
		error.result = k_compiler_result_no_entry_point;
		error.message = "No entry point '" + std::string(k_entry_point_name) + "' found";
		out_errors.push_back(error);
	}

	if (!out_errors.empty()) {
		// Don't associate the error with a particular file, those errors are collected through out_errors
		result.result = k_compiler_result_syntax_error;
		result.message = "Syntax error(s) detected";
	}

	return result;
}