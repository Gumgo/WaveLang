#include "compiler/ast.h"
#include "compiler/execution_graph_builder.h"
#include "compiler/execution_graph_optimizer.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/instrument.h"
#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"

#include <deque>
#include <map>
#include <stack>

// Set a cap to prevent crazy things from happening if a user tries to loop billions of times
static constexpr uint32 k_max_loop_count = 10000;

static constexpr e_native_module_primitive_type k_ast_primitive_type_to_native_module_primitive_type_mapping[] = {
	e_native_module_primitive_type::k_count,	// e_ast_primitive_type::k_void (invalid)
	e_native_module_primitive_type::k_count,	// e_ast_primitive_type::k_module (invalid)
	e_native_module_primitive_type::k_real,	// e_ast_primitive_type::k_real
	e_native_module_primitive_type::k_bool,	// e_ast_primitive_type::k_bool
	e_native_module_primitive_type::k_string	// e_ast_primitive_type::k_string
};
static_assert(
	array_count(k_ast_primitive_type_to_native_module_primitive_type_mapping) == enum_count<e_ast_primitive_type>(),
	"Ast primitive type to native module primitive type mismatch");

static e_native_module_primitive_type convert_ast_primitive_type_to_native_module_primitive_type(
	e_ast_primitive_type ast_primitive_type) {
	wl_assert(valid_enum_index(ast_primitive_type));
	e_native_module_primitive_type result =
		k_ast_primitive_type_to_native_module_primitive_type_mapping[enum_index(ast_primitive_type)];
	wl_assert(result != e_native_module_primitive_type::k_count);
	return result;
}

class c_execution_graph_module_builder : public c_ast_node_const_visitor {
private:
	struct s_expression_result {
		// Type of data
		c_ast_data_type type;
		// Node reference in which the returned value is stored
		c_node_reference node_reference;
		// If non-empty, this result contains an assignable named value and can be used as an out argument
		std::string identifier_name;
	};

	// Represents an identifier associated with a name
	struct s_identifier {
		static constexpr size_t k_invalid_argument = static_cast<size_t>(-1);

		c_ast_data_type data_type;						// Data type associated with this identifier
		c_node_reference current_value_node_reference;	// Reference to the node holding the most recent value
		size_t argument_index;							// Argument index, if this identifier is an argument
	};

	struct s_scope {
		// Maps names to identifiers for this scope
		std::map<std::string, s_identifier> identifiers;
	};

	// Contexts for native module libraries
	c_wrapped_array<void *> m_native_module_library_contexts;

	// Vector into which we accumulate errors
	std::vector<s_compiler_result> *m_errors;

	const c_ast_node *m_ast_root;								// Root of the AST
	c_execution_graph *m_execution_graph;						// The execution graph we are building
	const s_instrument_globals *m_instrument_globals;			// The globals for this graph
	const c_ast_node_module_declaration *m_module_declaration;	// The module we are adding to the graph

	// The number of arguments we have visited so far
	size_t m_arguments_found;

	// List of argument nodes; follows argument order
	// Inputs should be assigned before we begin, outputs should all be assigned by the end of iteration
	std::vector<c_node_reference> m_argument_node_references;

	// Reference to the node containing the return value, should be assigned by the end of iteration
	c_node_reference m_return_node_reference;

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

	// Used to remove unused nodes as we build the graph
	c_execution_graph_trimmer m_trimmer;

	// Adds initially unassigned identifier
	void add_identifier_to_scope(const std::string &name, c_ast_data_type data_type) {
#if IS_TRUE(ASSERTS_ENABLED)
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			wl_assert(m_scope_stack[index].identifiers.find(name) == m_scope_stack[index].identifiers.end());
		}
#endif // IS_TRUE(ASSERTS_ENABLED)

		s_identifier identifier;
		identifier.data_type = data_type;
		identifier.current_value_node_reference = c_node_reference();
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

	void update_identifier_node_reference(const std::string &name, c_node_reference node_reference) {
		s_identifier *identifier = get_identifier(name);

		// Update the node
		c_node_reference old_value_node_reference = identifier->current_value_node_reference;
		identifier->current_value_node_reference = node_reference;
		if (identifier->argument_index != s_identifier::k_invalid_argument) {
			const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(
				identifier->argument_index);

			if (argument->get_qualifier() == e_ast_qualifier::k_out) {
				// Update the out argument node
				// Add an extra reference to assigned outputs so they don't go out of scope and get deleted too soon
				swap_temporary_reference(m_argument_node_references[identifier->argument_index], node_reference);
				m_argument_node_references[identifier->argument_index] = node_reference;
			}
		}

		swap_temporary_reference(old_value_node_reference, node_reference);
	}

	void update_array_identifier_single_value_node_reference(
		const std::string &name,
		size_t array_index,
		c_node_reference node_reference) {
		s_identifier *identifier = get_identifier(name);

		// Create a new array with all values identical except for the one specified
		wl_assert(m_execution_graph->get_node_type(identifier->current_value_node_reference) ==
			e_execution_graph_node_type::k_constant);
		c_native_module_data_type array_type =
			m_execution_graph->get_constant_node_data_type(identifier->current_value_node_reference);
		wl_assert(array_type.is_array());

		if (m_execution_graph->get_node_outgoing_edge_count(identifier->current_value_node_reference) == 1) {
			// Simply update this array in-place, since nothing else is referencing it
			c_node_reference old_value_node_reference = m_execution_graph->set_constant_array_value_at_index(
				identifier->current_value_node_reference,
				cast_integer_verify<uint32>(array_index),
				node_reference);

			// See if we can remove the old value node
			m_trimmer.try_trim_node(old_value_node_reference);
		} else {
			c_node_reference new_array_node_reference = m_execution_graph->add_constant_array_node(
				array_type.get_element_type());

			// Add the identical array values for all indices except the specified one
			size_t array_count =
				m_execution_graph->get_node_incoming_edge_count(identifier->current_value_node_reference);
			for (size_t index = 0; index < array_count; index++) {
				c_node_reference source_node_reference;
				if (index == array_index) {
					source_node_reference = node_reference;
				} else {
					source_node_reference = m_execution_graph->get_node_incoming_edge_reference(
						m_execution_graph->get_node_incoming_edge_reference(
							identifier->current_value_node_reference, index), 0);
				}

				m_execution_graph->add_constant_array_value(new_array_node_reference, source_node_reference);
			}

			c_node_reference old_value_node_reference = identifier->current_value_node_reference;
			identifier->current_value_node_reference = new_array_node_reference;
			swap_temporary_reference(old_value_node_reference, new_array_node_reference);
		}
	}

	void add_temporary_reference(c_node_reference node_reference) {
		if (node_reference.is_valid()) {
			c_node_reference temporary_node_reference = m_execution_graph->add_temporary_reference_node();
			m_execution_graph->add_edge(node_reference, temporary_node_reference);
		}
	}

	void remove_temporary_reference(c_node_reference node_reference) {
		if (node_reference.is_valid()) {
			IF_ASSERTS_ENABLED(bool found = false;)
			size_t edge_count = m_execution_graph->get_node_outgoing_edge_count(node_reference);
			for (size_t edge_index = 0; edge_index < edge_count; edge_index++) {
				c_node_reference to_node_reference =
					m_execution_graph->get_node_outgoing_edge_reference(node_reference, edge_index);

				e_execution_graph_node_type to_node_type = m_execution_graph->get_node_type(to_node_reference);
				if (to_node_type == e_execution_graph_node_type::k_temporary_reference) {
					m_execution_graph->remove_node(to_node_reference);
					m_trimmer.try_trim_node(node_reference);
					IF_ASSERTS_ENABLED(found = true;)
					break;
				}
			}

			wl_assert(found);
		}
	}

	void swap_temporary_reference(c_node_reference old_node_reference, c_node_reference new_node_reference) {
		add_temporary_reference(new_node_reference);
		remove_temporary_reference(old_node_reference);
	}

	static void on_node_removed(void *context, c_node_reference node_reference) {
		c_execution_graph_module_builder *this_ptr = static_cast<c_execution_graph_module_builder *>(context);
		this_ptr->m_constant_evaluator.remove_cached_result(node_reference);
	}

public:
	c_execution_graph_module_builder(
		c_wrapped_array<void *> native_module_library_contexts,
		std::vector<s_compiler_result> *error_accumulator,
		const c_ast_node *ast_root,
		c_execution_graph *execution_graph,
		const s_instrument_globals *instrument_globals,
		const c_ast_node_module_declaration *module_declaration) {
		wl_assert(ast_root);
		wl_assert(execution_graph);
		wl_assert(module_declaration);

		m_native_module_library_contexts = native_module_library_contexts;
		m_errors = error_accumulator;
		m_ast_root = ast_root;
		m_execution_graph = execution_graph;
		m_instrument_globals = instrument_globals;
		m_module_declaration = module_declaration;
		m_arguments_found = 0;

		m_loop_count_for_next_scope = 1;

		m_constant_evaluator.initialize(native_module_library_contexts, execution_graph, instrument_globals, error_accumulator);
		m_trimmer.initialize(execution_graph, on_node_removed, this);

		m_argument_node_references.resize(module_declaration->get_argument_count());
		m_return_node_reference = c_node_reference();
	}

	~c_execution_graph_module_builder() {
		wl_assert(m_expression_stack.empty());
		remove_temporary_reference(m_last_assigned_expression_result.node_reference);
		remove_temporary_reference(m_return_node_reference);

		// Remove extra references we made to output arguments
		for (size_t arg = 0; arg < m_module_declaration->get_argument_count(); arg++) {
			if (m_module_declaration->get_argument(arg)->get_qualifier() == e_ast_qualifier::k_out) {
				// Remove the extra reference we added
				remove_temporary_reference(m_argument_node_references[arg]);
			}
		}
	}

	void set_in_argument_node_reference(size_t argument_index, c_node_reference node_reference) {
		const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(argument_index);
		wl_assert(argument->get_qualifier() == e_ast_qualifier::k_in);

		// We shouldn't be double assigning
		wl_assert(!m_argument_node_references[argument_index].is_valid());
		m_argument_node_references[argument_index] = node_reference;
	}

	c_node_reference get_out_argument_node_reference(size_t argument_index) const {
		const c_ast_node_named_value_declaration *argument = m_module_declaration->get_argument(argument_index);
		wl_assert(argument->get_qualifier() == e_ast_qualifier::k_out);

		// All outputs should be assigned
		wl_assert(m_argument_node_references[argument_index].is_valid());
		return m_argument_node_references[argument_index];
	}

	c_node_reference get_return_value_node_reference() const {
		wl_assert(m_module_declaration->get_return_type() != c_ast_data_type(e_ast_primitive_type::k_void));
		return m_return_node_reference;
	}

	static const c_ast_node_module_declaration *find_module_declaration(
		const c_ast_node *ast_root,
		const char *name,
		const std::vector<s_expression_result> &argument_types) {
		// Native modules must all be at the root
		wl_assert(ast_root->get_type() == e_ast_node_type::k_scope);

		const c_ast_node_scope *root_scope = static_cast<const c_ast_node_scope *>(ast_root);
		for (uint32 index = 0; index < root_scope->get_child_count(); index++) {
			const c_ast_node *child = root_scope->get_child(index);
			if (child->get_type() == e_ast_node_type::k_module_declaration) {
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

		return nullptr;
	}

	static const c_ast_node_module_declaration *find_module_declaration_single(
		const c_ast_node *ast_root, const char *name) {
		// Native modules must all be at the root
		wl_assert(ast_root->get_type() == e_ast_node_type::k_scope);

		const c_ast_node_scope *root_scope = static_cast<const c_ast_node_scope *>(ast_root);
		for (uint32 index = 0; index < root_scope->get_child_count(); index++) {
			const c_ast_node *child = root_scope->get_child(index);
			if (child->get_type() == e_ast_node_type::k_module_declaration) {
				const c_ast_node_module_declaration *module_declaration =
					static_cast<const c_ast_node_module_declaration *>(child);

				if (module_declaration->get_name() == name) {
					return module_declaration;
				}
			}
		}

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
		// Decrement refcount of all nodes in this scope
		s_scope &scope = m_scope_stack.front();
		for (auto identifier = scope.identifiers.begin(); identifier != scope.identifiers.end(); identifier++) {
			remove_temporary_reference(identifier->second.current_value_node_reference);
		}

		m_scope_stack.pop_front();
	}

	virtual bool begin_visit(const c_ast_node_module_declaration *node) {
		wl_assert(m_module_declaration == node);

		if (node->get_is_native()) {
			h_native_module native_module_handle = node->get_native_module_handle();
			const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);

			wl_assert((native_module.return_argument_index == k_invalid_native_module_argument_index)
				|| (node->get_return_type() != c_ast_data_type(e_ast_primitive_type::k_void)));

			// Create a native module call node
			c_node_reference native_module_call_node_reference =
				m_execution_graph->add_native_module_call_node(native_module_handle);

			if (native_module.return_argument_index != k_invalid_native_module_argument_index) {
				wl_assert(native_module.argument_count == m_argument_node_references.size() + 1);
			} else {
				wl_assert(native_module.argument_count == m_argument_node_references.size());
			}

			// Hook up the native module inputs and outputs to the arguments
			uint32 next_input = 0;
			uint32 next_output = 0;

			if (native_module.return_argument_index != k_invalid_native_module_argument_index) {
				size_t output_index = get_native_module_output_index_for_argument_index(
					native_module, native_module.return_argument_index);

				m_return_node_reference = m_execution_graph->get_node_outgoing_edge_reference(
					native_module_call_node_reference, output_index);
				add_temporary_reference(m_return_node_reference);

				next_output++;
			}

			for (size_t arg = 0; arg < node->get_argument_count(); arg++) {
				e_ast_qualifier qualifier = node->get_argument(arg)->get_qualifier();

				if (qualifier == e_ast_qualifier::k_in) {
					// This argument feeds into the module call input node
					c_node_reference input_node_reference = m_execution_graph->get_node_incoming_edge_reference(
						native_module_call_node_reference, next_input);
					m_execution_graph->add_edge(m_argument_node_references[arg], input_node_reference);

					next_input++;
				} else {
					wl_assert(qualifier == e_ast_qualifier::k_out);
					// This argument is assigned by the module call output node
					c_node_reference output_node_reference = m_execution_graph->get_node_outgoing_edge_reference(
						native_module_call_node_reference, next_output);
					m_argument_node_references[arg] = output_node_reference;
					add_temporary_reference(output_node_reference);

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

	virtual void end_visit(const c_ast_node_module_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		add_identifier_to_scope(node->get_name(), node->get_data_type());
		s_identifier *identifier = get_identifier(node->get_name());

		e_ast_qualifier qualifier = node->get_qualifier();

		if (qualifier == e_ast_qualifier::k_in) {
			// Associate the argument node with the named value
			identifier->argument_index = m_arguments_found;
			m_arguments_found++;

			// Inputs already have a value assigned
			update_identifier_node_reference(node->get_name(), m_argument_node_references[identifier->argument_index]);
		} else if (qualifier == e_ast_qualifier::k_out) {
			// Associate the argument node with the named value
			identifier->argument_index = m_arguments_found;
			m_arguments_found++;
			// No value is initially assigned
		} else {
			wl_assert(qualifier == e_ast_qualifier::k_none);
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

		s_expression_result array_index_result;
		if (node->get_array_index_expression()) {
			// We have an additional result on the stack for the array index expression
			array_index_result = m_expression_stack.top();
			m_expression_stack.pop();
		}

		wl_assert(node->get_is_valid_named_value());

		// Optimization: see if we can immediately evaluate as a constant
		if (result.node_reference.is_valid()
			&& m_execution_graph->get_node_type(result.node_reference) != e_execution_graph_node_type::k_constant
			&& m_constant_evaluator.evaluate_constant(result.node_reference)) {
			c_execution_graph_constant_evaluator::s_result constant_result =
				m_constant_evaluator.get_result();

			c_node_reference constant_node_reference;
			if (constant_result.type.is_array()) {
				constant_node_reference =
					m_execution_graph->add_constant_array_node(constant_result.type.get_element_type());
				for (size_t index = 0; index < constant_result.array_value.get_array().size(); index++) {
					m_execution_graph->add_constant_array_value(
						constant_node_reference, constant_result.array_value.get_array()[index]);
				}
			} else {
				switch (constant_result.type.get_primitive_type()) {
				case e_native_module_primitive_type::k_real:
					constant_node_reference = m_execution_graph->add_constant_node(constant_result.real_value);
					break;

				case e_native_module_primitive_type::k_bool:
					constant_node_reference = m_execution_graph->add_constant_node(constant_result.bool_value);
					break;

				case e_native_module_primitive_type::k_string:
					constant_node_reference =
						m_execution_graph->add_constant_node(constant_result.string_value.get_string());
					break;

				default:
					wl_unreachable();
				}
			}

			wl_assert(constant_node_reference.is_valid());

			swap_temporary_reference(result.node_reference, constant_node_reference);
			result.node_reference = constant_node_reference;
		}

		swap_temporary_reference(m_last_assigned_expression_result.node_reference, result.node_reference);
		m_last_assigned_expression_result = result;

		if (node->get_named_value().empty()) {
			// If this assignment node has no name, it means it's just a placeholder for an expression; nothing to do.
		} else {
			// We expect a non-void return-type
			wl_assert(result.node_reference.is_valid());

			if (node->get_array_index_expression()) {
				// Updating an array value requires several steps, bail if we fail any of them
				bool array_failed = false;

				// First make sure the index is a constant
				if (!m_constant_evaluator.evaluate_constant(array_index_result.node_reference)) {
					array_failed = true;
					s_compiler_result error;
					error.result = e_compiler_result::k_constant_expected;
					error.source_location = node->get_array_index_expression()->get_source_location();
					error.message = "Array index is not constant";
					m_errors->push_back(error);
				}

				// Ensure that the index is a valid integer
				int32 array_index = 0;
				if (!array_failed) {
					// We should have caught this with a type mismatch error in the validator
					wl_assert(m_constant_evaluator.get_result().type ==
						c_native_module_data_type(e_native_module_primitive_type::k_real));

					real32 array_index_real = m_constant_evaluator.get_result().real_value;
					if (std::isnan(array_index_real)
						|| std::isinf(array_index_real)
						|| array_index_real != std::floor(array_index_real)) {
						array_failed = true;
						s_compiler_result error;
						error.result = e_compiler_result::k_invalid_array_index;
						error.source_location = node->get_array_index_expression()->get_source_location();
						error.message = "Array index '" + std::to_string(array_index_real) + "' is not an integer";
						m_errors->push_back(error);
					} else {
						array_index = static_cast<int32>(array_index_real);
					}
				}

				// Make sure the array itself has been evaluated to a constant - note that this does not mean that the
				// array element values must be constant
				s_identifier *identifier = nullptr;
				if (!array_failed) {
					identifier = get_identifier(node->get_named_value());
					wl_assert(identifier);

					e_execution_graph_node_type array_value_node_type =
						m_execution_graph->get_node_type(identifier->current_value_node_reference);

					if (array_value_node_type != e_execution_graph_node_type::k_constant) {
						if (!m_constant_evaluator.evaluate_constant(identifier->current_value_node_reference)) {
							array_failed = true;
							// I don't believe this case is actually possible because the language syntax should not
							// allow construction of non-constant arrays
							s_compiler_result error;
							error.result = e_compiler_result::k_constant_expected;
							error.source_location = node->get_expression()->get_source_location();
							error.message = "Array is not constant";
							m_errors->push_back(error);
						} else {
							wl_assert(m_constant_evaluator.get_result().type.is_array());
							c_execution_graph_constant_evaluator::s_result constant_evaluator_result =
								m_constant_evaluator.get_result();
							wl_assert(constant_evaluator_result.type.is_array());

							c_node_reference new_array_node_reference = m_execution_graph->add_constant_array_node(
								constant_evaluator_result.type.get_element_type());

							size_t array_count = constant_evaluator_result.array_value.get_array().size();
							for (size_t index = 0; index < array_count; index++) {
								m_execution_graph->add_constant_array_value(
									new_array_node_reference,
									constant_evaluator_result.array_value.get_array()[index]);
							}

							update_identifier_node_reference(node->get_named_value(), new_array_node_reference);
						}
					}
				}

				if (!array_failed) {
					size_t array_count = m_execution_graph->get_node_incoming_edge_count(
						identifier->current_value_node_reference);
					if (array_index < 0 || cast_integer_verify<size_t>(array_index) >= array_count) {
						s_compiler_result error;
						error.result = e_compiler_result::k_invalid_array_index;
						error.source_location = node->get_array_index_expression()->get_source_location();
						error.message = "Array index '" + std::to_string(array_index) +
							"' out of range for array of size " + std::to_string(array_count);
						m_errors->push_back(error);
					} else {
						// Success - valid array index
						update_array_identifier_single_value_node_reference(
							node->get_named_value(),
							cast_integer_verify<size_t>(array_index),
							result.node_reference);
					}
				}

				// If we encountered an error, it's okay that we didn't update the array's value because arrays must
				// always be either completely initialized with valid values, or entirely uninitialized. So we will
				// have a valid array.
			} else {
				// Update the identifier with the new node
				update_identifier_node_reference(node->get_named_value(), result.node_reference);
			}
		}

		remove_temporary_reference(result.node_reference);
		remove_temporary_reference(array_index_result.node_reference);

		wl_assert(m_expression_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_return_statement *node) {
		s_expression_result result = m_expression_stack.top();
		m_expression_stack.pop();

		// We expect a non-void return-type
		wl_assert(result.node_reference.is_valid());

		// We should only be assigning a return value once
		wl_assert(!m_return_node_reference.is_valid());
		m_return_node_reference = result.node_reference;

		// Don't decrement node refcount since we simply swapped it into the return value node reference

		wl_assert(m_expression_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_repeat_loop *node) {
		wl_assert(m_last_assigned_expression_result.type == c_ast_data_type(e_ast_primitive_type::k_real));

		// Try to evaluate the expression down to a constant
		if (!m_constant_evaluator.evaluate_constant(m_last_assigned_expression_result.node_reference)) {
			s_compiler_result error;
			error.result = e_compiler_result::k_constant_expected;
			error.source_location = node->get_source_location();
			error.message = "Repeat loop count is not constant";
			m_errors->push_back(error);
		} else {
			// We should have caught this with a type mismatch error in the validator
			wl_assert(m_constant_evaluator.get_result().type ==
				c_native_module_data_type(e_native_module_primitive_type::k_real));

			real32 loop_count_real = m_constant_evaluator.get_result().real_value;
			if (std::isnan(loop_count_real)
				|| std::isinf(loop_count_real)
				|| loop_count_real < 0.0f
				|| loop_count_real != std::floor(loop_count_real)) {
				s_compiler_result error;
				error.result = e_compiler_result::k_invalid_loop_count;
				error.source_location = node->get_source_location();
				error.message = "Invalid repeat loop count '" + std::to_string(loop_count_real) + "'";
				m_errors->push_back(error);
			} else if (loop_count_real > static_cast<real32>(k_max_loop_count)) {
				s_compiler_result error;
				error.result = e_compiler_result::k_invalid_loop_count;
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
					error.result = e_compiler_result::k_invalid_loop_count;
					error.source_location = node->get_source_location();
					error.message = "Repeat loop must execute at least once";
					m_errors->push_back(error);

					m_loop_count_for_next_scope = 1;
				}
			}
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_repeat_loop *node) {}

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
		c_node_reference constant_node_reference;
		if (node->get_data_type().is_array()) {
			e_ast_primitive_type ast_primitive_type = node->get_data_type().get_primitive_type();
			e_native_module_primitive_type native_module_primitive_type =
				convert_ast_primitive_type_to_native_module_primitive_type(ast_primitive_type);
			constant_node_reference = m_execution_graph->add_constant_array_node(
				c_native_module_data_type(native_module_primitive_type));
		} else {
			switch (node->get_data_type().get_primitive_type()) {
			case e_ast_primitive_type::k_real:
				constant_node_reference = m_execution_graph->add_constant_node(node->get_real_value());
				break;

			case e_ast_primitive_type::k_bool:
				constant_node_reference = m_execution_graph->add_constant_node(node->get_bool_value());
				break;

			case e_ast_primitive_type::k_string:
				constant_node_reference = m_execution_graph->add_constant_node(node->get_string_value());
				break;

			default:
				constant_node_reference = c_node_reference();
				wl_unreachable();
			}
		}

		if (node->get_data_type().is_array()) {
			// Add the array values from the expression results on the stack - they are in reverse order
			std::vector<s_expression_result> value_expression_results(node->get_array_count());

			// Iterate over each argument in reverse, since that is the order they appear on the stack
			for (size_t reverse_index = 0; reverse_index < node->get_array_count(); reverse_index++) {
				size_t index = node->get_array_count() - reverse_index - 1;
				value_expression_results[index] = m_expression_stack.top();
				m_expression_stack.pop();
			}

			for (size_t index = 0; index < node->get_array_count(); index++) {
				m_execution_graph->add_constant_array_value(
					constant_node_reference, value_expression_results[index].node_reference);
			}

			for (size_t index = 0; index < value_expression_results.size(); index++) {
				remove_temporary_reference(value_expression_results[index].node_reference);
			}
		}

		s_expression_result result;
		result.type = node->get_data_type();
		result.node_reference = constant_node_reference;
		m_expression_stack.push(result);

		add_temporary_reference(constant_node_reference);
	}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {
		// Return the node containing this named value's current value, as well as the named value's name
		s_identifier *identifier = get_identifier(node->get_name());

		s_expression_result result;
		result.type = identifier->data_type;
		result.node_reference = identifier->current_value_node_reference;
		result.identifier_name = node->get_name();
		m_expression_stack.push(result);

		add_temporary_reference(result.node_reference);
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
		wl_assert(module_call_declaration);

		c_execution_graph_module_builder module_builder(
			m_native_module_library_contexts,
			m_errors,
			m_ast_root,
			m_execution_graph,
			m_instrument_globals,
			module_call_declaration);

		// Hook up the input arguments
		for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = module_call_declaration->get_argument(arg);
			s_expression_result result = argument_expression_results[arg];

			e_ast_qualifier qualifier = argument->get_qualifier();
			if (qualifier == e_ast_qualifier::k_in) {
				// We expect to be able to provide a node as input
				wl_assert(result.node_reference.is_valid());
				module_builder.set_in_argument_node_reference(arg, result.node_reference);
			} else {
				wl_assert(qualifier == e_ast_qualifier::k_out);
				// We expect a named value to store the output in
				wl_assert(!result.identifier_name.empty());
				// We will later expect that all outputs get filled with node indices
			}
		}

		module_call_declaration->iterate(&module_builder);

		// Remove temporary references from the input arguments after building the module so they don't go out of scope
		// too soon
		for (size_t arg = 0; arg < argument_expression_results.size(); arg++) {
			remove_temporary_reference(argument_expression_results[arg].node_reference);
		}

		// Hook up the output arguments
		for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = module_call_declaration->get_argument(arg);
			s_expression_result result = argument_expression_results[arg];

			e_ast_qualifier qualifier = argument->get_qualifier();
			if (qualifier == e_ast_qualifier::k_out) {
				c_node_reference out_node_reference = module_builder.get_out_argument_node_reference(arg);
				wl_assert(out_node_reference.is_valid());
				update_identifier_node_reference(result.identifier_name, out_node_reference);
			}
		}

		// Push this module call's result onto the stack
		s_expression_result result;
		result.type = module_call_declaration->get_return_type();
		if (module_call_declaration->get_return_type() != c_ast_data_type(e_ast_primitive_type::k_void)) {
			result.node_reference = module_builder.get_return_value_node_reference();
			wl_assert(result.node_reference.is_valid());
		} else {
			// Void return value
			result.node_reference = c_node_reference();
		}

		m_expression_stack.push(result);
		add_temporary_reference(result.node_reference);
	}
};

s_compiler_result c_execution_graph_builder::build_execution_graphs(
	c_wrapped_array<void *> native_module_library_contexts,
	const c_ast_node *ast,
	c_instrument_variant *instrument_variant_out,
	std::vector<s_compiler_result> &errors_out) {
	s_compiler_result result;
	result.clear();

	// Find the entry points for voice and FX modules
	const c_ast_node_module_declaration *voice_entry_point_module =
		c_execution_graph_module_builder::find_module_declaration_single(ast, k_voice_entry_point_name);
	const c_ast_node_module_declaration *fx_entry_point_module =
		c_execution_graph_module_builder::find_module_declaration_single(ast, k_fx_entry_point_name);

	wl_assert(voice_entry_point_module || fx_entry_point_module);

	if (voice_entry_point_module) {
		c_execution_graph *execution_graph = new c_execution_graph();
		instrument_variant_out->set_voice_execution_graph(execution_graph);

		c_execution_graph_module_builder builder(
			native_module_library_contexts,
			&errors_out,
			ast,
			execution_graph,
			&instrument_variant_out->get_instrument_globals(),
			voice_entry_point_module);
		voice_entry_point_module->iterate(&builder);

		// Add an output node for each argument (they should all be out arguments)
		for (size_t arg = 0; arg < voice_entry_point_module->get_argument_count(); arg++) {
			wl_assert(voice_entry_point_module->get_argument(arg)->get_qualifier() == e_ast_qualifier::k_out);

			c_node_reference output_node_reference = execution_graph->add_output_node(cast_integer_verify<uint32>(arg));
			c_node_reference out_argument_node_reference = builder.get_out_argument_node_reference(arg);
			execution_graph->add_edge(out_argument_node_reference, output_node_reference);
		}

		// Add an output for the remain-active result
		{
			wl_assert(voice_entry_point_module->get_return_type() == c_ast_data_type(e_ast_primitive_type::k_bool));

			c_node_reference output_node_reference = execution_graph->add_output_node(
				c_execution_graph::k_remain_active_output_index);
			c_node_reference out_argument_node_reference = builder.get_return_value_node_reference();
			execution_graph->add_edge(out_argument_node_reference, output_node_reference);
		}
	}

	if (fx_entry_point_module) {
		c_execution_graph *execution_graph = new c_execution_graph();
		instrument_variant_out->set_fx_execution_graph(execution_graph);

		c_execution_graph_module_builder builder(
			native_module_library_contexts,
			&errors_out,
			ast,
			execution_graph,
			&instrument_variant_out->get_instrument_globals(),
			fx_entry_point_module);

		// Add an input node for each input argument
		uint32 input_index = 0;
		for (size_t arg = 0; arg < fx_entry_point_module->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = fx_entry_point_module->get_argument(arg);

			if (argument->get_qualifier() == e_ast_qualifier::k_in) {
				c_node_reference input_node_reference = execution_graph->add_input_node(input_index);
				builder.set_in_argument_node_reference(arg, input_node_reference);
				input_index++;
			} else {
				wl_assert(argument->get_qualifier() == e_ast_qualifier::k_out);
			}
		}

		fx_entry_point_module->iterate(&builder);

		// Add an output node for each output argument
		uint32 output_index = 0;
		for (size_t arg = 0; arg < fx_entry_point_module->get_argument_count(); arg++) {
			const c_ast_node_named_value_declaration *argument = fx_entry_point_module->get_argument(arg);

			if (argument->get_qualifier() == e_ast_qualifier::k_out) {
				c_node_reference output_node_reference = execution_graph->add_output_node(output_index);
				c_node_reference out_argument_node_reference = builder.get_out_argument_node_reference(arg);
				execution_graph->add_edge(out_argument_node_reference, output_node_reference);
				output_index++;
			} else {
				wl_assert(argument->get_qualifier() == e_ast_qualifier::k_in);
			}
		}

		if (voice_entry_point_module) {
			// The output count from the voice graph should match the input count from the FX graph
			wl_assert(input_index == voice_entry_point_module->get_argument_count());
		}

		// Add an output for the remain-active result
		{
			wl_assert(fx_entry_point_module->get_return_type() == c_ast_data_type(e_ast_primitive_type::k_bool));

			c_node_reference output_node_reference = execution_graph->add_output_node(
				c_execution_graph::k_remain_active_output_index);
			c_node_reference out_argument_node_reference = builder.get_return_value_node_reference();
			execution_graph->add_edge(out_argument_node_reference, output_node_reference);
		}
	}

	if (!errors_out.empty()) {
		// Don't associate the error with a particular file, those errors are collected through errors_out
		result.result = e_compiler_result::k_graph_error;
		result.message = "Graph error(s) detected";
	}

	return result;
}
