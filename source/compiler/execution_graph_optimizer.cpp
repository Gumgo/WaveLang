#include "compiler/execution_graph_optimizer.h"

#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"

#include <array>
#include <stack>

static void build_array_value(const c_execution_graph *execution_graph, uint32 array_node_index,
	c_native_module_array &out_array_value);
static bool sanitize_array_index(const c_execution_graph *execution_graph, uint32 array_node_index, real32 array_index,
	uint32 &out_sanitized_array_index);

static bool optimize_node(c_execution_graph *execution_graph, uint32 node_index);
static bool optimize_native_module_call(c_execution_graph *execution_graph, uint32 node_index);
static bool try_to_apply_optimization_rule(c_execution_graph *execution_graph, uint32 node_index, uint32 rule_index);
static void remove_useless_nodes(c_execution_graph *execution_graph);
static void transfer_outputs(c_execution_graph *execution_graph,
	uint32 destination_index, uint32 source_index);
static void deduplicate_nodes(c_execution_graph *execution_graph);
static void validate_optimized_constants(const c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &out_errors);

c_execution_graph_constant_evaluator::c_execution_graph_constant_evaluator() {
	m_execution_graph = nullptr;
}

void c_execution_graph_constant_evaluator::initialize(const c_execution_graph *execution_graph) {
	wl_assert(!m_execution_graph);
	m_execution_graph = execution_graph;
	m_invalid_constant = true;
}

bool c_execution_graph_constant_evaluator::evaluate_constant(uint32 node_index) {
	wl_assert(m_execution_graph);
	m_invalid_constant = false;
	m_pending_nodes.push(node_index);

	while (!m_invalid_constant && !m_pending_nodes.empty()) {
		uint32 next_node_index = m_pending_nodes.top();
		wl_assert(m_execution_graph->get_node_type(next_node_index) == k_execution_graph_node_type_constant ||
			m_execution_graph->get_node_type(next_node_index) == k_execution_graph_node_type_indexed_output);

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

c_execution_graph_constant_evaluator::s_result c_execution_graph_constant_evaluator::get_result() const {
	wl_assert(m_execution_graph);
	wl_assert(!m_invalid_constant);
	return m_result;
}

void c_execution_graph_constant_evaluator::try_evaluate_node(uint32 node_index) {
	// Attempt to evaluate this node
	e_execution_graph_node_type node_type = m_execution_graph->get_node_type(node_index);
	if (node_type == k_execution_graph_node_type_constant) {
		// Easy - the node is simply a constant value already
		s_result result;
		result.type = m_execution_graph->get_constant_node_data_type(node_index);

		if (result.type.is_array()) {
			build_array_value(m_execution_graph, node_index, result.array_value);
		} else {
			switch (result.type.get_primitive_type()) {
			case k_native_module_primitive_type_real:
				result.real_value = m_execution_graph->get_constant_node_real_value(node_index);
				break;

			case k_native_module_primitive_type_bool:
				result.bool_value = m_execution_graph->get_constant_node_bool_value(node_index);
				break;

			case k_native_module_primitive_type_string:
				result.string_value.get_string() = m_execution_graph->get_constant_node_string_value(node_index);
				break;

			default:
				wl_unreachable();
			}
		}

		m_results.insert(std::make_pair(node_index, result));
	} else if (node_type == k_execution_graph_node_type_indexed_output) {
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
		std::vector<s_native_module_compile_time_argument> arg_list;

		bool all_inputs_evaluated = build_module_call_arguments(native_module, native_module_node_index, arg_list);

		// Make the compile time call to resolve the outputs
		if (all_inputs_evaluated) {
			c_native_module_compile_time_argument_list wrapped_arg_list(
				arg_list.empty() ? nullptr : &arg_list.front(), arg_list.size());
			native_module.compile_time_call(wrapped_arg_list);
			store_native_module_call_results(native_module, native_module_node_index, arg_list);
		}

	} else {
		// We should only ever add constants and native module output nodes
		wl_unreachable();
	}
}

bool c_execution_graph_constant_evaluator::build_module_call_arguments(const s_native_module &native_module,
	uint32 native_module_node_index, std::vector<s_native_module_compile_time_argument> &out_arg_list) {
	wl_assert(out_arg_list.empty());
	out_arg_list.resize(native_module.argument_count);

	// Perform the native module call to resolve the constant value
	size_t next_input = 0;
	size_t next_output = 0;

	bool all_inputs_evaluated = true;
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		s_native_module_compile_time_argument &compile_time_argument = out_arg_list[arg];
#if IS_TRUE(ASSERTS_ENABLED)
		compile_time_argument.type = argument.type;
#endif // IS_TRUE(ASSERTS_ENABLED)

		if (native_module_qualifier_is_input(argument.type.get_qualifier())) {
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

				if (input_result.type.is_array()) {
					compile_time_argument.array_value = input_result.array_value;
				} else {
					switch (input_result.type.get_primitive_type()) {
					case k_native_module_primitive_type_real:
						compile_time_argument.real_value = input_result.real_value;
						break;

					case k_native_module_primitive_type_bool:
						compile_time_argument.bool_value = input_result.bool_value;
						break;

					case k_native_module_primitive_type_string:
						compile_time_argument.string_value = input_result.string_value;
						break;

					default:
						wl_unreachable();
					}
				}
			}

			next_input++;
		} else {
			wl_assert(argument.type.get_qualifier() == k_native_module_qualifier_out);
			compile_time_argument.real_value = 0.0f;
			next_output++;
		}
	}

	wl_assert(next_input == native_module.in_argument_count);
	wl_assert(next_output == native_module.out_argument_count);

	return all_inputs_evaluated;
}

void c_execution_graph_constant_evaluator::store_native_module_call_results(const s_native_module &native_module,
	uint32 native_module_node_index, const std::vector<s_native_module_compile_time_argument> &arg_list) {
	size_t next_output = 0;

	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		if (argument.type.get_qualifier() == k_native_module_qualifier_out) {
			const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];

			// Store the output result
			s_result evaluation_result;
			evaluation_result.type = argument.type.get_data_type();

			if (argument.type.get_data_type().is_array()) {
				evaluation_result.array_value = compile_time_argument.array_value;
			} else {
				switch (argument.type.get_data_type().get_primitive_type()) {
				case k_native_module_primitive_type_real:
					evaluation_result.real_value = compile_time_argument.real_value;
					break;

				case k_native_module_primitive_type_bool:
					evaluation_result.bool_value = compile_time_argument.bool_value;
					break;

				case k_native_module_primitive_type_string:
					evaluation_result.string_value = compile_time_argument.string_value;
					break;
				default:
					wl_unreachable();
				}
			}

			uint32 output_node_index = m_execution_graph->get_node_outgoing_edge_index(
				native_module_node_index, next_output);
			m_results.insert(std::make_pair(output_node_index, evaluation_result));

			next_output++;
		}
	}

	wl_assert(next_output == native_module.out_argument_count);
}

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

	validate_optimized_constants(execution_graph, out_errors);
	if (!out_errors.empty()) {
		result.result = k_compiler_result_graph_error;
		result.message = "Graph error(s) detected";
	}

	return result;
}

static void build_array_value(const c_execution_graph *execution_graph, uint32 array_node_index,
	c_native_module_array &out_array_value) {
	wl_assert(execution_graph->get_node_type(array_node_index) == k_execution_graph_node_type_constant);
	wl_assert(execution_graph->get_constant_node_data_type(array_node_index).is_array());
	wl_assert(out_array_value.get_array().empty());

	size_t array_count = execution_graph->get_node_incoming_edge_count(array_node_index);
	out_array_value.get_array().reserve(array_count);

	for (size_t array_index = 0; array_index < array_count; array_index++) {
		// Jump past the indexed input node
		uint32 input_node_index = execution_graph->get_node_incoming_edge_index(array_node_index, array_index);
		uint32 value_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);
		out_array_value.get_array().push_back(value_node_index);
	}
}

static bool sanitize_array_index(const c_execution_graph *execution_graph, uint32 array_node_index, real32 array_index,
	uint32 &out_sanitized_array_index) {
	if (std::isnan(array_index) || std::isinf(array_index)) {
		return false;
	}

	// Casting to int will floor
	int32 array_index_signed = static_cast<int32>(std::floor(array_index));
	if (array_index_signed < 0) {
		return false;
	}

	uint32 sanitized_array_index = cast_integer_verify<uint32>(array_index_signed);
	if (sanitized_array_index >= execution_graph->get_node_incoming_edge_count(array_node_index)) {
		return false;
	}

	out_sanitized_array_index = sanitized_array_index;
	return true;
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

	case k_execution_graph_node_type_indexed_input:
		// Indexed inputs can't be optimized
		break;

	case k_execution_graph_node_type_indexed_output:
		// Indexed outputs can't be optimized
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
			std::vector<s_native_module_compile_time_argument> arg_list;

			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				s_native_module_compile_time_argument compile_time_argument;
#if IS_TRUE(ASSERTS_ENABLED)
				compile_time_argument.type = argument.type;
#endif // IS_TRUE(ASSERTS_ENABLED)

				if (native_module_qualifier_is_input(argument.type.get_qualifier())) {
					uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, next_input);
					uint32 constant_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);
					c_native_module_data_type type =
						execution_graph->get_constant_node_data_type(constant_node_index);

					if (type.is_array()) {
						build_array_value(execution_graph, constant_node_index, compile_time_argument.array_value);
					} else {
						switch (type.get_primitive_type()) {
						case k_native_module_primitive_type_real:
							compile_time_argument.real_value =
								execution_graph->get_constant_node_real_value(constant_node_index);
							break;

						case k_native_module_primitive_type_bool:
							compile_time_argument.bool_value =
								execution_graph->get_constant_node_bool_value(constant_node_index);
							break;

						case k_native_module_primitive_type_string:
							compile_time_argument.string_value.get_string() =
								execution_graph->get_constant_node_string_value(constant_node_index);
							break;

						default:
							wl_unreachable();
						}
					}

					next_input++;
				} else {
					wl_assert(argument.type.get_qualifier() == k_native_module_qualifier_out);
					compile_time_argument.real_value = 0.0f;
					next_output++;
				}

				arg_list.push_back(compile_time_argument);
			}

			wl_assert(next_input == native_module.in_argument_count);
			wl_assert(next_output == native_module.out_argument_count);

			{
				// Make the compile time call to resolve the outputs
				c_native_module_compile_time_argument_list wrapped_arg_list(
					arg_list.empty() ? nullptr : &arg_list.front(), arg_list.size());
				native_module.compile_time_call(wrapped_arg_list);
			}

			next_output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				if (argument.type.get_qualifier() == k_native_module_qualifier_out) {
					const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];

					// Create a constant node for this output
					uint32 constant_node_index = c_execution_graph::k_invalid_index;
					const c_native_module_array *argument_array = nullptr;

					if (argument.type.get_data_type().is_array()) {
						constant_node_index = execution_graph->add_constant_array_node(
							argument.type.get_data_type().get_element_type());
						argument_array = &compile_time_argument.array_value;
					} else {
						switch (argument.type.get_data_type().get_primitive_type()) {
						case k_native_module_primitive_type_real:
							constant_node_index = execution_graph->add_constant_node(compile_time_argument.real_value);
							break;

						case k_native_module_primitive_type_bool:
							constant_node_index = execution_graph->add_constant_node(compile_time_argument.bool_value);
							break;

						case k_native_module_primitive_type_string:
							constant_node_index = execution_graph->add_constant_node(
								compile_time_argument.string_value.get_string());
							break;

						default:
							wl_unreachable();
						}
					}

					if (argument_array) {
						// Hook up array inputs
						for (size_t index = 0; index < argument_array->get_array().size(); index++) {
							execution_graph->add_constant_array_value(
								constant_node_index, argument_array->get_array()[index]);
						}
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
	for (uint32 rule = 0; rule < c_native_module_registry::get_optimization_rule_count(); rule++) {
		if (try_to_apply_optimization_rule(execution_graph, node_index, rule)) {
			return true;
		}
	}

	return false;
}

class c_optimization_rule_applier {
private:
	// Used to determine if the rule matches
	struct s_match_state {
		uint32 current_node_index;
		uint32 current_node_output_index;
		uint32 next_input_index;
	};

	c_execution_graph *m_execution_graph;
	uint32 m_source_root_node_index;
	const s_native_module_optimization_rule *m_rule;

	std::stack<s_match_state> m_match_state_stack;

	// Node indices for the values and constants we've matched in the source pattern
	std::array<uint32, s_native_module_optimization_symbol::k_max_matched_symbols> m_matched_variable_node_indices;
	std::array<uint32, s_native_module_optimization_symbol::k_max_matched_symbols> m_matched_constant_node_indices;

	bool has_more_inputs(const s_match_state &match_state) const {
		return match_state.next_input_index <
			m_execution_graph->get_node_incoming_edge_count(match_state.current_node_index);
	}

	uint32 follow_next_input(s_match_state &match_state, uint32 &output_node_index) {
		wl_assert(has_more_inputs(match_state));

		// Advance twice to skip the module input node
		uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
			match_state.current_node_index, match_state.next_input_index);
		uint32 new_node_index = m_execution_graph->get_node_incoming_edge_index(input_node_index, 0);

		if (m_execution_graph->get_node_type(new_node_index) == k_execution_graph_node_type_indexed_output) {
			// We need to advance an additional node for the case (module <- input <- output <- module)
			output_node_index = new_node_index;
			new_node_index = m_execution_graph->get_node_incoming_edge_index(new_node_index, 0);
		}

		match_state.next_input_index++;
		return new_node_index;
	}

	void store_match(const s_native_module_optimization_symbol &symbol, uint32 node_index) {
		wl_assert(node_index != c_execution_graph::k_invalid_index);
		if (symbol.type == k_native_module_optimization_symbol_type_variable) {
			wl_assert(m_matched_variable_node_indices[symbol.data.index] == c_execution_graph::k_invalid_index);
			m_matched_variable_node_indices[symbol.data.index] = node_index;
		} else if (symbol.type == k_native_module_optimization_symbol_type_constant) {
			wl_assert(m_matched_constant_node_indices[symbol.data.index] == c_execution_graph::k_invalid_index);
			m_matched_constant_node_indices[symbol.data.index] = node_index;
		} else {
			wl_unreachable();
		}
	}

	uint32 load_match(const s_native_module_optimization_symbol &symbol) const {
		if (symbol.type == k_native_module_optimization_symbol_type_variable) {
			wl_assert(m_matched_variable_node_indices[symbol.data.index] != c_execution_graph::k_invalid_index);
			return m_matched_variable_node_indices[symbol.data.index];
		} else if (symbol.type == k_native_module_optimization_symbol_type_constant) {
			wl_assert(m_matched_constant_node_indices[symbol.data.index] != c_execution_graph::k_invalid_index);
			return m_matched_constant_node_indices[symbol.data.index];
		} else {
			wl_unreachable();
			return c_execution_graph::k_invalid_index;
		}
	}

	bool handle_source_native_module_symbol_match(uint32 native_module_index) const {
		const s_match_state &current_state = m_match_state_stack.top();
		uint32 current_index = current_state.current_node_index;

		// It's a match if the current node is a native module of the same index
		return (m_execution_graph->get_node_type(current_index) == k_execution_graph_node_type_native_module_call) &&
			(m_execution_graph->get_native_module_call_native_module_index(current_index) == native_module_index);
	}

	bool handle_source_value_symbol_match(const s_native_module_optimization_symbol &symbol,
		uint32 node_index, uint32 node_output_index) {
		if (symbol.type == k_native_module_optimization_symbol_type_variable) {
			// Match anything except for constants
			if (m_execution_graph->get_node_type(node_index) == k_execution_graph_node_type_constant) {
				return false;
			}

			// If this is a module node, it means we had to pass through an output node to get here. Store
			// the output node as the match, because that's what inputs will be hooked up to.
			uint32 matched_node_index = (node_output_index == c_execution_graph::k_invalid_index) ?
				node_index : node_output_index;
			store_match(symbol, matched_node_index);
			return true;
		} else if (symbol.type == k_native_module_optimization_symbol_type_constant) {
			// Match only constants
			if (m_execution_graph->get_node_type(node_index) != k_execution_graph_node_type_constant) {
				return false;
			}

			store_match(symbol, node_index);
			return true;
		} else {
			// Match only constants with the given value
			if (m_execution_graph->get_node_type(node_index) != k_execution_graph_node_type_constant) {
				return false;
			}

			// get_constant_node_<type>_value will assert that there's no type mismatch
			if (symbol.type == k_native_module_optimization_symbol_type_real_value) {
				return (symbol.data.real_value == m_execution_graph->get_constant_node_real_value(node_index));
			} else if (symbol.type == k_native_module_optimization_symbol_type_bool_value) {
				return (symbol.data.bool_value == m_execution_graph->get_constant_node_bool_value(node_index));
			} else {
				wl_unreachable();
				return false;
			}
		}
	}

	bool try_to_match_source_pattern() {
		IF_ASSERTS_ENABLED(bool should_be_done = false);

		for (size_t sym = 0; sym < NUMBEROF(m_rule->source.symbols) && m_rule->source.symbols[sym].is_valid(); sym++) {
			wl_assert(!should_be_done);

			const s_native_module_optimization_symbol &symbol = m_rule->source.symbols[sym];
			switch (symbol.type) {
			case k_native_module_optimization_symbol_type_native_module:
			{
				wl_assert(symbol.data.native_module_uid != s_native_module_uid::k_invalid);
				uint32 native_module_index =
					c_native_module_registry::get_native_module_index(symbol.data.native_module_uid);

				if (m_match_state_stack.empty()) {
					// This is the beginning of the source pattern, so add the initial module call to the stack
					s_match_state initial_state;
					initial_state.current_node_output_index = c_execution_graph::k_invalid_index;
					initial_state.current_node_index = m_source_root_node_index;
					initial_state.next_input_index = 0;
					m_match_state_stack.push(initial_state);
				} else {
					// Try to advance to the next input
					s_match_state &current_state = m_match_state_stack.top();
					wl_vassert(has_more_inputs(current_state), "Rule inputs don't match native module definition");

					s_match_state new_state;
					new_state.current_node_output_index = c_execution_graph::k_invalid_index;
					new_state.current_node_index =
						follow_next_input(current_state, new_state.current_node_output_index);
					new_state.next_input_index = 0;
					m_match_state_stack.push(new_state);
				}

				// The module described by the rule should match the top of the state stack
				if (!handle_source_native_module_symbol_match(native_module_index)) {
					return false;
				}
				break;
			}

			case k_native_module_optimization_symbol_type_native_module_end:
			{
				wl_assert(!m_match_state_stack.empty());
				// We expect that if we were able to match and enter a module, we should also match when leaving
				// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
				wl_assert(!has_more_inputs(m_match_state_stack.top()));
				IF_ASSERTS_ENABLED(should_be_done = (m_match_state_stack.size() == 1));
				m_match_state_stack.pop();
				break;
			}

			case k_native_module_optimization_symbol_type_array_dereference:
				wl_vhalt("Array dereference not allowed in source pattern");
				break;

			case k_native_module_optimization_symbol_type_variable:
			case k_native_module_optimization_symbol_type_constant:
			case k_native_module_optimization_symbol_type_real_value:
			case k_native_module_optimization_symbol_type_bool_value:
			{
				// Try to advance to the next input
				wl_assert(!m_match_state_stack.empty());
				s_match_state &current_state = m_match_state_stack.top();
				wl_vassert(has_more_inputs(current_state), "Rule inputs don't match native module definition");

				uint32 new_node_output_index = c_execution_graph::k_invalid_index;
				uint32 new_node_index = follow_next_input(current_state, new_node_output_index);

				if (!handle_source_value_symbol_match(symbol, new_node_index, new_node_output_index)) {
					return false;
				}
				break;
			}

			default:
				wl_unreachable();
				return false;
			}
		}

		wl_assert(m_match_state_stack.empty());
		return true;
	}

	uint32 handle_target_array_dereference_symbol_match(
		const s_native_module_optimization_symbol &array_symbol,
		const s_native_module_optimization_symbol &index_symbol) {
		// Validate the array and index symbols
		wl_assert(array_symbol.is_valid());
		wl_assert(array_symbol.type == k_native_module_optimization_symbol_type_constant);
		uint32 array_node = load_match(array_symbol);
		wl_assert(m_execution_graph->get_constant_node_data_type(array_node).is_array());

		wl_assert(index_symbol.is_valid());
		wl_assert(index_symbol.type == k_native_module_optimization_symbol_type_constant);
		uint32 index_node = load_match(index_symbol);
		wl_assert(m_execution_graph->get_constant_node_data_type(index_node) ==
			c_native_module_data_type(k_native_module_primitive_type_real));

		// Obtain the array index
		uint32 array_index = 0;
		bool valid_array_index = sanitize_array_index(
			m_execution_graph,
			array_node,
			m_execution_graph->get_constant_node_real_value(index_node),
			array_index);

		// If array index is out of bounds, we fall back to a default value
		uint32 dereferenced_node_index = c_execution_graph::k_invalid_index;
		if (!valid_array_index) {
			switch (m_execution_graph->get_constant_node_data_type(array_node).get_primitive_type()) {
			case k_native_module_primitive_type_real:
				dereferenced_node_index = m_execution_graph->add_constant_node(0.0f);
				break;

			case k_native_module_primitive_type_bool:
				dereferenced_node_index = m_execution_graph->add_constant_node(false);
				break;

			case k_native_module_primitive_type_string:
				dereferenced_node_index = m_execution_graph->add_constant_node("");
				break;

			default:
				wl_unreachable();
			}
		} else {
			// Follow the incoming index specified from the array node
			uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(array_node, array_index);
			dereferenced_node_index = m_execution_graph->get_node_incoming_edge_index(input_node_index, 0);
		}

		return dereferenced_node_index;
	}

	uint32 handle_target_value_symbol_match(const s_native_module_optimization_symbol &symbol) {
		if (symbol.type == k_native_module_optimization_symbol_type_variable ||
			symbol.type == k_native_module_optimization_symbol_type_constant) {
			return load_match(symbol);
		} else if (symbol.type == k_native_module_optimization_symbol_type_real_value) {
			// Create a constant node with this value
			return m_execution_graph->add_constant_node(symbol.data.real_value);
		} else if (symbol.type == k_native_module_optimization_symbol_type_bool_value) {
			// Create a constant node with this value
			return m_execution_graph->add_constant_node(symbol.data.bool_value);
		} else {
			wl_unreachable();
			return c_execution_graph::k_invalid_index;
		}
	}

	uint32 build_target_pattern() {
		// Replaced the matched source pattern nodes with the ones defined by the target in the rule
		uint32 root_node_index = c_execution_graph::k_invalid_index;

		IF_ASSERTS_ENABLED(bool should_be_done = false);

		for (size_t sym = 0; sym < NUMBEROF(m_rule->target.symbols) && m_rule->target.symbols[sym].is_valid(); sym++) {
			wl_assert(!should_be_done);

			const s_native_module_optimization_symbol &symbol = m_rule->target.symbols[sym];
			switch (symbol.type) {
			case k_native_module_optimization_symbol_type_native_module:
			{
				uint32 native_module_index =
					c_native_module_registry::get_native_module_index(symbol.data.native_module_uid);
				uint32 node_index = m_execution_graph->add_native_module_call_node(native_module_index);
				// We currently only allow a single outgoing edge, due to the way we express rules
				wl_assert(m_execution_graph->get_node_outgoing_edge_count(node_index) == 1);

				if (root_node_index == c_execution_graph::k_invalid_index) {
					// This is the root node of the target pattern
					wl_assert(m_match_state_stack.empty());
					root_node_index = node_index;
				} else {
					s_match_state &current_state = m_match_state_stack.top();
					wl_assert(has_more_inputs(current_state));

					uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
						current_state.current_node_index, current_state.next_input_index);
					uint32 output_node_index = m_execution_graph->get_node_outgoing_edge_index(node_index, 0);
					m_execution_graph->add_edge(output_node_index, input_node_index);
					current_state.next_input_index++;
				}

				s_match_state new_state;
				new_state.current_node_output_index = c_execution_graph::k_invalid_index;
				new_state.current_node_index = node_index;
				new_state.next_input_index = 0;
				m_match_state_stack.push(new_state);
				break;
			}

			case k_native_module_optimization_symbol_type_native_module_end:
			{
				wl_assert(!m_match_state_stack.empty());
				// We expect that if we were able to match and enter a module, we should also match when leaving
				// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
				wl_assert(!has_more_inputs(m_match_state_stack.top()));
				IF_ASSERTS_ENABLED(should_be_done = m_match_state_stack.size() == 1);
				m_match_state_stack.pop();
				break;
			}

			case k_native_module_optimization_symbol_type_array_dereference:
			case k_native_module_optimization_symbol_type_variable:
			case k_native_module_optimization_symbol_type_constant:
			case k_native_module_optimization_symbol_type_real_value:
			case k_native_module_optimization_symbol_type_bool_value:
			{
				uint32 matched_node_index;
				if (symbol.type == k_native_module_optimization_symbol_type_array_dereference) {
					// Read the next two symbols - must be constant array and constant index
					wl_assert(sym + 2 <= NUMBEROF(m_rule->target.symbols));
					sym++;
					const s_native_module_optimization_symbol &array_symbol = m_rule->target.symbols[sym];
					sym++;
					const s_native_module_optimization_symbol &index_symbol = m_rule->target.symbols[sym];

					matched_node_index = handle_target_array_dereference_symbol_match(array_symbol, index_symbol);
				} else {
					matched_node_index = handle_target_value_symbol_match(symbol);
				}

				// Hook up this input and advance
				if (root_node_index == c_execution_graph::k_invalid_index) {
					wl_assert(m_match_state_stack.empty());
					root_node_index = matched_node_index;
					IF_ASSERTS_ENABLED(should_be_done = true);
				} else {
					wl_assert(!m_match_state_stack.empty());
					s_match_state &current_state = m_match_state_stack.top();
					wl_assert(has_more_inputs(current_state));

					uint32 input_node_index = m_execution_graph->get_node_incoming_edge_index(
						current_state.current_node_index, current_state.next_input_index);
					current_state.next_input_index++;
					m_execution_graph->add_edge(matched_node_index, input_node_index);
				}
				break;
			}

			default:
				wl_unreachable();
			}
		}

		wl_assert(m_match_state_stack.empty());
		return root_node_index;
	}

	void reroute_source_to_target(uint32 target_root_node_index) {
		// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
		// deleting the old set of nodes, they will automatically be cleaned up later.

		// We currently only support modules with a single output
		wl_assert(m_execution_graph->get_node_outgoing_edge_count(m_source_root_node_index) == 1);
		uint32 old_output_node = m_execution_graph->get_node_outgoing_edge_index(m_source_root_node_index, 0);

		switch (m_execution_graph->get_node_type(target_root_node_index)) {
		case k_execution_graph_node_type_native_module_call:
		{
			wl_assert(m_execution_graph->get_node_outgoing_edge_count(target_root_node_index) == 1);
			uint32 new_output_node = m_execution_graph->get_node_outgoing_edge_index(target_root_node_index, 0);
			transfer_outputs(m_execution_graph, new_output_node, old_output_node);
			break;
		}

		case k_execution_graph_node_type_constant:
		case k_execution_graph_node_type_indexed_output:
			transfer_outputs(m_execution_graph, target_root_node_index, old_output_node);
			break;

		default:
			wl_unreachable();
		}
	}

public:
	c_optimization_rule_applier(c_execution_graph *execution_graph, uint32 source_root_node_index, uint32 rule_index) {
		m_execution_graph = execution_graph;
		m_source_root_node_index = source_root_node_index;
		m_rule = &c_native_module_registry::get_optimization_rule(rule_index);
	}

	bool try_to_apply_optimization_rule() {
		wl_assert(m_rule);

		// Initially we haven't matched any values
		std::fill(m_matched_variable_node_indices.begin(), m_matched_variable_node_indices.end(),
			c_execution_graph::k_invalid_index);
		std::fill(m_matched_constant_node_indices.begin(), m_matched_constant_node_indices.end(),
			c_execution_graph::k_invalid_index);

		if (try_to_match_source_pattern()) {
			uint32 target_root_node_index = build_target_pattern();
			reroute_source_to_target(target_root_node_index);
			return true;
		}

		return false;
	}
};

static bool try_to_apply_optimization_rule(c_execution_graph *execution_graph, uint32 node_index, uint32 rule_index) {
	c_optimization_rule_applier optimization_rule_applier(execution_graph, node_index, rule_index);
	return optimization_rule_applier.try_to_apply_optimization_rule();
}

void remove_useless_nodes(c_execution_graph *execution_graph) {
	std::stack<uint32> node_stack;
	std::vector<bool> nodes_visited(execution_graph->get_node_count(), false);

	// Start at the output nodes and work backwards, marking each node as it is encountered
	for (uint32 index = 0; index < execution_graph->get_node_count(); index++) {
		e_execution_graph_node_type type = execution_graph->get_node_type(index);
		if (type == k_execution_graph_node_type_output) {
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
			// Don't directly remove inputs/outputs, since they automatically get removed along with their nodes. Also
			// avoid removing invalid nodes, because they're already removed.
			e_execution_graph_node_type type = execution_graph->get_node_type(index);
			if (type != k_execution_graph_node_type_invalid &&
				type != k_execution_graph_node_type_indexed_input &&
				type != k_execution_graph_node_type_indexed_output) {
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
	// 2) combine any native modules and arrays with the same type and inputs
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
			c_native_module_data_type type = execution_graph->get_constant_node_data_type(node_a_index);
			if (type != execution_graph->get_constant_node_data_type(node_b_index)) {
				continue;
			}

			bool equal = false;

			if (type.is_array()) {
				// Arrays are deduplicated later
			} else {
				switch (type.get_primitive_type()) {
				case k_native_module_primitive_type_real:
					equal = (execution_graph->get_constant_node_real_value(node_a_index) ==
						execution_graph->get_constant_node_real_value(node_b_index));
					break;

				case k_native_module_primitive_type_bool:
					equal = (execution_graph->get_constant_node_bool_value(node_a_index) ==
						execution_graph->get_constant_node_bool_value(node_b_index));
					break;

				case k_native_module_primitive_type_string:
					equal = (strcmp(execution_graph->get_constant_node_string_value(node_a_index),
						execution_graph->get_constant_node_string_value(node_b_index)) == 0);
					break;

				default:
					wl_unreachable();
				}
			}

			if (equal) {
				// Redirect node B's outputs as outputs of node A
				while (execution_graph->get_node_outgoing_edge_count(node_b_index) > 0) {
					uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(node_b_index,
						execution_graph->get_node_outgoing_edge_count(node_b_index) - 1);
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

		// Find any module calls or arrays with identical type and inputs and merge them. (The common factor here is
		// nodes which use indexed inputs.)
		// Currently n^2 but we could do better easily.
		for (uint32 node_a_index = 0; node_a_index < execution_graph->get_node_count(); node_a_index++) {
			if (execution_graph->get_node_type(node_a_index) == k_execution_graph_node_type_invalid) {
				continue;
			}

			if (!execution_graph->does_node_use_indexed_inputs(node_a_index)) {
				continue;
			}

			for (uint32 node_b_index = node_a_index + 1;
				 node_b_index < execution_graph->get_node_count();
				 node_b_index++) {
				if (execution_graph->get_node_type(node_a_index) != execution_graph->get_node_type(node_b_index)) {
					continue;
				}

				e_execution_graph_node_type node_type = execution_graph->get_node_type(node_a_index);
				if (node_type == k_execution_graph_node_type_native_module_call) {
					// If native module indices don't match, skip
					if (execution_graph->get_native_module_call_native_module_index(node_a_index) !=
						execution_graph->get_native_module_call_native_module_index(node_b_index)) {
						continue;
					}
				} else if (node_type == k_execution_graph_node_type_constant) {
					wl_assert(execution_graph->get_constant_node_data_type(node_a_index).is_array());

					// If array types don't match, skip
					if (execution_graph->get_constant_node_data_type(node_a_index) !=
						execution_graph->get_constant_node_data_type(node_b_index)) {
						continue;
					}

					wl_assert(execution_graph->get_constant_node_data_type(node_b_index).is_array());

					// If array sizes don't match, skip
					if (execution_graph->get_node_incoming_edge_count(node_a_index) !=
						execution_graph->get_node_incoming_edge_count(node_b_index)) {
						continue;
					}
				} else {
					// What other node types use indexed inputs?
					wl_unreachable();
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
					if (execution_graph->does_node_use_indexed_outputs(node_a_index)) {
						// Remap each indexed output
						for (size_t edge = 0;
							 edge < execution_graph->get_node_outgoing_edge_count(node_a_index);
							 edge++) {
							uint32 output_node_a = execution_graph->get_node_outgoing_edge_index(node_a_index, edge);
							uint32 output_node_b = execution_graph->get_node_outgoing_edge_index(node_b_index, edge);

							while (execution_graph->get_node_outgoing_edge_count(output_node_b) > 0) {
								uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(output_node_b,
									execution_graph->get_node_outgoing_edge_count(output_node_b) - 1);
								execution_graph->remove_edge(output_node_b, to_node_index);
								execution_graph->add_edge(output_node_a, to_node_index);
							}
						}
					} else {
						// Directly remap outputs
						while (execution_graph->get_node_outgoing_edge_count(node_b_index) > 0) {
							uint32 to_node_index = execution_graph->get_node_outgoing_edge_index(node_b_index,
								execution_graph->get_node_outgoing_edge_count(node_b_index) - 1);
							execution_graph->remove_edge(node_b_index, to_node_index);
							execution_graph->add_edge(node_a_index, to_node_index);
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
			e_native_module_qualifier qualifier = native_module.arguments[arg].type.get_qualifier();
			if (qualifier == k_native_module_qualifier_in) {
				input++;
			} else if (qualifier == k_native_module_qualifier_constant) {
				// Validate that this input is constant
				uint32 input_node_index = execution_graph->get_node_incoming_edge_index(node_index, input);
				uint32 constant_node_index = execution_graph->get_node_incoming_edge_index(input_node_index, 0);

				if (execution_graph->get_node_type(constant_node_index) != k_execution_graph_node_type_constant) {
					s_compiler_result error;
					error.result = k_compiler_result_constant_expected;
					error.source_location.clear();
					error.message = "Input argument to native module call '" +
						std::string(native_module.name.get_string()) + "' does not resolve to a constant";
					out_errors.push_back(error);
				}

				input++;
			}
		}

		wl_assert(input == native_module.in_argument_count);
	}
}
