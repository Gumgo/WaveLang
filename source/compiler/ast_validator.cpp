#include "compiler/ast_validator.h"

#include <algorithm>
#include <deque>
#include <map>
#include <stack>

class c_ast_validator_visitor : public c_ast_node_const_visitor {
public:
	enum class e_pass {
		k_register_global_identifiers,
		k_validate_syntax,

		k_count
	};

private:
	struct s_expression_result {
		// Type which the expression resolved to
		c_ast_data_type type;
		// If non-empty, this result contains an assignable named value and can be used as an out argument
		std::string identifier_name;
		// This is false if the result represents a named value which hasn't yet been assigned
		bool has_value;
	};

	struct s_identifier {
		// The type this identifier represents
		c_ast_data_type data_type;

		// Node associated with the identifier
		const c_ast_node *ast_node;

		// The number of overloads, if this is a module. If this number is greater than 1, then the actual identifiers
		// can be looked up by querying "<module_name>$i" where i is in the range [0,overload_count-1]. ast_node will
		// still point to the first registered overload for convenience.
		uint32 overload_count;
	};

	struct s_named_value {
		// Last statement number this named value was assigned on, or -1 if it has not yet been assigned
		int32 last_statement_assigned;

		// Last statement number this named value was used on, or -1 if it has not yet been used
		int32 last_statement_used;

		// A named value cannot be both assigned and used in the same statement
		// A named value cannot be used without first being assigned
	};

	struct s_scope {
		// The module this scope belongs to, or null
		const c_ast_node_module_declaration *module_for_scope;

		// Whether this scope is the outermost scope for a module
		bool is_module_outer_scope;

		// The statement number which return occurs on
		int32 return_statement;

		// Identifiers in this scope
		std::map<std::string, s_identifier> identifiers;

		// Named values in this scope
		std::map<const c_ast_node *, s_named_value> named_values;

		s_scope() {
			module_for_scope = nullptr;
			is_module_outer_scope = false;
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

	// The compiler context
	const s_compiler_context *m_compiler_context;

	// The current pass
	e_pass m_pass;

	// Vector into which we accumulate errors
	std::vector<s_compiler_result> *m_errors;

	// If this is set, then we're about to enter the body scope for this module
	const c_ast_node_module_declaration *m_module_for_next_scope;

	// Set to true when the voice entry point is found
	bool m_voice_entry_point_found;

	// Output count for voice module
	size_t m_voice_output_count;

	// Set to true when the FX entry point is found
	bool m_fx_entry_point_found;

	// Input count for FX processing
	size_t m_fx_input_count;

	// Stack of scopes - global scope is at the bottom
	std::deque<s_scope> m_scope_stack;
	size_t m_scope_depth;

	// The current statement - this is globally increasing because all that matters is increasing order, not the initial
	// value, and statement number must be retained in nested scopes
	uint32 m_current_statement;

	// When we build expressions, we return the results through this stack
	std::stack<s_expression_result> m_expression_stack;

	// Expression result of the last named value assignment. This exists because repeat loops aren't responsible for
	// evaluating their expression, so they need to access the result somehow
	s_expression_result m_last_assigned_expression_result;

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
	bool add_identifier_to_scope(const std::string &name, const c_ast_node *node, c_ast_data_type data_type) {
		bool found = false;
		for (size_t index = 0; !found && index < m_scope_stack.size(); index++) {
			const s_scope &scope = m_scope_stack[index];
			found = (scope.identifiers.find(name) != scope.identifiers.end());
		}

		if (!found) {
			s_identifier identifier;
			identifier.data_type = data_type;
			identifier.ast_node = node;
			identifier.overload_count = 1;
			m_scope_stack.front().identifiers.insert(std::make_pair(name, identifier));
		} else {
			s_compiler_result error;
			error.result = e_compiler_result::k_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + name + "' is already in use";
			m_errors->push_back(error);
		}

		return !found;
	}

	// Returns true if the two sets of module arguments would prevent a module with the same name from being overloaded
	bool do_module_arguments_conflict_for_overload(
		const c_ast_node_module_declaration *node_a,
		const c_ast_node_module_declaration *node_b) {
		wl_assert(node_a);
		wl_assert(node_b);

		if (node_a->get_argument_count() != node_b->get_argument_count()) {
			return false;
		}

		for (size_t arg = 0; arg < node_a->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *arg_a = node_a->get_argument(arg);
			const c_ast_node_named_value_declaration *arg_b = node_b->get_argument(arg);
			if (arg_a->get_data_type() != arg_b->get_data_type()) {
				// Only check argument type, not qualifier, because currently at the calling scope, inputs and outputs
				// look identical, and we have no way of selecting between two modules that differ only by in vs. out.
				return false;
			}
		}

		// We cannot use return type alone to overload
		return true;
	}

	// Adds a module identifier to the top scope if it does not exist in this scope or lower. Properly handles module
	// overloading by adding uniquely-named identifiers for each version following a common pattern.
	bool add_module_identifier_to_scope(const std::string &name, const c_ast_node_module_declaration *node) {
		bool added = false;
		s_identifier *match = nullptr;
		for (size_t index = 0; !match && index < m_scope_stack.size(); index++) {
			s_scope &scope = m_scope_stack[index];
			auto it = scope.identifiers.find(name);
			if (it != scope.identifiers.end()) {
				match = &it->second;
			}
		}

		if (!match) {
			s_identifier identifier;
			identifier.data_type = c_ast_data_type(e_ast_primitive_type::k_module);
			identifier.ast_node = node;
			identifier.overload_count = 1;
			m_scope_stack.front().identifiers.insert(std::make_pair(name, identifier));
			added = true;
		} else if (match->data_type != c_ast_data_type(e_ast_primitive_type::k_module)) {
			s_compiler_result error;
			error.result = e_compiler_result::k_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + name + "' is already in use";
			m_errors->push_back(error);
		} else {
			bool conflict = false;
			// Check if this is a valid, non-conflicting overload
			if (match->overload_count == 1) {
				conflict = do_module_arguments_conflict_for_overload(
					node, static_cast<const c_ast_node_module_declaration *>(match->ast_node));
			} else {
				wl_assert(match->overload_count > 1);
				// Look at all existing overloaded versions
				for (uint32 overload = 0; !conflict && overload < match->overload_count; overload++) {
					std::string overload_name = name + '$' + std::to_string(overload);
					const s_identifier *overload_identifier = get_identifier(overload_name);
					wl_assert(overload_identifier);
					wl_assert(overload_identifier->data_type == c_ast_data_type(e_ast_primitive_type::k_module));

					conflict = do_module_arguments_conflict_for_overload(
						node, static_cast<const c_ast_node_module_declaration *>(overload_identifier->ast_node));
				}
			}

			if (conflict) {
				s_compiler_result error;
				error.result = e_compiler_result::k_duplicate_identifier;
				error.source_location = node->get_source_location();
				error.message = "Overloaded module '" + name + "' conflicts with existing definition";
				m_errors->push_back(error);
			} else {
				if (match->overload_count == 1) {
					// Store the existing module in the overloaded way
					s_identifier identifier;
					identifier.data_type = c_ast_data_type(e_ast_primitive_type::k_module);
					identifier.ast_node = match->ast_node;
					identifier.overload_count = 1;
					m_scope_stack.front().identifiers.insert(std::make_pair(name + "$0", identifier));
				}

				s_identifier identifier;
				identifier.data_type = c_ast_data_type(e_ast_primitive_type::k_module);
				identifier.ast_node = node;
				identifier.overload_count = 1;
				std::string overload_name = name + '$' + std::to_string(match->overload_count);
				m_scope_stack.front().identifiers.insert(std::make_pair(overload_name, identifier));

				match->overload_count++;
				added = true;
			}
		}

		return added;
	}

	const c_ast_node_module_declaration *find_matching_module_overload(
		const s_identifier *identifier,
		const std::vector<s_expression_result> &argument_types) {
		wl_assert(identifier);
		wl_assert(identifier->data_type == c_ast_data_type(e_ast_primitive_type::k_module));
		for (uint32 overload = 0; overload < identifier->overload_count; overload++) {
			const s_identifier *overload_identifier;
			if (identifier->overload_count == 1) {
				overload_identifier = identifier;
			} else {
				std::string overload_name =
					static_cast<const c_ast_node_module_declaration *>(identifier->ast_node)->get_name() + '$' +
					std::to_string(overload);
				overload_identifier = get_identifier(overload_name);
				wl_assert(overload_identifier);
			}

			const c_ast_node_module_declaration *overload_module_node =
				static_cast<const c_ast_node_module_declaration *>(overload_identifier->ast_node);
			if (overload_module_node->get_argument_count() != argument_types.size()) {
				continue;
			}

			bool match = true;
			for (size_t arg = 0; match && arg < overload_module_node->get_argument_count(); arg++) {
				if (overload_module_node->get_argument(arg)->get_data_type() != argument_types[arg].type) {
					match = false;
				}
			}

			if (match) {
				return overload_module_node;
			}
		}

		return nullptr;
	}

	// Returns the identifier associated with the given name, or null
	const s_identifier *get_identifier(const std::string &name) const {
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			const s_scope &scope = m_scope_stack[index];
			auto match = scope.identifiers.find(name);
			if (match != scope.identifiers.end()) {
				return &match->second;
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
		if (scope.return_statement >= 0 && m_current_statement == scope.return_statement + 1) {
			s_compiler_result error;
			error.result = e_compiler_result::k_statements_after_return;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() +
				"' contains statements after the return statement";
			m_errors->push_back(error);
		}
	}

	void push_scope() {
		if (m_scope_depth == 0 && !m_scope_stack.empty()) {
			// On the second pass, the global scope already exists
			wl_assert(m_pass == e_pass::k_validate_syntax);
		} else {
			m_scope_stack.push_front(s_scope());
		}
		m_scope_depth++;
	}

	void undeclared_identifier_error(s_compiler_source_location source_location, const std::string &name) {
		s_compiler_result error;
		error.result = e_compiler_result::k_undeclared_identifier;
		error.source_location = source_location;
		error.message = "Identifier '" + name + "' has not been declared";
		m_errors->push_back(error);
	}

	void unassignable_type_error(
		s_compiler_source_location source_location,
		const std::string &name,
		c_ast_data_type type) {
		s_compiler_result error;
		error.result = e_compiler_result::k_type_mismatch;
		error.source_location = source_location;
		error.message = "Identifier '" + name + "' of type '" + type.get_string() + "' is not assignable";
		m_errors->push_back(error);
	}

	void uncallable_type_error(
		s_compiler_source_location source_location,
		const std::string &name,
		c_ast_data_type type) {
		s_compiler_result error;
		error.result = e_compiler_result::k_type_mismatch;
		error.source_location = source_location;
		error.message = "Identifier '" + name + "' of type '" + type.get_string() + "' is not callable";
		m_errors->push_back(error);
	}

	void type_mismatch_error(
		s_compiler_source_location source_location,
		c_ast_data_type expected,
		c_ast_data_type actual) {
		s_compiler_result error;
		error.result = e_compiler_result::k_type_mismatch;
		error.source_location = source_location;
		error.message = "Type mismatch: expected '" + expected.get_string() + "', got '" + actual.get_string() + "'";
		m_errors->push_back(error);
	}

	void dereference_non_array_error(s_compiler_source_location source_location, c_ast_data_type non_array_type) {
		s_compiler_result error;
		error.result = e_compiler_result::k_type_mismatch;
		error.source_location = source_location;
		error.message = "Cannot dereference non-array type '" + non_array_type.get_string() + "'";
		m_errors->push_back(error);
	}

	void unassigned_named_value_error(s_compiler_source_location source_location, const std::string &name) {
		s_compiler_result error;
		error.result = e_compiler_result::k_unassigned_named_value_used;
		error.source_location = source_location;
		error.message = "Unassigned named value '" + name + "' used";
		m_errors->push_back(error);
	}

public:
	c_ast_validator_visitor(std::vector<s_compiler_result> *error_accumulator) {
		wl_assert(error_accumulator);

		m_compiler_context = nullptr;
		m_pass = e_pass::k_count;
		m_errors = error_accumulator;
		m_module_for_next_scope = nullptr;
		m_current_statement = 0;
		m_voice_entry_point_found = false;
		m_voice_output_count = 0;
		m_fx_entry_point_found = false;
		m_fx_input_count = 0;
		m_scope_depth = 0;
	}

	void set_pass(e_pass pass) {
		wl_assert(valid_enum_index(pass));
		m_pass = pass;
	}

	void set_compiler_context(const s_compiler_context *compiler_context) {
		m_compiler_context = compiler_context;
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
					error.result = e_compiler_result::k_cyclic_module_call;
					error.source_location.clear();
					error.message = "Cyclic module call(s) detected";
					m_errors->push_back(error);
					break;
				}
			} else {
				break;
			}
		}
	}

	bool was_voice_entry_point_found() const {
		return m_voice_entry_point_found;
	}

	bool was_fx_entry_point_found() const {
		return m_fx_entry_point_found;
	}

	virtual bool begin_visit(const c_ast_node_scope *node) {
		push_scope();
		s_scope &scope = m_scope_stack.front();

		if (m_module_for_next_scope) {
			wl_assert(m_pass == e_pass::k_validate_syntax);
			scope.module_for_scope = m_module_for_next_scope;
			scope.is_module_outer_scope = true;
			m_module_for_next_scope = nullptr;
		} else {
			// Try to take the module from the previous scope if it exists
			if (m_scope_stack.size() > 1) {
				const s_scope &prev_scope = m_scope_stack[1];
				scope.module_for_scope = prev_scope.module_for_scope;
			}
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_scope *node) {
		s_scope &scope = m_scope_stack.front();

		// Process the end of modules here:
		if (scope.is_module_outer_scope) {
			wl_assert(scope.module_for_scope);

			// Make sure a return statement was found if necessary
			if (scope.module_for_scope->get_return_type() != c_ast_data_type(e_ast_primitive_type::k_void)
				&& scope.return_statement < 0) {
				s_compiler_result error;
				error.result = e_compiler_result::k_missing_return_statement;
				error.source_location = scope.module_for_scope->get_source_location();
				error.message = "Module '" + scope.module_for_scope->get_name() + "' is missing return statement";
				m_errors->push_back(error);
			}

			// Make sure all out arguments have been assigned
			for (size_t index = 0; index < scope.module_for_scope->get_argument_count(); index++) {
				const c_ast_node_named_value_declaration *argument = scope.module_for_scope->get_argument(index);

				if (argument->get_qualifier() == e_ast_qualifier::k_out) {
					// Look up the context
					s_named_value *match = get_named_value(argument);
					// Context must already exist for this argument
					wl_assert(match);

					if (match->last_statement_assigned < 0) {
						s_compiler_result error;
						error.result = e_compiler_result::k_unassigned_output;
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
		if (m_pass == e_pass::k_register_global_identifiers) {
			// Add the module's name to the list of identifiers seen so far
			wl_assert(m_scope_stack.size() == 1);
			add_module_identifier_to_scope(node->get_name(), node);

			m_module_calls.push_back(s_module_call());
			m_module_calls.back().module = node;

			bool is_voice_entry_point = (node->get_name() == k_voice_entry_point_name);
			bool is_fx_entry_point = (node->get_name() == k_fx_entry_point_name);

			if (is_voice_entry_point || is_fx_entry_point) {
				if ((is_voice_entry_point && m_voice_entry_point_found)
					|| (is_fx_entry_point && m_fx_entry_point_found)) {
					s_compiler_result error;
					error.result = e_compiler_result::k_invalid_entry_point;
					error.source_location = node->get_source_location();
					error.message = std::string(is_voice_entry_point ? "Voice" : "FX") +
						" entry point '" + node->get_name() + "' must not be overloaded";
					m_errors->push_back(error);
				} else {
					if (is_voice_entry_point) {
						m_voice_entry_point_found = true;
					} else {
						wl_assert(is_fx_entry_point);
						m_fx_entry_point_found = true;
					}

					// Should have bool return value and only out arguments
					if (node->get_return_type() != c_ast_data_type(e_ast_primitive_type::k_bool)) {
						s_compiler_result error;
						error.result = e_compiler_result::k_invalid_entry_point;
						error.source_location = node->get_source_location();
						error.message = "Entry point '" + node->get_name() + "' must return a bool";
						m_errors->push_back(error);
					}

					// $TODO $INPUT change this when we support inputs
					bool all_out = true;
					bool all_real = true;
					for (size_t arg = 0; all_out && arg < node->get_argument_count(); arg++) {
						e_ast_qualifier qualifier = node->get_argument(arg)->get_qualifier();
						m_voice_output_count += is_voice_entry_point && (qualifier == e_ast_qualifier::k_out);
						m_fx_input_count += is_fx_entry_point && (qualifier == e_ast_qualifier::k_in);

						if (qualifier != e_ast_qualifier::k_out) {
							all_out = false;
						}

						if (node->get_argument(arg)->get_data_type() != c_ast_data_type(e_ast_primitive_type::k_real)) {
							all_real = false;
						}
					}

					if (is_voice_entry_point && !all_out) {
						s_compiler_result error;
						error.result = e_compiler_result::k_invalid_entry_point;
						error.source_location = node->get_source_location();
						error.message = "Voice entry point '" + node->get_name() + "' must only take out arguments";
						m_errors->push_back(error);
					}

					if (!all_real) {
						s_compiler_result error;
						error.result = e_compiler_result::k_invalid_entry_point;
						error.source_location = node->get_source_location();
						error.message = std::string(is_voice_entry_point ? "Voice" : "FX") +
							" entry point '" + node->get_name() + "' must only take real arguments";
						m_errors->push_back(error);
					}

					// If we've found both the voice and FX module, validate that inputs and outputs match
					if (m_voice_entry_point_found && m_fx_entry_point_found) {
						if (m_voice_output_count != m_fx_input_count) {
							s_compiler_result error;
							error.result = e_compiler_result::k_invalid_entry_point;
							error.source_location.clear();
							error.message = "Voice entry point '" + std::string(k_voice_entry_point_name) +
								"' output count does not match FX entry point '" + std::string(k_fx_entry_point_name) +
								"' input count";
							m_errors->push_back(error);
						}
					}
				}
			}

			// Don't enter the scope in this pass
			return false;
		} else if (!node->get_is_native()) {
			wl_assert(m_pass == e_pass::k_validate_syntax);

			// Prepare for processing in scope visitor
			wl_assert(!m_module_for_next_scope);
			m_module_for_next_scope = node;
			return true;
		} else {
			wl_assert(m_pass == e_pass::k_validate_syntax);
			// Don't visit native modules
			return false;
		}
	}

	virtual void end_visit(const c_ast_node_module_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		s_scope &scope = m_scope_stack.front();

		detect_statements_after_return(scope, node);

		// Add the identifier to the scope
		if (add_identifier_to_scope(node->get_name(), node, node->get_data_type())) {
			s_named_value named_value;
			if (node->get_qualifier() == e_ast_qualifier::k_none || node->get_qualifier() == e_ast_qualifier::k_out) {
				// Locals and outputs start out being unassigned
				named_value.last_statement_assigned = -1;
			} else {
				// Module inputs are already assigned at the beginning of the module
				wl_assert(node->get_qualifier() == e_ast_qualifier::k_in);
				named_value.last_statement_assigned = m_current_statement;
			}

			named_value.last_statement_used = -1;
			add_named_value_to_scope(node, named_value);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_declaration *node) {
		m_current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_assignment *node) {
		s_scope &scope = m_scope_stack.front();
		detect_statements_after_return(scope, node);

		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();
		m_last_assigned_expression_result = result;

		s_expression_result array_index_result;
		if (node->get_array_index_expression()) {
			// We have an additional result on the stack for the array index expression
			array_index_result = m_expression_stack.top();
			m_expression_stack.pop();
		}

		// If a valid identifier has been provided, update its last used statement
		const s_identifier *result_identifier = get_identifier(result.identifier_name);
		if (result_identifier) {
			s_named_value *named_value = get_named_value(result_identifier->ast_node);
			wl_assert(named_value);
			named_value->last_statement_used = m_current_statement;
		}

		if (!node->get_is_valid_named_value()) {
			s_compiler_result error;
			error.result = e_compiler_result::k_invalid_lhs_named_value_assignment;
			error.source_location = node->get_source_location();
			error.message = "Invalid LHS expression for assignment";
			m_errors->push_back(error);
		} else if (node->get_named_value().empty()) {
			// If this assignment node has no name, it means it's just a placeholder for an expression; nothing to do.
		} else {
			// First make sure this is actually a named value
			const s_identifier *identifier = get_identifier(node->get_named_value());
			if (!identifier) {
				undeclared_identifier_error(node->get_source_location(), node->get_named_value());
			} else if (identifier->ast_node->get_type() != e_ast_node_type::k_named_value_declaration) {
				unassignable_type_error(node->get_source_location(), node->get_named_value(), identifier->data_type);
			} else if (node->get_array_index_expression() && !identifier->data_type.is_array()) {
				// We're trying to dereference a non-array type
				dereference_non_array_error(node->get_source_location(), identifier->data_type);
			} else {
				c_ast_data_type expected_type = node->get_array_index_expression()
					? identifier->data_type.get_element_type()
					: identifier->data_type;

				if (result.type != expected_type) {
					type_mismatch_error(node->get_source_location(), expected_type, result.type);
				} else if (!result.has_value) {
					// The result is a named value which hasn't been assigned yet
					unassigned_named_value_error(node->get_source_location(), result.identifier_name);
				} else if (node->get_array_index_expression()
					&& array_index_result.type != c_ast_data_type(e_ast_primitive_type::k_real)) {
					// The array index should be a real
					type_mismatch_error(
						node->get_array_index_expression()->get_source_location(),
						c_ast_data_type(e_ast_primitive_type::k_real),
						array_index_result.type);
				} else {
					// Look up the context
					s_named_value *match = get_named_value(identifier->ast_node);
					// Since we've verified that this is a named value, it must already have a context
					wl_assert(match);

					if (node->get_array_index_expression() && match->last_statement_assigned < 0) {
						// If this is an array, we need to have assigned to it before we can dereference it
						unassigned_named_value_error(node->get_source_location(), node->get_named_value());
					} else if (match->last_statement_assigned == m_current_statement) {
						// Make sure we're not assigning to a value we used as an output parameter; this is ambiguous
						// However, it's okay if we used the named value on this same statement, e.g. x := x + 1;
						s_compiler_result error;
						error.result = e_compiler_result::k_ambiguous_named_value_assignment;
						error.source_location = node->get_source_location();
						error.message = "Ambiguous assignment for named value '" + node->get_named_value() + "'";
						m_errors->push_back(error);
					} else {
						match->last_statement_assigned = m_current_statement;
					}
				}
			}
		}

		wl_assert(m_expression_stack.empty());

		m_current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_repeat_loop *node) {
		s_scope &scope = m_scope_stack.front();
		detect_statements_after_return(scope, node);

		// Validate that the loop expression type is correct - a named value assignment node immediately preceded this
		if (m_last_assigned_expression_result.type != c_ast_data_type(e_ast_primitive_type::k_real)) {
			type_mismatch_error(
				node->get_source_location(),
				m_last_assigned_expression_result.type,
				c_ast_data_type(e_ast_primitive_type::k_real));
		} else if (!m_last_assigned_expression_result.has_value) {
			unassigned_named_value_error(
				node->get_source_location(),
				m_last_assigned_expression_result.identifier_name);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_repeat_loop *node) {}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		s_scope &scope = m_scope_stack.front();

		// Return statements will fail at the parser stage if they are outside of a module
		wl_assert(scope.module_for_scope);

		detect_statements_after_return(scope, node);

		if (!scope.is_module_outer_scope) {
			s_compiler_result error;
			error.result = e_compiler_result::k_statements_after_return;
			error.source_location = node->get_source_location();
			error.message = "Return statement in module '" + scope.module_for_scope->get_name() +
				"' must be in outermost scope";
			m_errors->push_back(error);
		}

		if (scope.return_statement >= 0) {
			s_compiler_result error;
			error.result = e_compiler_result::k_duplicate_return_statement;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() + "' has multiple return statements";
			m_errors->push_back(error);
		}

		if (scope.module_for_scope->get_return_type() == c_ast_data_type(e_ast_primitive_type::k_void)) {
			s_compiler_result error;
			error.result = e_compiler_result::k_extraneous_return_statement;
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
		const s_identifier *identifier = get_identifier(result.identifier_name);
		if (identifier) {
			s_named_value *named_value = get_named_value(identifier->ast_node);
			wl_assert(named_value);
			named_value->last_statement_used = m_current_statement;
		}

		wl_assert(scope.module_for_scope);
		if (result.type != scope.module_for_scope->get_return_type()) {
			type_mismatch_error(node->get_source_location(), scope.module_for_scope->get_return_type(), result.type);
		}

		if (scope.return_statement < 0) {
			scope.return_statement = m_current_statement;
		}

		wl_assert(m_expression_stack.empty());

		m_current_statement++;
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
		result.type = node->get_data_type();
		result.has_value = true;

		if (result.type.is_array()) {
			c_ast_data_type element_type = result.type.get_element_type();

			// If this is an array, verify that all values are of the correct type
			std::vector<s_expression_result> value_expression_results(node->get_array_count());

			// Iterate over each argument in reverse, since that is the order they appear on the stack
			// We don't really need to do this since we're just checking type, but it will make any errors appear in the
			// correct order
			for (size_t reverse_index = 0; reverse_index < node->get_array_count(); reverse_index++) {
				size_t index = node->get_array_count() - reverse_index - 1;
				value_expression_results[index] = m_expression_stack.top();
				m_expression_stack.pop();
			}

			for (size_t index = 0; index < value_expression_results.size(); index++) {
				c_ast_data_type result_type = value_expression_results[index].type;
				if (result_type != element_type) {
					type_mismatch_error(node->get_source_location(), element_type, result_type);
				}
			}
		}

		m_expression_stack.push(result);
	}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {
		s_expression_result result;
		result.type = c_ast_data_type(e_ast_primitive_type::k_void);
		result.identifier_name = node->get_name();
		result.has_value = true;

		s_scope &scope = m_scope_stack.front();

		// First make sure this is actually a named value
		const s_identifier *identifier = get_identifier(node->get_name());
		if (!identifier) {
			result.identifier_name = "$invalid";
			undeclared_identifier_error(node->get_source_location(), node->get_name());
		} else if (identifier->ast_node->get_type() != e_ast_node_type::k_named_value_declaration) {
			unassignable_type_error(node->get_source_location(), node->get_name(), identifier->data_type);
		} else {
			// Look up the context
			s_named_value *match = get_named_value(identifier->ast_node);
			// Since we've verified that this is a named value, it must already have a context
			wl_assert(match);
			result.type = identifier->data_type;

			// If this identifier hasn't yet been assigned, clear the has_value field
			result.has_value = (match->last_statement_assigned >= 0);
		}

		m_expression_stack.push(result);
	}

	virtual bool begin_visit(const c_ast_node_module_call *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_module_call *node) {
		// If there is an error, this is the default result we will push
		s_expression_result result;
		result.type = c_ast_data_type(e_ast_primitive_type::k_void);

		// The result always has a value (even if it's void)
		result.has_value = true;

		std::vector<s_expression_result> argument_expression_results(node->get_argument_count());

		// Iterate over each argument in reverse, since that is the order they appear on the stack
		for (size_t reverse_arg = 0; reverse_arg < node->get_argument_count(); reverse_arg++) {
			size_t arg = node->get_argument_count() - reverse_arg - 1;
			argument_expression_results[arg] = m_expression_stack.top();
			m_expression_stack.pop();
		}

		s_scope &scope = m_scope_stack.front();

		// First make sure this is actually a module value
		const s_identifier *identifier = get_identifier(node->get_name());
		if (!identifier) {
			undeclared_identifier_error(node->get_source_location(), node->get_name());
		} else if (identifier->ast_node->get_type() != e_ast_node_type::k_module_declaration) {
			uncallable_type_error(node->get_source_location(), node->get_name(), identifier->data_type);
		} else {
			const c_ast_node_module_declaration *module_declaration =
				static_cast<const c_ast_node_module_declaration *>(identifier->ast_node);

			const c_ast_node_module_declaration *matching_overload_module_declaration =
				find_matching_module_overload(identifier, argument_expression_results);

			if (!matching_overload_module_declaration && identifier->overload_count > 1) {
				// For overloaded modules, spit out a single catch-all error if we don't find a match
				s_compiler_result error;
				error.result = e_compiler_result::k_missing_import;
				error.source_location = node->get_source_location();
				error.message = "No overloads for module '" + node->get_name() + "' match the arguments provided";
				m_errors->push_back(error);
			} else {
				// Either we (1) found a valid overload, or (2) the module isn't overloaded. In case (1),
				// use matching_overload_module_declaration. In case (2), matching_overload_module_declaration may be
				// null if the argument types don't match for the single instance that exists of this module, so don't
				// overwrite module_declaration with matching_overload_module_declaration in this case - we will produce
				// type errors in a moment for module_declaration.
				if (matching_overload_module_declaration) {
					module_declaration = matching_overload_module_declaration;
				}

				// Make sure this module is visible from the file it's being called from. This is kind of a hack/
				// incomplete solution, but it prevents situations where A imports B, and then B can use modules from A.
				bool module_is_imported = true;
				if (!module_declaration->get_is_native()) {
					size_t calling_source_file_index = scope.module_for_scope->get_source_location().source_file_index;
					size_t callee_source_file_index = module_declaration->get_source_location().source_file_index;

					module_is_imported =
						m_compiler_context->source_files[calling_source_file_index].imports[callee_source_file_index];
				}

				if (!module_is_imported) {
					s_compiler_result error;
					error.result = e_compiler_result::k_missing_import;
					error.source_location = node->get_source_location();
					error.message = "Identifier '" + node->get_name() + "' has not been imported";
					m_errors->push_back(error);
				} else {
					add_module_subcall(scope.module_for_scope, module_declaration);

					// Set return type
					result.type = module_declaration->get_return_type();

					if (identifier->overload_count > 1) {
						wl_assert(node->get_argument_count() == module_declaration->get_argument_count());
					} else {
						if (node->get_argument_count() != module_declaration->get_argument_count()) {
							s_compiler_result error;
							error.result = e_compiler_result::k_incorrect_argument_count;
							error.source_location = node->get_source_location();
							error.message =
								"Incorrect argument count for module call '" + module_declaration->get_name() +
								"': expected " + std::to_string(module_declaration->get_argument_count()) +
								", got " + std::to_string(node->get_argument_count());
							m_errors->push_back(error);
						}
					}

					// Examine each argument to see if its result matches
					size_t args = std::min(node->get_argument_count(), module_declaration->get_argument_count());
					for (size_t arg = 0; arg < args; arg++) {
						const c_ast_node_named_value_declaration *argument = module_declaration->get_argument(arg);
						const s_expression_result &argument_result = argument_expression_results[arg];

						if (identifier->overload_count > 1) {
							wl_assert(argument_result.type == argument->get_data_type());
						} else {
							// We already validated the expression, which would have provided errors for things like
							// invalid identifier, so all we need to do here is validate that the type matches
							if (argument_result.type != argument->get_data_type()) {
								type_mismatch_error(
									node->get_argument(arg)->get_source_location(),
									argument->get_data_type(),
									argument_result.type);
							}
						}

						// If this argument has an out qualifier, it must take a direct named value
						// We currently don't support outputting to an array element directly
						if (argument->get_qualifier() == e_ast_qualifier::k_out
							&& argument_result.identifier_name.empty()) {
							s_compiler_result error;
							error.result = e_compiler_result::k_named_value_expected;
							error.source_location = node->get_argument(arg)->get_source_location();
							error.message = "Expected named value for out-qualified argument";
							m_errors->push_back(error);
						}

						if (argument->get_qualifier() == e_ast_qualifier::k_in
							&& !argument_result.has_value) {
							unassigned_named_value_error(
								node->get_argument(arg)->get_source_location(),
								argument_result.identifier_name);
						}

						// If a valid identifier has been provided, update its last assigned or last used statement
						const s_identifier *identifier = get_identifier(argument_result.identifier_name);
						if (identifier) {
							s_named_value *named_value = get_named_value(identifier->ast_node);
							wl_assert(named_value);

							if (argument->get_qualifier() == e_ast_qualifier::k_in) {
								named_value->last_statement_used = m_current_statement;
							} else {
								wl_assert(argument->get_qualifier() == e_ast_qualifier::k_out);

								if (named_value->last_statement_assigned == m_current_statement) {
									// We should not assign the same named value from two different outputs
									s_compiler_result error;
									error.result = e_compiler_result::k_ambiguous_named_value_assignment;
									error.source_location = node->get_source_location();
									error.message = "Ambiguous assignment for named value '" +
										result.identifier_name + "'";
									m_errors->push_back(error);
								}

								named_value->last_statement_assigned = m_current_statement;
							}

							// It is an error to use a named value as both an input and output in the same statement
							if (named_value->last_statement_assigned == named_value->last_statement_used) {
								// Make sure we're not assigning to a value we used as an output parameter; this is
								// ambiguous
								s_compiler_result error;
								error.result = e_compiler_result::k_ambiguous_named_value_assignment;
								error.source_location = node->get_source_location();
								error.message = "Used and assigned named value '" + result.identifier_name +
									"' in the same statement";
								m_errors->push_back(error);
							}
						}
					}
				}
			}
		}

		m_expression_stack.push(result);
	}
};

s_compiler_result c_ast_validator::validate(
	const s_compiler_context *compiler_context,
	const c_ast_node *ast,
	std::vector<s_compiler_result> &errors_out) {
	wl_assert(ast);

	s_compiler_result result;
	result.clear();

	c_ast_validator_visitor visitor(&errors_out);
	visitor.set_compiler_context(compiler_context);
	visitor.set_pass(c_ast_validator_visitor::e_pass::k_register_global_identifiers);
	ast->iterate(&visitor);

	visitor.set_pass(c_ast_validator_visitor::e_pass::k_validate_syntax);
	ast->iterate(&visitor);

	// Validate that an entry point was found
	if (!visitor.was_voice_entry_point_found()
		&& !visitor.was_fx_entry_point_found()) {
		s_compiler_result error;
		error.result = e_compiler_result::k_no_entry_point;
		error.source_location.clear();
		error.message = "No voice entry point '" + std::string(k_voice_entry_point_name) +
			"' or FX entry point '" + std::string(k_fx_entry_point_name) + "' found";
		errors_out.push_back(error);
	}

	if (!errors_out.empty()) {
		// Don't associate the error with a particular file, those errors are collected through errors_out
		result.result = e_compiler_result::k_syntax_error;
		result.message = "Syntax error(s) detected";
	}

	return result;
}