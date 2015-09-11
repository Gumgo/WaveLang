#include "compiler/execution_graph_optimizer.h"
#include "execution_graph/native_modules.h"
#include <stack>
#include <array>

// Each optimization rule consists of two descriptions, "from" and "to"
// Each describes a possible node pattern that could be found in the graph
// If the "from" pattern is identified, it is replaced with the "to" pattern
// The descriptions consist of a list of module calls and placeholders, e.g.
// ADD( X1, NEG( X2 ) ) => SUB( X1, X2 )
// Internally, these are represented as two lists:
// ADD, X1, NEG, X2, END, END
// SUB, X1, X2, END

enum e_optimization_rule_description_component_type {
	k_optimization_rule_description_component_type_invalid, // Marks the end of the list

	k_optimization_rule_description_component_type_native_module,
	k_optimization_rule_description_component_type_native_module_end,
	k_optimization_rule_description_component_type_variable,
	k_optimization_rule_description_component_type_constant,
	k_optimization_rule_description_component_type_real_value,
	k_optimization_rule_description_component_type_bool_value,

	k_optimization_rule_description_component_type_count
};

struct s_optimization_rule_description_component {
	e_optimization_rule_description_component_type type;
	union {
		uint32 index;
		real32 real_value;
		bool bool_value;
		// Currently no string value (likely will never be needed)
	} data;


	s_optimization_rule_description_component() {
		type = k_optimization_rule_description_component_type_invalid;
		data.index = 0;
	}

	static s_optimization_rule_description_component native_module(e_native_module native_module) {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_native_module;
		result.data.index = static_cast<uint32>(native_module);
		return result;
	}

	static s_optimization_rule_description_component native_module_end() {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_native_module_end;
		result.data.index = 0;
		return result;
	}

	static s_optimization_rule_description_component variable(uint32 index) {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_variable;
		result.data.index = index;
		return result;
	}

	static s_optimization_rule_description_component constant(uint32 index) {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_constant;
		result.data.index = index;
		return result;
	}

	static s_optimization_rule_description_component real_value(real32 real_value) {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_real_value;
		result.data.real_value = real_value;
		return result;
	}

	static s_optimization_rule_description_component bool_value(bool bool_value) {
		s_optimization_rule_description_component result;
		result.type = k_optimization_rule_description_component_type_bool_value;
		result.data.bool_value = bool_value;
		return result;
	}

	bool is_valid() const {
		return type != k_optimization_rule_description_component_type_invalid;
	}
};

struct s_optimization_rule_description {
	static const size_t k_max_length = 16;
	s_optimization_rule_description_component components[k_max_length];
};

struct s_optimization_rule {
	s_optimization_rule_description from;
	s_optimization_rule_description to;
};

// Shorthand for placeholders
#define X0		s_optimization_rule_description_component::variable(0)
#define X1		s_optimization_rule_description_component::variable(1)
#define X2		s_optimization_rule_description_component::variable(2)
#define X3		s_optimization_rule_description_component::variable(3)
#define C0		s_optimization_rule_description_component::constant(0)
#define C1		s_optimization_rule_description_component::constant(1)
#define C2		s_optimization_rule_description_component::constant(2)
#define C3		s_optimization_rule_description_component::constant(3)
#define RV(r)	s_optimization_rule_description_component::real_value(r)
#define BV(b)	s_optimization_rule_description_component::bool_value(b)

// Shorthand for native modules
#define NM_BEGIN(x)		s_optimization_rule_description_component::native_module(x)
#define NM_END			s_optimization_rule_description_component::native_module_end()
#define NM_NOOP(x, ...)	NM_BEGIN(k_native_module_noop), x, ##__VA_ARGS__, NM_END
#define NM_NEG(x, ...)	NM_BEGIN(k_native_module_negation), x, ##__VA_ARGS__, NM_END
#define NM_ADD(x, ...)	NM_BEGIN(k_native_module_addition), x, ##__VA_ARGS__, NM_END
#define NM_SUB(x, ...)	NM_BEGIN(k_native_module_subtraction), x, ##__VA_ARGS__, NM_END
#define NM_MUL(x, ...)	NM_BEGIN(k_native_module_multiplication), x, ##__VA_ARGS__, NM_END
#define NM_DIV(x, ...)	NM_BEGIN(k_native_module_division), x, ##__VA_ARGS__, NM_END
#define NM_MOD(x, ...)	NM_BEGIN(k_native_module_modulo), x, ##__VA_ARGS__, NM_END
#define NM_RSEL(x, ...)	NM_BEGIN(k_native_module_real_static_select), x, ##__VA_ARGS__, NM_END
#define NM_SSEL(x, ...)	NM_BEGIN(k_native_module_string_static_select), x, ##__VA_ARGS__, NM_END
#define NM_SL(x, ...)	NM_BEGIN(k_native_module_sampler_loop), x, ##__VA_ARGS__, NM_END
#define NM_SLPS(x, ...)	NM_BEGIN(k_native_module_sampler_loop_phase_shift), x, ##__VA_ARGS__, NM_END

static const s_optimization_rule k_optimization_rules[] = {
#include "execution_graph_optimization_rules.txt"
};

static bool optimize_node(c_execution_graph *execution_graph, uint32 node_index);
static bool optimize_native_module_call(c_execution_graph *execution_graph, uint32 node_index);
static bool try_to_apply_optimization_rule(c_execution_graph *execution_graph, uint32 node_index, size_t rule_index);
static void remove_useless_nodes(c_execution_graph *execution_graph);
static void transfer_outputs(c_execution_graph *execution_graph,
	uint32 destination_index, uint32 source_index);
static void deduplicate_nodes(c_execution_graph *execution_graph);
static void validate_optimized_constants(const c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &out_errors);

s_compiler_result c_execution_graph_optimizer::optimize_graph(c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &out_errors) {
	s_compiler_result result;
	result.clear();

	// Keep making passes over the graph until no more optimizations can be applied
	bool optimization_performed;
	do {
		optimization_performed = false;

		for (uint32 index = 0; index < execution_graph->get_node_count(); index++) {
			optimization_performed |= optimize_node(execution_graph, index);
		}

		remove_useless_nodes(execution_graph);
	} while (optimization_performed);

	execution_graph->remove_unused_nodes_and_reassign_node_indices();

	deduplicate_nodes(execution_graph);
	execution_graph->remove_unused_nodes_and_reassign_node_indices();

#if PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
	execution_graph->output_to_file();
#endif // PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)

	validate_optimized_constants(execution_graph, out_errors);
	if (!out_errors.empty()) {
		result.result = k_compiler_result_optimization_error;
		result.message = "Optimization error(s) detected";
	}

	return result;
}

static bool optimize_node(c_execution_graph *execution_graph, uint32 node_index) {
	bool optimized = false;

	switch (execution_graph->get_node_type(node_index)) {
	case k_execution_graph_node_type_invalid:
		// This node was removed, skip it
		break;

	case k_execution_graph_node_type_constant:
		// We can't directly optimize constant nodes
		break;

	case k_execution_graph_node_type_native_module_call:
		optimized = optimize_native_module_call(execution_graph, node_index);
		break;

	case k_execution_graph_node_type_native_module_input:
		// Module call inputs can't be optimized
		break;

	case k_execution_graph_node_type_native_module_output:
		// Module call outputs can't be optimized
		break;

	case k_execution_graph_node_type_output:
		// Outputs can't be optimized
		break;

	default:
		wl_unreachable();
	}

	return optimized;
}

static bool optimize_native_module_call(c_execution_graph *execution_graph, uint32 node_index) {
	wl_assert(execution_graph->get_node_type(node_index) == k_execution_graph_node_type_native_module_call);
	const s_native_module &native_module = c_native_module_registry::get_native_module(
		execution_graph->get_native_module_call_native_module_index(node_index));

	// If this is a no-op, we can trivially remove it
	if (execution_graph->get_native_module_call_native_module_index(node_index) == k_native_module_noop) {
		// Redirect input directly to outputs
		uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, 0);
		uint32 output_node_index = execution_graph->get_node_outgoing_edge_index(node_index, 0);
		wl_assert(execution_graph->get_node_incoming_edge_count(input_node_index) == 1);
		uint32 from_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);
		transfer_outputs(execution_graph, from_index, output_node_index);
		execution_graph->remove_node(node_index);
		return true;
	}

	// Determine if all inputs are constants
	if (native_module.compile_time_call) {
		bool all_constant_inputs = true;
		size_t input_count = execution_graph->get_node_incoming_edge_count(node_index);
		for (size_t edge = 0; all_constant_inputs && edge < input_count; edge++) {
			uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, edge);
			uint32 input_type_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);
			if (execution_graph->get_node_type(input_type_node_index) != k_execution_graph_node_type_constant) {
				all_constant_inputs = false;
			}
		}

		if (all_constant_inputs) {
			// Perform the native module call to resolve the constant value
			size_t next_input = 0;
			size_t next_output = 0;
			c_native_module_compile_time_argument_list arg_list;

			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				s_native_module_compile_time_argument compile_time_argument;
#if PREDEFINED(ASSERTS_ENABLED)
				compile_time_argument.assigned = false;
				compile_time_argument.argument = argument;
#endif // PREDEFINED(ASSERTS_ENABLED)

				if (argument.qualifier == k_native_module_argument_qualifier_in ||
					argument.qualifier == k_native_module_argument_qualifier_constant) {
					uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, next_input);
					uint32 constant_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);

					switch (execution_graph->get_constant_node_data_type(constant_node_index)) {
					case k_native_module_argument_type_real:
						compile_time_argument.real_value =
							execution_graph->get_constant_node_real_value(constant_node_index);
						break;

					case k_native_module_argument_type_bool:
						compile_time_argument.bool_value =
							execution_graph->get_constant_node_bool_value(constant_node_index);
						break;

					case k_native_module_argument_type_string:
						compile_time_argument.string_value =
							execution_graph->get_constant_node_string_value(constant_node_index);
						break;

					default:
						wl_unreachable();
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
			native_module.compile_time_call(arg_list);

			next_output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				if (argument.qualifier == k_native_module_argument_qualifier_out) {
					const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];
					wl_assert(compile_time_argument.assigned);

					// Create a constant node for this output
					uint32 constant_node_index = c_execution_graph::k_invalid_index;
					switch (argument.type) {
					case k_native_module_argument_type_real:
						constant_node_index = execution_graph->add_constant_node(compile_time_argument.real_value);
						break;

					case k_native_module_argument_type_bool:
						constant_node_index = execution_graph->add_constant_node(compile_time_argument.bool_value);
						break;

					case k_native_module_argument_type_string:
						constant_node_index = execution_graph->add_constant_node(compile_time_argument.string_value);
						break;

					default:
						wl_unreachable();
					}

					// Hook up all outputs with this node instead
					uint32 output_node_index = execution_graph->get_node_outgoing_edge_index(node_index, next_output);
					while (execution_graph->get_node_outgoing_edge_count(output_node_index) > 0) {
						uint32 to_index = execution_graph->get_node_outgoing_edge_index(output_node_index, 0);
						execution_graph->remove_edge(output_node_index, to_index);
						execution_graph->add_edge(constant_node_index, to_index);
					}

					next_output++;
				}
			}

			wl_assert(next_output == native_module.out_argument_count);

			// Finally, remove the native module call entirely
			execution_graph->remove_node(node_index);
			return true;
		}
	}

	// Try to apply all the optimization rules
	for (size_t rule = 0; rule < NUMBEROF(k_optimization_rules); rule++) {
		if (try_to_apply_optimization_rule(execution_graph, node_index, rule)) {
			return true;
		}
	}

	return false;
}

static bool try_to_apply_optimization_rule(c_execution_graph *execution_graph, uint32 node_index, size_t rule_index) {
	// Currently we have the limitation that modules used in rules must have only a single output, and that output must
	// be the return value. This is due to the syntax we use to express optimizations.
	const s_optimization_rule &rule = k_optimization_rules[rule_index];

	// Determine if the rule matches
	struct s_match_state {
		uint32 current_node_index;
		uint32 current_node_output_index;
		uint32 next_input_index;

		bool has_more_inputs(const c_execution_graph *execution_graph) const {
			return next_input_index < execution_graph->get_node_incoming_edge_count(current_node_index);
		}

		uint32 follow_next_input(const c_execution_graph *execution_graph, uint32 &output_node_index) {
			wl_assert(has_more_inputs(execution_graph));
			// Advance twice to skip the module input node
			uint32 new_node_index = execution_graph->get_node_incoming_edge_index(
				execution_graph->get_node_incoming_edge_index(current_node_index, next_input_index), 0);
			if (execution_graph->get_node_type(new_node_index) == k_execution_graph_node_type_native_module_output) {
				// We need to advance an additional node for the case (module <- input <- output <- module)
				output_node_index = new_node_index;
				new_node_index = execution_graph->get_node_incoming_edge_index(new_node_index, 0);
			}
			next_input_index++;
			return new_node_index;
		}
	};
	std::stack<s_match_state> match_state_stack;

	const size_t k_max_matches = 4;
	std::array<uint32, k_max_matches> matched_value_node_indices;
	std::array<uint32, k_max_matches> matched_constant_node_indices;
	std::fill(matched_value_node_indices.begin(), matched_value_node_indices.end(),
		c_execution_graph::k_invalid_index);
	std::fill(matched_constant_node_indices.begin(), matched_constant_node_indices.end(),
		c_execution_graph::k_invalid_index);

#if PREDEFINED(ASSERTS_ENABLED)
	bool should_be_done = false;
#endif // PREDEFINED(ASSERTS_ENABLED)
	for (size_t c = 0; c < NUMBEROF(rule.from.components) && rule.from.components[c].is_valid(); c++) {
		wl_assert(!should_be_done);

		const s_optimization_rule_description_component &component = rule.from.components[c];
		switch (component.type) {
		case k_optimization_rule_description_component_type_native_module:
		{
			if (match_state_stack.empty()) {
				// Only do this when starting out
				s_match_state initial_state;
				initial_state.current_node_output_index = c_execution_graph::k_invalid_index;
				initial_state.current_node_index = node_index;
				initial_state.next_input_index = 0;
				match_state_stack.push(initial_state);
			} else {
				// Try to advance to the next input
				s_match_state &current_state = match_state_stack.top();
				if (!current_state.has_more_inputs(execution_graph)) {
					return false;
				}

				s_match_state new_state;
				new_state.current_node_output_index = c_execution_graph::k_invalid_index;
				new_state.current_node_index = current_state.follow_next_input(execution_graph,
					new_state.current_node_output_index);
				new_state.next_input_index = 0;
				match_state_stack.push(new_state);
			}

			// The module described by the rule should match the top of the state stack
			const s_match_state &current_state = match_state_stack.top();
			uint32 current_index = current_state.current_node_index;
			if (execution_graph->get_node_type(current_index) != k_execution_graph_node_type_native_module_call ||
				component.data.index != execution_graph->get_native_module_call_native_module_index(current_index)) {
				// Not a match
				return false;
			}
			break;
		}

		case k_optimization_rule_description_component_type_native_module_end:
		{
			wl_assert(!match_state_stack.empty());
			// We expect that if we were able to match and enter a module, we should also match when leaving
			// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
#if PREDEFINED(ASSERTS_ENABLED)
			const s_match_state &current_state = match_state_stack.top();
			wl_assert(!current_state.has_more_inputs(execution_graph));
			if (match_state_stack.size() == 1) {
				should_be_done = true;
			}
#endif // PREDEFINED(ASSERTS_ENABLED)
			match_state_stack.pop();
			break;
		}

		case k_optimization_rule_description_component_type_variable:
		case k_optimization_rule_description_component_type_constant:
		case k_optimization_rule_description_component_type_real_value:
		case k_optimization_rule_description_component_type_bool_value:
		{
			// Try to advance to the next input
			wl_assert(!match_state_stack.empty());
			s_match_state &current_state = match_state_stack.top();
			if (!current_state.has_more_inputs(execution_graph)) {
				return false;
			}

			uint32 new_node_output_index = c_execution_graph::k_invalid_index;
			uint32 new_node_index = current_state.follow_next_input(execution_graph, new_node_output_index);

			if (component.type == k_optimization_rule_description_component_type_variable) {
				// Match anything except for constants
				if (execution_graph->get_node_type(new_node_index) == k_execution_graph_node_type_constant) {
					return false;
				} else {
					wl_assert(matched_value_node_indices[component.data.index] == c_execution_graph::k_invalid_index);
					// If this is a module node, it means we had to pass through an output node to get here. Store
					// the output node as the match, because that's what inputs will be hooked up to.
					if (new_node_output_index == c_execution_graph::k_invalid_index) {
						matched_value_node_indices[component.data.index] = new_node_index;
					} else {
						matched_value_node_indices[component.data.index] = new_node_output_index;
					}
				}
			} else if (component.type == k_optimization_rule_description_component_type_constant) {
				// Match only constants
				if (execution_graph->get_node_type(new_node_index) != k_execution_graph_node_type_constant) {
					return false;
				} else {
					wl_assert(matched_constant_node_indices[component.data.index] ==
						c_execution_graph::k_invalid_index);
					matched_constant_node_indices[component.data.index] = new_node_index;
				}
			} else {
				// Match only constants with the given value
				if (execution_graph->get_node_type(new_node_index) != k_execution_graph_node_type_constant) {
					return false;
				}

				if (component.type == k_optimization_rule_description_component_type_real_value) {
					if (execution_graph->get_constant_node_data_type(new_node_index) !=
						k_native_module_argument_type_real ||
						execution_graph->get_constant_node_real_value(new_node_index) != component.data.real_value) {
						return false;
					}
				} else {
					wl_assert(component.type == k_optimization_rule_description_component_type_bool_value);
					if (execution_graph->get_constant_node_data_type(new_node_index) !=
						k_native_module_argument_type_bool ||
						execution_graph->get_constant_node_bool_value(new_node_index) != component.data.bool_value) {
						return false;
					}

				}
			}
			break;
		}

		default:
			wl_unreachable();
			return false;
		}
	}

	wl_assert(match_state_stack.empty());

	// If we have not returned false yet, it means we have matched. Replaced the matched nodes with the ones defined by
	// the rule.
	uint32 root_module_node_index = c_execution_graph::k_invalid_index;
#if PREDEFINED(ASSERTS_ENABLED)
	should_be_done = false;
#endif // PREDEFINED(ASSERTS_ENABLED)
	for (size_t c = 0; c < NUMBEROF(rule.to.components) && rule.to.components[c].is_valid(); c++) {
		wl_assert(!should_be_done);

		const s_optimization_rule_description_component &component = rule.to.components[c];
		switch (component.type) {
		case k_optimization_rule_description_component_type_native_module:
		{
			uint32 node_index = execution_graph->add_native_module_call_node(component.data.index);
			// We currently only allow a single outgoing edge, due to the way we express rules
			wl_assert(execution_graph->get_node_outgoing_edge_count(node_index) == 1);

			if (root_module_node_index == c_execution_graph::k_invalid_index) {
				wl_assert(match_state_stack.empty());
				root_module_node_index = node_index;
			} else {
				s_match_state &current_state = match_state_stack.top();
				wl_assert(current_state.has_more_inputs(execution_graph));

				uint32 input_node_index = execution_graph->get_node_incoming_edge_index(
					current_state.current_node_index, current_state.next_input_index);
				uint32 output_node_index = execution_graph->get_node_outgoing_edge_index(node_index, 0);
				execution_graph->add_edge(output_node_index, input_node_index);
				current_state.next_input_index++;
			}

			s_match_state new_state;
			new_state.current_node_output_index = c_execution_graph::k_invalid_index;
			new_state.current_node_index = node_index;
			new_state.next_input_index = 0;
			match_state_stack.push(new_state);
			break;
		}

		case k_optimization_rule_description_component_type_native_module_end:
		{
			wl_assert(!match_state_stack.empty());
			// We expect that if we were able to match and enter a module, we should also match when leaving
			// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
#if PREDEFINED(ASSERTS_ENABLED)
			const s_match_state &current_state = match_state_stack.top();
			wl_assert(!current_state.has_more_inputs(execution_graph));
			if (match_state_stack.size() == 1) {
				should_be_done = true;
			}
#endif // PREDEFINED(ASSERTS_ENABLED)
			match_state_stack.pop();
			break;
		}

		case k_optimization_rule_description_component_type_variable:
		case k_optimization_rule_description_component_type_constant:
		case k_optimization_rule_description_component_type_real_value:
		case k_optimization_rule_description_component_type_bool_value:
		{
			// Hook up this input and advance
			wl_assert(!match_state_stack.empty());
			s_match_state &current_state = match_state_stack.top();
			wl_assert(current_state.has_more_inputs(execution_graph));

			uint32 input_node_index = execution_graph->get_node_incoming_edge_index(
				current_state.current_node_index, current_state.next_input_index);
			current_state.next_input_index++;

			if (component.type == k_optimization_rule_description_component_type_variable) {
				execution_graph->add_edge(matched_value_node_indices[component.data.index], input_node_index);
			} else if (component.type == k_optimization_rule_description_component_type_constant) {
				execution_graph->add_edge(matched_constant_node_indices[component.data.index], input_node_index);
			} else if (component.type == k_optimization_rule_description_component_type_real_value) {
				// Create a constant node with this value
				uint32 new_constant_node_index = execution_graph->add_constant_node(component.data.real_value);
				execution_graph->add_edge(new_constant_node_index, input_node_index);
			} else {
				wl_assert(component.type == k_optimization_rule_description_component_type_bool_value);
				// Create a constant node with this value
				uint32 new_constant_node_index = execution_graph->add_constant_node(component.data.bool_value);
				execution_graph->add_edge(new_constant_node_index, input_node_index);
			}
			break;
		}

		default:
			wl_unreachable();
		}
	}

	wl_assert(match_state_stack.empty());

	// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
	// deleting the old set of nodes, they will automatically be cleaned up later.
	wl_assert(execution_graph->get_node_outgoing_edge_count(node_index) == 1);
	uint32 old_output_node = execution_graph->get_node_outgoing_edge_index(node_index, 0);
	wl_assert(execution_graph->get_node_outgoing_edge_count(root_module_node_index) == 1);
	uint32 new_output_node = execution_graph->get_node_outgoing_edge_index(root_module_node_index, 0);
	transfer_outputs(execution_graph, new_output_node, old_output_node);

	return true;
}

void remove_useless_nodes(c_execution_graph *execution_graph) {
	std::stack<uint32> node_stack;
	std::vector<bool> nodes_visited(execution_graph->get_node_count(), false);

	// Start at the output nodes and work backwards, marking each node as it is encountered
	for (uint32 index = 0; index < execution_graph->get_node_count(); index++) {
		if (execution_graph->get_node_type(index) == k_execution_graph_node_type_output) {
			node_stack.push(index);
			nodes_visited[index] = true;
		}
	}

	while (!node_stack.empty()) {
		uint32 index = node_stack.top();
		node_stack.pop();

		for (size_t edge = 0; edge < execution_graph->get_node_incoming_edge_count(index); edge++) {
			uint32 input_index = execution_graph->get_node_incoming_edge_index(index, edge);
			if (!nodes_visited[input_index]) {
				node_stack.push(input_index);
				nodes_visited[input_index] = true;
			}
		}
	}

	// Remove all unvisited nodes
	for (uint32 index = 0; index < execution_graph->get_node_count(); index++) {
		if (!nodes_visited[index]) {
			// Don't directly remove module inputs/outputs, since they are linked with the modules themselves
			e_execution_graph_node_type type = execution_graph->get_node_type(index);
			if (type != k_execution_graph_node_type_native_module_input &&
				type != k_execution_graph_node_type_native_module_output) {
				execution_graph->remove_node(index);
			}
		}
	}
}

static void transfer_outputs(c_execution_graph *execution_graph,
	uint32 destination_index, uint32 source_index) {
	// Redirect the inputs of input_node directly to the outputs of output_node
	while (execution_graph->get_node_outgoing_edge_count(source_index) > 0) {
		uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(source_index, 0);
		execution_graph->remove_edge(source_index, to_node_index);
		execution_graph->add_edge(destination_index, to_node_index);
	}
}

static void deduplicate_nodes(c_execution_graph *execution_graph) {
	// 1) deduplicate all constant nodes with the same value
	// 2) combine any native modules with the same type and inputs
	// 3) repeat (2) until no changes occur

	// Combine all equivalent constant nodes. Currently n^2, we could easily do better if it's worth the speedup.
	for (uint32 node_a_index = 0; node_a_index < execution_graph->get_node_count(); node_a_index++) {
		if (execution_graph->get_node_type(node_a_index) != k_execution_graph_node_type_constant) {
			continue;
		}

		for (uint32 node_b_index = node_a_index + 1; node_b_index < execution_graph->get_node_count(); node_b_index++) {
			if (execution_graph->get_node_type(node_b_index) != k_execution_graph_node_type_constant) {
				continue;
			}

			// Both nodes are constant nodes
			e_native_module_argument_type type = execution_graph->get_constant_node_data_type(node_a_index);
			if (type != execution_graph->get_constant_node_data_type(node_b_index)) {
				continue;
			}

			bool equal = false;
			switch (type) {
			case k_native_module_argument_type_real:
				equal = (execution_graph->get_constant_node_real_value(node_a_index) ==
					execution_graph->get_constant_node_real_value(node_b_index));
				break;

			case k_native_module_argument_type_bool:
				equal = (execution_graph->get_constant_node_bool_value(node_a_index) ==
					execution_graph->get_constant_node_bool_value(node_b_index));
				break;

			case k_native_module_argument_type_string:
				equal = (strcmp(execution_graph->get_constant_node_string_value(node_a_index),
					execution_graph->get_constant_node_string_value(node_b_index)) == 0);
				break;

			default:
				wl_unreachable();
			}

			if (equal) {
				// Redirect node B's outputs as outputs of node A
				for (size_t edge = 0; edge < execution_graph->get_node_outgoing_edge_count(node_b_index); edge++) {
					uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(node_b_index, edge);
					execution_graph->remove_edge(node_b_index, to_node_index);
					execution_graph->add_edge(node_a_index, to_node_index);
				}

				// Remove node B, since it has no outgoing edges left
				execution_graph->remove_node(node_b_index);
			}
		}
	}

	// Repeat the following until no changes occur:
	bool graph_changed;
	do {
		graph_changed = false;

		// Find any module calls with identical index and inputs and merge them.
		// Currently n^2 but we could do better easily.
		for (uint32 node_a_index = 0; node_a_index < execution_graph->get_node_count(); node_a_index++) {
			if (execution_graph->get_node_type(node_a_index) != k_execution_graph_node_type_native_module_call) {
				continue;
			}

			for (uint32 node_b_index = node_a_index + 1;
				 node_b_index < execution_graph->get_node_count();
				 node_b_index++) {
				if (execution_graph->get_node_type(node_b_index) != k_execution_graph_node_type_native_module_call) {
					continue;
				}

				if (execution_graph->get_native_module_call_native_module_index(node_a_index) !=
					execution_graph->get_native_module_call_native_module_index(node_b_index)) {
					continue;
				}

				wl_assert(execution_graph->get_node_incoming_edge_count(node_a_index) ==
					execution_graph->get_node_incoming_edge_count(node_b_index));

				uint32 identical_inputs = true;
				for (size_t edge = 0;
					 identical_inputs && edge < execution_graph->get_node_incoming_edge_count(node_a_index);
					 edge++) {
					// Skip past the "input" nodes directly to the source
					uint32 source_node_a = execution_graph->get_node_incoming_edge_index(
						execution_graph->get_node_incoming_edge_index(node_a_index, edge), 0);
					uint32 source_node_b = execution_graph->get_node_incoming_edge_index(
						execution_graph->get_node_incoming_edge_index(node_b_index, edge), 0);

					identical_inputs = (source_node_a == source_node_b);
				}

				if (identical_inputs) {
					for (size_t edge = 0; edge < execution_graph->get_node_outgoing_edge_count(node_a_index); edge++) {
						uint32 output_node_a = execution_graph->get_node_outgoing_edge_index(node_a_index, edge);
						uint32 output_node_b = execution_graph->get_node_outgoing_edge_index(node_b_index, edge);

						for (size_t output = 0;
							 output < execution_graph->get_node_outgoing_edge_count(output_node_b);
							 output++) {
							uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(output_node_b, output);
							execution_graph->remove_edge(output_node_b, to_node_index);
							execution_graph->add_edge(output_node_a, to_node_index);
						}
					}

					// Remove node B, since it has no outgoing edges left
					execution_graph->remove_node(node_b_index);
					graph_changed = true;
				}
			}
		}
	} while (graph_changed);
}

static void validate_optimized_constants(const c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &out_errors) {
	for (uint32 node_index = 0; node_index < execution_graph->get_node_count(); node_index++) {
		if (execution_graph->get_node_type(node_index) != k_execution_graph_node_type_native_module_call) {
			continue;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			execution_graph->get_native_module_call_native_module_index(node_index));

		wl_assert(native_module.in_argument_count == execution_graph->get_node_incoming_edge_count(node_index));
		// For each constant input, verify that a constant node is linked up
		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			e_native_module_argument_qualifier argument_qualifier = native_module.arguments[arg].qualifier;
			if (argument_qualifier == k_native_module_argument_qualifier_in) {
				input++;
			} else if (argument_qualifier == k_native_module_argument_qualifier_constant) {
				// Validate that this input is constant
				uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, input);
				uint32 constant_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);

				if (execution_graph->get_node_type(constant_node_index) != k_execution_graph_node_type_constant) {
					s_compiler_result error;
					error.result = k_compiler_result_constant_expected;
					error.source_location.clear();
					error.message = "Input argument to native module call '" + std::string(native_module.name) +
						"' does not resolve to a constant";
					out_errors.push_back(error);
				}

				input++;
			}
		}

		wl_assert(input == native_module.in_argument_count);
	}
}
