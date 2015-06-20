#include "compiler\execution_graph_builder.h"
#include "compiler\ast.h"
#include "execution_graph\execution_graph.h"
#include "execution_graph\native_modules.h"
#include <map>
#include <deque>
#include <stack>

class c_execution_graph_module_builder : public c_ast_node_const_visitor {
private:
	enum e_expression_expectation {
		k_expression_expectation_value,			// This expression must have a value
		k_expression_expectation_assignment,	// This expression must be a single unassigned identifier

		k_expression_expectation_count
	};

	struct s_scope {
		// Maps the identifier name to the index of the intermediate value node holding its value
		std::map<std::string, uint32> identifiers;
	};

	const c_ast_node_module_declaration *m_module_declaration;

	size_t m_arguments_found;

	std::deque<s_scope> m_scope_stack;

	// While building expressions, we push the node which the return value should be stored in to this stack
	std::stack<uint32> m_return_value_node_stack;
	std::stack<e_expression_expectation> m_expression_expectation_stack;

	uint32 m_unnamed_identifier_count;

public:
	const c_ast_node *m_ast_root;
	c_execution_graph *m_execution_graph;

	// Follows argument order
	std::vector<uint32> m_input_output_node_indices;
	uint32 m_return_node_index;

	c_execution_graph_module_builder() {
		m_arguments_found = 0;
		m_execution_graph = nullptr;
		m_module_declaration = nullptr;
		m_unnamed_identifier_count = 0;
	}

	void add_identifier_to_scope(const std::string &identifier, uint32 intermediate_value_node_index) {
#if PREDEFINED(ASSERTS_ENABLED)
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			wl_assert(m_scope_stack[index].identifiers.find(identifier) == m_scope_stack[index].identifiers.end());
		}
#endif // PREDEFINED(ASSERTS_ENABLED)

		wl_assert(m_execution_graph->get_node_type(intermediate_value_node_index) ==
			k_execution_graph_node_type_intermediate_value);
		m_scope_stack.front().identifiers.insert(std::make_pair(identifier, intermediate_value_node_index));
	}

	uint32 get_identifier_node_index(const std::string &identifier) const {
		for (size_t index = 0; index < m_scope_stack.size(); index++) {
			auto match = m_scope_stack[index].identifiers.find(identifier);
			if (match != m_scope_stack[index].identifiers.end() &&
				match->first == identifier) {
				wl_assert(m_execution_graph->get_node_type(match->second) ==
					k_execution_graph_node_type_intermediate_value);
				return match->second;
			}
		}

		// Not found
		wl_halt();
		return c_execution_graph::k_invalid_index;
	}

	const c_ast_node_module_declaration *find_module_declaration(const char *name) {
		// Native modules must all be at the root
		wl_assert(m_ast_root->get_type() == k_ast_node_type_scope);

		const c_ast_node_scope *root_scope = static_cast<const c_ast_node_scope *>(m_ast_root);
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

		wl_halt();
		return nullptr;
	}

	virtual bool begin_visit(const c_ast_node_scope *node) {
		m_scope_stack.push_front(s_scope());
		return true;
	}

	virtual void end_visit(const c_ast_node_scope *node) {
		m_scope_stack.pop_front();
	}

	virtual bool begin_visit(const c_ast_node_module_declaration *node) {
		wl_assert(m_module_declaration == nullptr);
		m_module_declaration = node;
		// Push an extra scope in order to catch the arguments, since they will be visited before the module scope
		m_scope_stack.push_front(s_scope());
		return true;
	}

	virtual void end_visit(const c_ast_node_module_declaration *node) {
		m_scope_stack.pop_back(); // For completeness
	}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		uint32 node_index;
		c_ast_node_named_value_declaration::e_qualifier qualifier = node->get_qualifier();
		if (qualifier == c_ast_node_named_value_declaration::k_qualifier_in ||
			qualifier == c_ast_node_named_value_declaration::k_qualifier_out) {
			// Associate the argument node with the named value
			uint32 argument_node_index = m_input_output_node_indices[m_arguments_found];
			wl_assert(m_execution_graph->get_node_type(argument_node_index) ==
				k_execution_graph_node_type_intermediate_value);
			node_index = argument_node_index;
			m_arguments_found++;
		} else {
			wl_assert(qualifier == c_ast_node_named_value_declaration::k_qualifier_none);
			// Create a new intermediate value node for this identifier
			node_index = m_execution_graph->add_intermediate_value_node();
		}

		add_identifier_to_scope(node->get_name(), node_index);
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) {
		// Find the intermediate value node for this identifier
		uint32 node_index;

		if (node->get_named_value().empty()) {
			// If there is no named value being assigned, make a placeholder which will be optimized away
			std::string placeholder_name = "$unnamed_" + std::to_string(m_unnamed_identifier_count);
			m_unnamed_identifier_count++;
			node_index = m_execution_graph->add_intermediate_value_node();
			add_identifier_to_scope(placeholder_name, node_index);
		} else {
			// Look up the identifier being assigned
			node_index = get_identifier_node_index(node->get_named_value());
		}

		// Push the node to the stack so the expression can act as its input
		m_return_value_node_stack.push(node_index);
		m_expression_expectation_stack.push(k_expression_expectation_value);
		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_assignment *node) {
		wl_assert(m_return_value_node_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		wl_assert(m_module_declaration->get_has_return_value());
		wl_assert(m_return_node_index != c_execution_graph::k_invalid_index);

		// Push the return node to the stack so the expression can connect its result
		m_return_value_node_stack.push(m_return_node_index);
		m_expression_expectation_stack.push(k_expression_expectation_value);
		return true;
	}

	virtual void end_visit(const c_ast_node_return_statement *node) {
		wl_assert(m_return_value_node_stack.empty());
	}

	virtual bool begin_visit(const c_ast_node_expression *node) {
		// We should have at least one node to return into
		wl_assert(!m_return_value_node_stack.empty());
		wl_assert(!m_expression_expectation_stack.empty());
		return true;
	}

	virtual void end_visit(const c_ast_node_expression *node) {}

	virtual bool begin_visit(const c_ast_node_constant *node) {
		wl_assert(m_expression_expectation_stack.top() == k_expression_expectation_value);
		m_expression_expectation_stack.pop();

		uint32 constant_node_index = m_execution_graph->add_constant_node(node->get_value());

		// Attach this constant value to the input at the top of the return node stack
		uint32 return_value_node_index = m_return_value_node_stack.top();
		m_execution_graph->add_edge(constant_node_index, return_value_node_index);
		m_return_value_node_stack.pop();

		return true;
	}

	virtual void end_visit(const c_ast_node_constant *node) {}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		e_expression_expectation expectation = m_expression_expectation_stack.top();
		m_expression_expectation_stack.pop();

		uint32 named_value_node_index = get_identifier_node_index(node->get_name());

		uint32 return_value_node_index = m_return_value_node_stack.top();
		m_return_value_node_stack.pop();
		if (expectation == k_expression_expectation_value) {
			// Attach this named value to the input at the top of the return node stack
			m_execution_graph->add_edge(named_value_node_index, return_value_node_index);
		} else {
			wl_assert(expectation == k_expression_expectation_assignment);
			// The node on the stack actually acts as the input to this assignment, so add the edge in reverse
			m_execution_graph->add_edge(return_value_node_index, named_value_node_index);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {}

	virtual bool begin_visit(const c_ast_node_module_call *node) {
		wl_assert(m_expression_expectation_stack.top() == k_expression_expectation_value);
		m_expression_expectation_stack.pop();

		const c_ast_node_module_declaration *module_call_declaration = find_module_declaration(node->get_name().c_str());

		// Create intermediate value nodes for each argument and for the return value
		std::vector<uint32> module_argument_node_indices;
		for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
			module_argument_node_indices.push_back(m_execution_graph->add_intermediate_value_node());
		}
		uint32 return_value_node_index = m_return_value_node_stack.top();
		m_return_value_node_stack.pop();

		// If the module returns no value, just create a placeholder return value which will be optimized away
		if (!module_call_declaration->get_has_return_value()) {
			uint32 placeholder_index = m_execution_graph->add_constant_node(0.0f);
			m_execution_graph->add_edge(placeholder_index, return_value_node_index);
		}

		if (module_call_declaration->get_is_native()) {
			uint32 native_module_index = c_native_module_registry::get_native_module_index(node->get_name().c_str());
			const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);

			wl_assert(native_module.first_output_is_return == module_call_declaration->get_has_return_value());

			// Create a native module call node
			uint32 native_module_call_node_index = m_execution_graph->add_native_module_call_node(native_module_index);

			if (native_module.first_output_is_return) {
				wl_assert(native_module.argument_count == module_argument_node_indices.size() + 1);
			} else {
				wl_assert(native_module.argument_count == module_argument_node_indices.size());
			}

			// Hook up the native module inputs and outputs to the arguments
			uint32 next_input = 0;
			uint32 next_output = 0;

			if (native_module.first_output_is_return) {
				m_execution_graph->add_edge(
					m_execution_graph->get_node_outgoing_edge_index(native_module_call_node_index, 0),
					return_value_node_index);
				next_output++;
			}

			for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
				c_ast_node_named_value_declaration::e_qualifier qualifier =
					module_call_declaration->get_argument(arg)->get_qualifier();

				if (qualifier == c_ast_node_named_value_declaration::k_qualifier_in) {
					// This argument feeds into the module call input node
					uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
						native_module_call_node_index, next_input);
					m_execution_graph->add_edge(module_argument_node_indices[arg], input_node_index);
					next_input++;
				} else {
					wl_assert(qualifier == c_ast_node_named_value_declaration::k_qualifier_out);
					// This argument is assigned by the module call output node
					uint32 output_node_index = m_execution_graph->get_node_outgoing_edge_index(
						native_module_call_node_index, next_output);
					m_execution_graph->add_edge(output_node_index, module_argument_node_indices[arg]);
					next_output++;
				}
			}

			// Push to the stack in reverse, along with the proper expectation
			for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
				size_t reverse_arg = module_call_declaration->get_argument_count() - arg - 1;
				c_ast_node_named_value_declaration::e_qualifier qualifier =
					module_call_declaration->get_argument(reverse_arg)->get_qualifier();
				e_expression_expectation expectation =
					(qualifier == c_ast_node_named_value_declaration::k_qualifier_in) ?
					k_expression_expectation_value : k_expression_expectation_assignment;

				m_return_value_node_stack.push(module_argument_node_indices[reverse_arg]);
				m_expression_expectation_stack.push(expectation);
			}
		} else {
			// Push to the stack in reverse, along with the proper expectation
			for (size_t arg = 0; arg < module_call_declaration->get_argument_count(); arg++) {
				size_t reverse_arg = module_call_declaration->get_argument_count() - arg - 1;
				const c_ast_node_named_value_declaration *argument = module_call_declaration->get_argument(reverse_arg);
				wl_assert(argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_in ||
					argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out);
				e_expression_expectation expectation =
					(argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_in) ?
					k_expression_expectation_value : k_expression_expectation_assignment;

				m_return_value_node_stack.push(module_argument_node_indices[reverse_arg]);
				m_expression_expectation_stack.push(expectation);
			}

			// Build the called module
			c_execution_graph_module_builder module_builder;
			module_builder.m_ast_root = m_ast_root;
			module_builder.m_execution_graph = m_execution_graph;
			module_builder.m_input_output_node_indices = module_argument_node_indices;
			module_builder.m_return_node_index = module_call_declaration->get_has_return_value() ?
				return_value_node_index : c_execution_graph::k_invalid_index;

			module_call_declaration->iterate(&module_builder);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_module_call *node) {}
};

void c_execution_graph_builder::build_execution_graph(const c_ast_node *ast, c_execution_graph *out_execution_graph) {
	c_execution_graph_module_builder builder;
	builder.m_ast_root = ast;
	builder.m_execution_graph = out_execution_graph;

	// Find the entry point to start iteration
	const c_ast_node_module_declaration *entry_point_module = builder.find_module_declaration(k_entry_point_name);
	wl_assert(!entry_point_module->get_has_return_value());
	// Add an output node for each argument (they should all be out arguments)
	for (size_t arg = 0; arg < entry_point_module->get_argument_count(); arg++) {
		wl_assert(entry_point_module->get_argument(arg)->get_qualifier() ==
			c_ast_node_named_value_declaration::k_qualifier_out);

		uint32 output_node_index = out_execution_graph->add_output_node();
		uint32 intermediate_value_node_index = out_execution_graph->add_intermediate_value_node();
		out_execution_graph->add_edge(intermediate_value_node_index, output_node_index);
		builder.m_input_output_node_indices.push_back(intermediate_value_node_index);
	}

	entry_point_module->iterate(&builder);

#if PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
	out_execution_graph->output_to_file();
#endif // PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
}