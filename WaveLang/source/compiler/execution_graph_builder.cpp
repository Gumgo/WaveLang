#include "compiler/execution_graph_builder.h"
#include "compiler/ast.h"
#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"
#include <map>
#include <deque>
#include <stack>

// Set a cap to prevent crazy things from happening if a user tries to loop billions of times
static const uint32 k_max_loop_count = 10000;

// Used to evaluate constants in a partially-built graph. Necessary for building repeat loops.
class c_execution_graph_constant_evaluator {
public:
	struct s_result {
		e_native_module_argument_type type;

		union {
			real32 real_value;
			bool bool_value;
		};
		std::string string_value;
	};

private:
	// The graph
	const c_execution_graph *m_execution_graph;

	// Maps node indices to their evaluated results. Either native module output nodes or constant nodes.
	std::map<uint32, s_result> m_results;

	// Stack of nodes to evaluate. Either native module output nodes or constant nodes.
	std::stack<uint32> m_pending_nodes;

	// The final result
	bool m_invalid_constant;
	s_result m_result;

	void try_evaluate_node(uint32 node_index) {
		// Attempt to evaluate this node
		e_execution_graph_node_type node_type = m_execution_graph->get_node_type(node_index);
		if (node_type == k_execution_graph_node_type_constant) {
			// Easy - the node is simply a constant value already
			s_result result;
			result.type = m_execution_graph->get_constant_node_data_type(node_index);
			switch (result.type) {
			case k_native_module_argument_type_real:
				result.real_value = m_execution_graph->get_constant_node_real_value(node_index);
				break;

			case k_native_module_argument_type_bool:
				result.bool_value = m_execution_graph->get_constant_node_bool_value(node_index);
				break;

			case k_native_module_argument_type_string:
				result.string_value = m_execution_graph->get_constant_node_string_value(node_index);
				break;

			default:
				wl_unreachable();
			}

			m_results.insert(std::make_pair(node_index, result));
		} else if (node_type == k_execution_graph_node_type_native_module_output) {
			// This is the output to a native module call
			uint32 native_module_node_index = m_execution_graph->get_node_incoming_edge_index(node_index, 0);
			wl_assert(m_execution_graph->get_node_type(native_module_node_index) ==
				k_execution_graph_node_type_native_module_call);

			// First check if it can even be evaluated at compile-time
			uint32 native_module_index =
				m_execution_graph->get_native_module_call_native_module_index(native_module_node_index);
			const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);

			if (!native_module.compile_time_call) {
				// This native module can't be evaluated at compile-time
				m_invalid_constant = true;
				return;
			}

			// Perform the native module call to resolve the constant value
			size_t next_input = 0;
			size_t next_output = 0;
			c_native_module_compile_time_argument_list arg_list;

			bool all_inputs_evaluated = true;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				s_native_module_compile_time_argument compile_time_argument;
#if PREDEFINED(ASSERTS_ENABLED)
				compile_time_argument.assigned = false;
				compile_time_argument.argument = argument;
#endif // PREDEFINED(ASSERTS_ENABLED)

				if (argument.qualifier == k_native_module_argument_qualifier_in ||
					argument.qualifier == k_native_module_argument_qualifier_constant) {
					uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
						native_module_node_index, next_input);
					uint32 source_node_index = m_execution_graph->get_node_incoming_edge_index(input_node_index, 0);

					auto it = m_results.find(source_node_index);
					if (it == m_results.end()) {
						// Not yet evaluated - push this node to the stack. It may already be there, but that's okay, it
						// will be skipped if it's encountered and already evaluated.
						m_pending_nodes.push(source_node_index);
						all_inputs_evaluated = false;
					} else if (all_inputs_evaluated) {
						// Don't bother adding any more arguments if we've failed to evaluate an input, but still loop
						// so that we can add unevaluated nodes to the stack
						const s_result &input_result = it->second;
						switch (input_result.type) {
						case k_native_module_argument_type_real:
							compile_time_argument.real_value = input_result.real_value;
							break;

						case k_native_module_argument_type_bool:
							compile_time_argument.bool_value = input_result.bool_value;
							break;

						case k_native_module_argument_type_string:
							compile_time_argument.string_value = input_result.string_value;
							break;

						default:
							wl_unreachable();
						}
					}

					next_input++;
				} else {
					wl_assert(argument.qualifier == k_native_module_argument_qualifier_out);
					compile_time_argument.real_value = 0.0f;
					next_output++;
				}

				arg_list.push_back(compile_time_argument);
			}

			wl_assert(next_input == native_module.in_argument_count);
			wl_assert(next_output == native_module.out_argument_count);

			// Make the compile time call to resolve the outputs
			if (all_inputs_evaluated) {
				native_module.compile_time_call(arg_list);

				next_output = 0;
				for (size_t arg = 0; arg < native_module.argument_count; arg++) {
					s_native_module_argument argument = native_module.arguments[arg];
					if (argument.qualifier == k_native_module_argument_qualifier_out) {
						const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];
						wl_assert(compile_time_argument.assigned);

						// Store the output result
						s_result evaluation_result;
						evaluation_result.type = argument.type;
						switch (argument.type) {
						case k_native_module_argument_type_real:
							evaluation_result.real_value = compile_time_argument.real_value;
							break;

						case k_native_module_argument_type_bool:
							evaluation_result.bool_value = compile_time_argument.bool_value;
							break;

						case k_native_module_argument_type_string:
							evaluation_result.string_value = compile_time_argument.string_value;
							break;

						default:
							wl_unreachable();
						}

						uint32 output_node_index = m_execution_graph->get_node_outgoing_edge_index(
							native_module_node_index, next_output);
						m_results.insert(std::make_pair(output_node_index, evaluation_result));

						next_output++;
					}
				}

				wl_assert(next_output == native_module.out_argument_count);
			}

		} else {
			// We should only ever add constants and native module output nodes
			wl_unreachable();
		}
	}

public:
	c_execution_graph_constant_evaluator() {
		m_execution_graph = nullptr;
	}

	void initialize(const c_execution_graph *execution_graph) {
		wl_assert(!m_execution_graph);
		m_execution_graph = execution_graph;
		m_invalid_constant = true;
	}

	// Each time a constant is evaluated, the evaluation progress is retained. This way, we can avoid re-evaluating the
	// same thing multiple times. This works as long as nodes are only added and never removed or changed during
	// construction. (We perform these actions at optimization time.)
	bool evaluate_constant(uint32 node_index) {
		wl_assert(m_execution_graph);
		m_invalid_constant = false;
		m_pending_nodes.push(node_index);

		while (!m_invalid_constant && !m_pending_nodes.empty()) {
			uint32 next_node_index = m_pending_nodes.top();
			wl_assert(m_execution_graph->get_node_type(next_node_index) == k_execution_graph_node_type_constant ||
				m_execution_graph->get_node_type(next_node_index) == k_execution_graph_node_type_native_module_output);

			// Try to look up the result of this node's evaluation
			auto it = m_results.find(next_node_index);
			if (it == m_results.end()) {
				try_evaluate_node(next_node_index);
			} else {
				// We have already evaluated the result
				if (next_node_index == node_index) {
					// This is our final result
					m_result = it->second;
				}

				m_pending_nodes.pop();
			}
		}

		while (!m_pending_nodes.empty()) {
			m_pending_nodes.pop();
		}

		return !m_invalid_constant;
	}

	s_result get_result() const {
		wl_assert(m_execution_graph);
		wl_assert(!m_invalid_constant);
		return m_result;
	}
};

class c_execution_graph_module_builder : public c_ast_node_const_visitor {
private:
	struct s_expression_result {
		// Type of data
		e_ast_data_type type;
		// Node index in which the returned value is stored
		uint32 node_index;
		// If non-empty, this result contains an assignable named value and can be used as an out argument
		std::string identifier_name;
	};

	// Represents an identifier associated with a name
	struct s_identifier {
		static const size_t k_invalid_argument = static_cast<size_t>(-1);

		e_ast_data_type data_type;			// Data type associated with this identifier
		uint32 current_value_node_index;	// Index of the node holding the most recent value
		size_t argument_index;				// Argument index, if this identifier is an argument
	};

	struct s_scope {
		// Maps names to identifiers for this scope
		std::map<std::string, s_identifier> identifiers;
	};

	// Vector into which we accumulate errors
	std::vector<s_compiler_result> *m_errors;

	const c_ast_node *m_ast_root;								// Root of the AST
	c_execution_graph *m_execution_graph;						// The execution graph we are building
	const c_ast_node_module_declaration *m_module_declaration;	// The module we are adding to the graph

	// The number of arguments we have visited so far
	size_t m_arguments_found;

	// List of argument nodes; follows argument order
	// Inputs should be assigned before we begin, outputs should all be assigned by the end of iteration
	std::vector<uint32> m_argument_node_indices;

	// Index of the node containing the return value, should be assigned by the end of iteration
	uint32 m_return_node_index;

	// Stack of scopes
	std::deque<s_scope> m_scope_stack;

	// When we build expressions, we return the results through this stack
	std::stack<s_expression_result> m_expression_stack;

	// The last expression result assigned from a named value assignment node. We need this because repeat loops don't
	// own their loop expressions, so they must loop back on the last evaluated result.
	s_expression_result m_last_assigned_expression_result;

	// Loop count for the next scope
	uint32 m_loop_count_for_next_scope;

	// Used to evaluate constants as we build the graph
	c_execution_graph_constant_evaluator m_constant_evaluator;

	// Adds initially unassigned identifier
	void add_identifier_to_scope(const std::string &name, e_ast_data_type data_type) {
#if PREDEFINED(ASSERTS_ENABLED)
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			wl_assert(m_scope_stack[index].identifiers.find(name) == m_scope_stack[index].identifiers.end());
		}
#endif // PREDEFINED(ASSERTS_ENABLED)

		s_identifier identifier;
		identifier.data_type = data_type;
		identifier.current_value_node_index = c_execution_graph::k_invalid_index;
		identifier.argument_index = s_identifier::k_invalid_argument;
		m_scope_stack.front().identifiers.insert(std::make_pair(name, identifier));
	}

	s_identifier *get_identifier(const std::string &name) {
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			auto match = m_scope_stack[index].identifiers.find(name);
			if (match != m_scope_stack[index].identifiers.end()) {
				s_identifier &identifier = match->second;
				return &identifier;
			}
		}

		wl_vhalt("Identifier not found");
		return nullptr;
	}

	void update_identifier_node_index(const std::string &name, uint32 node_index) {
		s_identifier *identifier = get_identifier(name);

		// Update the node
		identifier->current_value_node_index = node_index;
		if (identifier->argument_index != s_identifier::k_invalid_argument) {
			const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(
				identifier->argument_index);

			if (argument->get_qualifier() == k_ast_qualifier_out) {
				// Update the out argument node
				m_argument_node_indices[identifier->argument_index] = node_index;
			}
		}
	}

public:
	c_execution_graph_module_builder(
		std::vector<s_compiler_result> *error_accumulator,
		const c_ast_node *ast_root,
		c_execution_graph *execution_graph,
		const c_ast_node_module_declaration *module_declaration) {
		wl_assert(ast_root);
		wl_assert(execution_graph);
		wl_assert(module_declaration);

		m_errors = error_accumulator;
		m_ast_root = ast_root;
		m_execution_graph = execution_graph;
		m_module_declaration = module_declaration;
		m_arguments_found = 0;

		m_loop_count_for_next_scope = 1;

		m_constant_evaluator.initialize(execution_graph);

		m_argument_node_indices.resize(module_declaration->get_argument_count(), c_execution_graph::k_invalid_index);
		m_return_node_index = c_execution_graph::k_invalid_index;
	}

	void set_in_argument_node_index(size_t argument_index, uint32 node_index) {
		const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(argument_index);
		wl_assert(argument->get_qualifier() == k_ast_qualifier_in);

		// We shouldn't be double assigning
		wl_assert(m_argument_node_indices[argument_index] == c_execution_graph::k_invalid_index);
		m_argument_node_indices[argument_index] = node_index;
	}

	uint32 get_out_argument_node_index(size_t argument_index) const {
		const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(argument_index);
		wl_assert(argument->get_qualifier() == k_ast_qualifier_out);

		// All outputs should be assigned
		wl_assert(m_argument_node_indices[argument_index] != c_execution_graph::k_invalid_index);
		return m_argument_node_indices[argument_index];
	}

	uint32 get_return_value_node_index() const {
		wl_assert(m_module_declaration->get_return_type() != k_ast_data_type_void);
		return m_return_node_index;
	}

	static const c_ast_node_module_declaration *find_module_declaration(const c_ast_node *ast_root, const char *name,
		const std::vector<s_expression_result> &argument_types) {
		// Native modules must all be at the root
		wl_assert(ast_root->get_type() == k_ast_node_type_scope);

		const c_ast_node_scope *root_scope = static_cast<const c_ast_node_scope *>(ast_root);
		for (uint32 index = 0; index < root_scope->get_child_count(); index++) {
			const c_ast_node *child = root_scope->get_child(index);
			if (child->get_type() == k_ast_node_type_module_declaration) {
				const c_ast_node_module_declaration *module_declaration =
					static_cast<const c_ast_node_module_declaration *>(child);

				if (module_declaration->get_name() == name) {
					// Check if the argument types match to determine if this is the correct overload
					bool match = (module_declaration->get_argument_count() == argument_types.size());

					if (match) {
						for (size_t arg = 0; match && arg < module_declaration->get_argument_count(); arg++) {
							if (argument_types[arg].type != module_declaration->get_argument(arg)->get_data_type()) {
								match = false;
							}
						}
					}

					if (match) {
						return module_declaration;
					}
				}
			}
		}

		wl_vhalt("Module declaration not found");
		return nullptr;
	}

	static const c_ast_node_module_declaration *find_module_declaration_single(
		const c_ast_node *ast_root, const char *name) {
		// Native modules must all be at the root
		wl_assert(ast_root->get_type() == k_ast_node_type_scope);

		const c_ast_node_scope *root_scope = static_cast<const c_ast_node_scope *>(ast_root);
		for (uint32 index = 0; index < root_scope->get_child_count(); index++) {
			const c_ast_node *child = root_scope->get_child(index);
			if (child->get_type() == k_ast_node_type_module_declaration) {
				const c_ast_node_module_declaration *module_declaration =
					static_cast<const c_ast_node_module_declaration *>(child);

				if (module_declaration->get_name() == name) {
					return module_declaration;
				}
			}
		}

		wl_vhalt("Module declaration not found");
		return nullptr;
	}

	virtual bool begin_visit(const c_ast_node_scope *node) {
		if (m_loop_count_for_next_scope > 1) {
			// If this is a looped scope, then instead of visiting this scope directly, we manually invoke a recursive
			// visit on the same node N times, except we reset the pending loop count so we don't loop infinitely
			uint32 loop_count = m_loop_count_for_next_scope;
			m_loop_count_for_next_scope = 1;

			for (uint32 loop = 0; loop < loop_count; loop++) {
				node->iterate(this);
			}

			return false;
		} else {
			m_scope_stack.push_front(s_scope());
			return true;
		}
	}

	virtual void end_visit(const c_ast_node_scope *node) {
		m_scope_stack.pop_front();
	}

	virtual bool begin_visit(const c_ast_node_module_declaration *node) {
		wl_assert(m_module_declaration == node);

		if (node->get_is_native()) {
			uint32 native_module_index = node->get_native_module_index();
			const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);

			wl_assert(native_module.first_output_is_return == (node->get_return_type() != k_ast_data_type_void));

			// Create a native module call node
			uint32 native_module_call_node_index = m_execution_graph->add_native_module_call_node(native_module_index);

			if (native_module.first_output_is_return) {
				wl_assert(native_module.argument_count == m_argument_node_indices.size() + 1);
			} else {
				wl_assert(native_module.argument_count == m_argument_node_indices.size());
			}

			// Hook up the native module inputs and outputs to the arguments
			uint32 next_input = 0;
			uint32 next_output = 0;

			if (native_module.first_output_is_return) {
				m_return_node_index = m_execution_graph->get_node_outgoing_edge_index(native_module_call_node_index, 0);
				next_output++;
			}

			for (size_t arg = 0; arg < node->get_argument_count(); arg++) {
				e_ast_qualifier qualifier = node->get_argument(arg)->get_qualifier();

				if (qualifier == k_ast_qualifier_in) {
					// This argument feeds into the module call input node
					uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
						native_module_call_node_index, next_input);
					m_execution_graph->add_edge(m_argument_node_indices[arg], input_node_index);
					next_input++;
				} else {
					wl_assert(qualifier == k_ast_qualifier_out);
					// This argument is assigned by the module call output node
					uint32 output_node_index = m_execution_graph->get_node_outgoing_edge_index(
						native_module_call_node_index, next_output);
					m_argument_node_indices[arg] = output_node_index;
					next_output++;
				}
			}

			wl_assert(next_input == native_module.in_argument_count);
			wl_assert(next_output == native_module.out_argument_count);

			// Don't visit anything else (with a native module, there's nothing to visit anyway)
			return false;
		} else {
			return true;
		}
	}

	virtual void end_visit(const c_ast_node_module_declaration *node) {
	}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		add_identifier_to_scope(node->get_name(), node->get_data_type());
		s_identifier *identifier = get_identifier(node->get_name());

		e_ast_qualifier qualifier = node->get_qualifier();

		if (qualifier == k_ast_qualifier_in) {
			// Associate the argument node with the named value
			identifier->argument_index = m_arguments_found;
			m_arguments_found++;

			// Inputs already have a value assigned
			uint32 argument_node_index = m_argument_node_indices[identifier->argument_index];
			identifier->current_value_node_index = argument_node_index;
		} else if (qualifier == k_ast_qualifier_out) {
			// Associate the argument node with the named value
			identifier->argument_index = m_arguments_found;
			m_arguments_found++;
			// No value is initially assigned
		} else {
			wl_assert(qualifier == k_ast_qualifier_none);
			// No value is initially assigned
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_assignment *node) {
		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();
		m_last_assigned_expression_result = result;

		if (node->get_named_value().empty()) {
			// If this assignment node has no name, it means it's just a placeholder for an expression; nothing to do.
		} else {
			// We expect a non-void return-type
			wl_assert(result.node_index != c_execution_graph::k_invalid_index);

			// Update the identifier with the new node
			update_identifier_node_index(node->get_named_value(), result.node_index);
		}

		wl_assert(m_expression_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_return_statement *node) {
		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();

		// We expect a non-void return-type
		wl_assert(result.node_index != c_execution_graph::k_invalid_index);

		// We should only be assigning a return value once
		wl_assert(m_return_node_index == c_execution_graph::k_invalid_index);
		m_return_node_index = result.node_index;

		wl_assert(m_expression_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_repeat_loop *node) {
		wl_assert(m_last_assigned_expression_result.type == k_ast_data_type_real);

		// Try to evaluate the expression down to a constant
		if (!m_constant_evaluator.evaluate_constant(m_last_assigned_expression_result.node_index)) {
			s_compiler_result error;
			error.result = k_compiler_result_constant_expected;
			error.source_location = node->get_source_location();
			error.message = "Repeat loop count is not constant";
			m_errors->push_back(error);
		} else {
			// We should have caught this with a type mismatch error in the evaluator
			wl_assert(m_constant_evaluator.get_result().type == k_native_module_argument_type_real);

			real32 loop_count_real = m_constant_evaluator.get_result().real_value;
			if (std::isnan(loop_count_real) ||
				std::isinf(loop_count_real) ||
				loop_count_real < 0.0f ||
				loop_count_real != std::floor(loop_count_real)) {
				s_compiler_result error;
				error.result = k_compiler_result_invalid_loop_count;
				error.source_location = node->get_source_location();
				error.message = "Invalid repeat loop count: " + std::to_string(loop_count_real);
				m_errors->push_back(error);
			} else if (loop_count_real > static_cast<real32>(k_max_loop_count)) {
				s_compiler_result error;
				error.result = k_compiler_result_invalid_loop_count;
				error.source_location = node->get_source_location();
				error.message = "Repeat loop count " + std::to_string(loop_count_real) +
					" exceeds maximum allowed loop count " + std::to_string(k_max_loop_count);
				m_errors->push_back(error);
			} else {
				m_loop_count_for_next_scope = static_cast<uint32>(loop_count_real);

				if (m_loop_count_for_next_scope == 0) {
					// $TODO We currently don't allow looping zero times. This is because it's possible that previously
					// unassigned named values are assigned in the loop, and if we don't run it at all, those values
					// will remain uninitialized. Need to think about possible solutions, but for now just don't allow
					// that case.
					s_compiler_result error;
					error.result = k_compiler_result_invalid_loop_count;
					error.source_location = node->get_source_location();
					error.message = "Repeat loop must execute at least once";
					m_errors->push_back(error);

					m_loop_count_for_next_scope = 1;
				}
			}
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_repeat_loop *node) {
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
		// Add a constant node and return it through the expression stack
		uint32 constant_node_index;
		switch (node->get_data_type()) {
		case k_ast_data_type_real:
			constant_node_index = m_execution_graph->add_constant_node(node->get_real_value());
			break;

		case k_ast_data_type_bool:
			constant_node_index = m_execution_graph->add_constant_node(node->get_bool_value());
			break;

		case k_ast_data_type_string:
			constant_node_index = m_execution_graph->add_constant_node(node->get_string_value());
			break;

		default:
			constant_node_index = c_execution_graph::k_invalid_index;
			wl_unreachable();
		}

		s_expression_result result;
		result.type = node->get_data_type();
		result.node_index = constant_node_index;
		m_expression_stack.push(result);
	}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {
		// Return the node containing this named value's current value, as well as the named value's name
		s_identifier *identifier = get_identifier(node->get_name());
		wl_assert(identifier->current_value_node_index != c_execution_graph::k_invalid_index);

		s_expression_result result;
		result.type = identifier->data_type;
		result.node_index = identifier->current_value_node_index;
		result.identifier_name = node->get_name();
		m_expression_stack.push(result);
	}

	virtual bool begin_visit(const c_ast_node_module_call *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_module_call *node) {
		std::vector<s_expression_result> argument_expression_results(node->get_argument_count());

		// Iterate over each argument in reverse, since that is the order they appear on the stack
		for (size_t reverse_arg = 0; reverse_arg < node->get_argument_count(); reverse_arg++) {
			size_t arg = node->get_argument_count() - reverse_arg - 1;
			argument_expression_results[arg] = m_expression_stack.top();
			m_expression_stack.pop();
		}

		const c_ast_node_module_declaration *module_call_declaration =
			find_module_declaration(m_ast_root, node->get_name().c_str(), argument_expression_results);

		c_execution_graph_module_builder module_builder(
			m_errors, m_ast_root, m_execution_graph, module_call_declaration);

		// Hook up the input arguments
		for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = module_call_declaration->get_argument(arg);
			s_expression_result result = argument_expression_results[arg];

			e_ast_qualifier qualifier = argument->get_qualifier();
			if (qualifier == k_ast_qualifier_in) {
				// We expect to be able to provide a node as input
				wl_assert(result.node_index != c_execution_graph::k_invalid_index);
				module_builder.set_in_argument_node_index(arg, result.node_index);
			} else {
				wl_assert(qualifier == k_ast_qualifier_out);
				// We expect a named value to store the output in
				wl_assert(!result.identifier_name.empty());
				// We will later expect that all outputs get filled with node indices
			}
		}

		module_call_declaration->iterate(&module_builder);

		// Hook up the output arguments
		for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = module_call_declaration->get_argument(arg);
			s_expression_result result = argument_expression_results[arg];

			e_ast_qualifier qualifier = argument->get_qualifier();
			if (qualifier == k_ast_qualifier_out) {
				uint32 out_node_index = module_builder.get_out_argument_node_index(arg);
				wl_assert(out_node_index != c_execution_graph::k_invalid_index);
				update_identifier_node_index(result.identifier_name, out_node_index);
			}
		}

		// Push this module call's result onto the stack
		s_expression_result result;
		result.type = module_call_declaration->get_return_type();
		if (module_call_declaration->get_return_type() != k_ast_data_type_void) {
			result.node_index = module_builder.get_return_value_node_index();
			wl_assert(result.node_index != c_execution_graph::k_invalid_index);
		} else {
			// Void return value
			result.node_index = c_execution_graph::k_invalid_index;
		}
		m_expression_stack.push(result);
	}
};

s_compiler_result c_execution_graph_builder::build_execution_graph(
	const c_ast_node *ast, c_execution_graph *out_execution_graph, std::vector<s_compiler_result> &out_errors) {
	s_compiler_result result;
	result.clear();

	// Find the entry point to start iteration
	const c_ast_node_module_declaration *entry_point_module =
		c_execution_graph_module_builder::find_module_declaration_single(ast, k_entry_point_name);
	wl_assert(entry_point_module->get_return_type() == k_ast_data_type_void);

	c_execution_graph_module_builder builder(&out_errors, ast, out_execution_graph, entry_point_module);
	entry_point_module->iterate(&builder);

	// Add an output node for each argument (they should all be out arguments)
	for (size_t arg = 0; arg < entry_point_module->get_argument_count(); arg++) {
		wl_assert(entry_point_module->get_argument(arg)->get_qualifier() == k_ast_qualifier_out);

		uint32 output_node_index = out_execution_graph->add_output_node(cast_integer_verify<uint32>(arg));
		uint32 out_argument_node_index = builder.get_out_argument_node_index(arg);
		out_execution_graph->add_edge(out_argument_node_index, output_node_index);
	}

	// $TODO Get real data for this:
	s_execution_graph_globals globals;
	globals.max_voices = 1;
	out_execution_graph->set_globals(globals);

#if PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
	out_execution_graph->output_to_file();
#endif // PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)

	if (!out_errors.empty()) {
		// Don't associate the error with a particular file, those errors are collected through out_errors
		result.result = k_compiler_result_graph_error;
		result.message = "Graph error(s) detected";
	}

	return result;
}