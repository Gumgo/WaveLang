#include "compiler/execution_graph_optimizer.h"

#include "execution_graph/diagnostic/diagnostic.h"
#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/node_interface/node_interface.h"

#include <array>
#include <set>

struct s_native_module_diagnostic_callback_context {
	std::vector<s_compiler_result> *errors;
	const s_native_module *native_module;
};

static void build_array_value(
	const c_execution_graph *execution_graph,
	c_node_reference array_node_reference,
	c_native_module_array &array_value_out);
static bool sanitize_array_index(
	const c_execution_graph *execution_graph,
	c_node_reference array_node_reference,
	real32 array_index,
	uint32 &sanitized_array_index_out);

static void native_module_diagnostic_callback(
	void *context,
	e_diagnostic_level diagnostic_level,
	const std::string &message);
static void execute_compile_time_call(
	c_wrapped_array<void *> native_module_library_contexts,
	const c_execution_graph *execution_graph,
	c_node_interface *node_interface,
	const s_instrument_globals *instrument_globals,
	const s_native_module &native_module,
	c_native_module_compile_time_argument_list arguments,
	std::vector<s_compiler_result> *errors);

static bool optimize_node(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	c_node_reference node_reference,
	std::vector<s_compiler_result> *errors);
static bool optimize_native_module_call(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	c_node_reference node_reference,
	std::vector<s_compiler_result> *errors);
static bool try_to_apply_optimization_rule(
	c_execution_graph *execution_graph,
	c_node_reference node_reference,
	uint32 rule_index);
static void remove_useless_nodes(c_execution_graph *execution_graph);
static void transfer_outputs(
	c_execution_graph *execution_graph,
	c_node_reference destination_reference,
	c_node_reference source_reference);
static void deduplicate_nodes(c_execution_graph *execution_graph);
static void validate_optimized_constants(
	const c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &errors_out);

c_execution_graph_constant_evaluator::c_execution_graph_constant_evaluator() {
	m_execution_graph = nullptr;
	m_instrument_globals = nullptr;
}

void c_execution_graph_constant_evaluator::initialize(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	std::vector<s_compiler_result> *errors) {
	wl_assert(!m_execution_graph);
	wl_assert(!m_instrument_globals);
	m_native_module_library_contexts = native_module_library_contexts;
	m_execution_graph = execution_graph;
	m_instrument_globals = instrument_globals;
	m_errors = errors;
	m_invalid_constant = true;
}

bool c_execution_graph_constant_evaluator::evaluate_constant(c_node_reference node_reference) {
	wl_assert(m_execution_graph);
	m_invalid_constant = false;
	m_pending_nodes.push(node_reference);

	while (!m_invalid_constant && !m_pending_nodes.empty()) {
		c_node_reference next_node_reference = m_pending_nodes.top();
		wl_assert(m_execution_graph->get_node_type(next_node_reference) == e_execution_graph_node_type::k_constant
			|| m_execution_graph->get_node_type(next_node_reference) == e_execution_graph_node_type::k_indexed_output);

		// Try to look up the result of this node's evaluation
		auto it = m_results.find(next_node_reference);
		if (it == m_results.end()) {
			try_evaluate_node(next_node_reference);
		} else {
			// We have already evaluated the result
			if (next_node_reference == node_reference) {
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

void c_execution_graph_constant_evaluator::remove_cached_result(c_node_reference node_reference) {
	auto it = m_results.find(node_reference);
	if (it != m_results.end()) {
		m_results.erase(it);
	}
}

void c_execution_graph_constant_evaluator::try_evaluate_node(c_node_reference node_reference) {
	// Attempt to evaluate this node
	e_execution_graph_node_type node_type = m_execution_graph->get_node_type(node_reference);
	if (node_type == e_execution_graph_node_type::k_constant) {
		// Easy - the node is simply a constant value already
		s_result result;
		result.type = m_execution_graph->get_constant_node_data_type(node_reference);

		if (result.type.is_array()) {
			build_array_value(m_execution_graph, node_reference, result.array_value);
		} else {
			switch (result.type.get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				result.real_value = m_execution_graph->get_constant_node_real_value(node_reference);
				break;

			case e_native_module_primitive_type::k_bool:
				result.bool_value = m_execution_graph->get_constant_node_bool_value(node_reference);
				break;

			case e_native_module_primitive_type::k_string:
				result.string_value.get_string() = m_execution_graph->get_constant_node_string_value(node_reference);
				break;

			default:
				wl_unreachable();
			}
		}

		m_results.insert(std::make_pair(node_reference, result));
	} else if (node_type == e_execution_graph_node_type::k_indexed_output) {
		// This is the output to a native module call
		c_node_reference native_module_node_reference =
			m_execution_graph->get_node_incoming_edge_reference(node_reference, 0);
		wl_assert(m_execution_graph->get_node_type(native_module_node_reference) ==
			e_execution_graph_node_type::k_native_module_call);

		// First check if it can even be evaluated at compile-time
		uint32 native_module_index =
			m_execution_graph->get_native_module_call_native_module_index(native_module_node_reference);
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

		bool all_inputs_evaluated = build_module_call_arguments(native_module, native_module_node_reference, arg_list);

		// Make the compile time call to resolve the outputs
		if (all_inputs_evaluated) {
			c_node_interface node_interface(m_execution_graph);

			execute_compile_time_call(
				m_native_module_library_contexts,
				m_execution_graph,
				&node_interface,
				m_instrument_globals,
				native_module,
				c_native_module_compile_time_argument_list(
					arg_list.empty() ? nullptr : &arg_list.front(),
					arg_list.size()),
				m_errors);
			store_native_module_call_results(native_module, native_module_node_reference, arg_list);

			// $TODO BUG: Fix this! We can't call this here because the nodes aren't actually in use yet. This whole
			// caching system is kind of janky and should probably be removed or rethought.
			// Remove any newly created but unused constant nodes
			//for (c_node_reference constant_node_reference : node_interface.get_created_node_references()) {
			//	if (m_execution_graph->get_node_outgoing_edge_count(constant_node_reference) == 0) {
			//		m_execution_graph->remove_node(constant_node_reference);
			//	}
			//}
		}
	} else {
		// We should only ever add constants and native module output nodes
		wl_unreachable();
	}
}

bool c_execution_graph_constant_evaluator::build_module_call_arguments(
	const s_native_module &native_module,
	c_node_reference native_module_node_reference,
	std::vector<s_native_module_compile_time_argument> &arg_list_out) {
	wl_assert(arg_list_out.empty());
	arg_list_out.resize(native_module.argument_count);

	// Perform the native module call to resolve the constant value
	size_t next_input = 0;
	size_t next_output = 0;

	bool all_inputs_evaluated = true;
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		s_native_module_compile_time_argument &compile_time_argument = arg_list_out[arg];
#if IS_TRUE(ASSERTS_ENABLED)
		compile_time_argument.type = argument.type;
#endif // IS_TRUE(ASSERTS_ENABLED)

		if (native_module_qualifier_is_input(argument.type.get_qualifier())) {
			c_node_reference input_node_reference = m_execution_graph->get_node_incoming_edge_reference(
				native_module_node_reference, next_input);
			c_node_reference source_node_reference = m_execution_graph->get_node_incoming_edge_reference(
				input_node_reference, 0);

			auto it = m_results.find(source_node_reference);
			if (it == m_results.end()) {
				// Not yet evaluated - push this node to the stack. It may already be there, but that's okay, it
				// will be skipped if it's encountered and already evaluated.
				m_pending_nodes.push(source_node_reference);
				all_inputs_evaluated = false;
			} else if (all_inputs_evaluated) {
				// Don't bother adding any more arguments if we've failed to evaluate an input, but still loop
				// so that we can add unevaluated nodes to the stack
				const s_result &input_result = it->second;

				if (input_result.type.is_array()) {
					compile_time_argument.array_value = input_result.array_value;
				} else {
					switch (input_result.type.get_primitive_type()) {
					case e_native_module_primitive_type::k_real:
						compile_time_argument.real_value = input_result.real_value;
						break;

					case e_native_module_primitive_type::k_bool:
						compile_time_argument.bool_value = input_result.bool_value;
						break;

					case e_native_module_primitive_type::k_string:
						compile_time_argument.string_value = input_result.string_value;
						break;

					default:
						wl_unreachable();
					}
				}
			}

			next_input++;
		} else {
			wl_assert(argument.type.get_qualifier() == e_native_module_qualifier::k_out);
			compile_time_argument.real_value = 0.0f;
			next_output++;
		}
	}

	wl_assert(next_input == native_module.in_argument_count);
	wl_assert(next_output == native_module.out_argument_count);

	return all_inputs_evaluated;
}

void c_execution_graph_constant_evaluator::store_native_module_call_results(
	const s_native_module &native_module,
	c_node_reference native_module_node_reference,
	const std::vector<s_native_module_compile_time_argument> &arg_list) {
	size_t next_output = 0;

	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		if (argument.type.get_qualifier() == e_native_module_qualifier::k_out) {
			const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];

			// Store the output result
			s_result evaluation_result;
			evaluation_result.type = argument.type.get_data_type();

			if (argument.type.get_data_type().is_array()) {
				evaluation_result.array_value = compile_time_argument.array_value;
			} else {
				switch (argument.type.get_data_type().get_primitive_type()) {
				case e_native_module_primitive_type::k_real:
					evaluation_result.real_value = compile_time_argument.real_value;
					break;

				case e_native_module_primitive_type::k_bool:
					evaluation_result.bool_value = compile_time_argument.bool_value;
					break;

				case e_native_module_primitive_type::k_string:
					evaluation_result.string_value = compile_time_argument.string_value;
					break;
				default:
					wl_unreachable();
				}
			}

			c_node_reference output_node_reference = m_execution_graph->get_node_outgoing_edge_reference(
				native_module_node_reference, next_output);
			m_results.insert(std::make_pair(output_node_reference, evaluation_result));

			next_output++;
		}
	}

	wl_assert(next_output == native_module.out_argument_count);
}

c_execution_graph_trimmer::c_execution_graph_trimmer() {
	m_execution_graph = nullptr;
}

void c_execution_graph_trimmer::initialize(
	c_execution_graph *execution_graph,
	f_on_node_removed on_node_removed,
	void *on_node_removed_context) {
	m_execution_graph = execution_graph;
	m_on_node_removed = on_node_removed;
	m_on_node_removed_context = on_node_removed_context;
}

void c_execution_graph_trimmer::try_trim_node(c_node_reference node_reference) {
	{
		e_execution_graph_node_type node_type = m_execution_graph->get_node_type(node_reference);
		if (node_type == e_execution_graph_node_type::k_indexed_output) {
			// Jump to the source node
			node_reference = m_execution_graph->get_node_incoming_edge_reference(node_reference, 0);
		}
	}

	m_pending_nodes.push(node_reference);

	while (!m_pending_nodes.empty()) {
		c_node_reference node_reference = m_pending_nodes.top();
		m_pending_nodes.pop();

		// Check if this node has any outputs
		bool has_any_outputs = false;
		switch (m_execution_graph->get_node_type(node_reference)) {
		case e_execution_graph_node_type::k_constant:
			has_any_outputs = m_execution_graph->get_node_outgoing_edge_count(node_reference) > 0;
			break;

		case e_execution_graph_node_type::k_native_module_call:
			for (size_t edge_index = 0;
				!has_any_outputs && edge_index < m_execution_graph->get_node_outgoing_edge_count(node_reference);
				edge_index++) {
				c_node_reference out_node_reference =
					m_execution_graph->get_node_outgoing_edge_reference(node_reference, edge_index);
				has_any_outputs |= m_execution_graph->get_node_outgoing_edge_count(out_node_reference) > 0;
			}
			break;

		case e_execution_graph_node_type::k_indexed_input:
		case e_execution_graph_node_type::k_indexed_output:
			// Should not encounter these node types
			wl_unreachable();
			break;

		case e_execution_graph_node_type::k_input:
			// Never remove these nodes, even if they're unused
			has_any_outputs = true;
			break;

		case e_execution_graph_node_type::k_output:
		case e_execution_graph_node_type::k_temporary_reference:
			// Should not encounter these node types
			wl_unreachable();
			break;

		default:
			wl_unreachable();
		}

		if (!has_any_outputs) {
			// Remove this node and recursively check its inputs
			switch (m_execution_graph->get_node_type(node_reference)) {
			case e_execution_graph_node_type::k_constant:
				if (m_execution_graph->get_constant_node_data_type(node_reference).is_array()) {
					for (size_t edge_index = 0;
						edge_index < m_execution_graph->get_node_incoming_edge_count(node_reference);
						edge_index++) {
						c_node_reference in_node_reference =
							m_execution_graph->get_node_incoming_edge_reference(node_reference, edge_index);
						c_node_reference source_node_reference =
							m_execution_graph->get_node_incoming_edge_reference(in_node_reference, 0);
						add_pending_node(source_node_reference);
					}
				}
				break;

			case e_execution_graph_node_type::k_native_module_call:
				for (size_t edge_index = 0;
					edge_index < m_execution_graph->get_node_incoming_edge_count(node_reference);
					edge_index++) {
					c_node_reference in_node_reference =
						m_execution_graph->get_node_incoming_edge_reference(node_reference, edge_index);
					for (size_t in_edge_index = 0;
						in_edge_index < m_execution_graph->get_node_incoming_edge_count(in_node_reference);
						in_edge_index++) {
						c_node_reference source_node_reference =
							m_execution_graph->get_node_incoming_edge_reference(in_node_reference, in_edge_index);
						add_pending_node(source_node_reference);
					}
				}
				break;

			case e_execution_graph_node_type::k_indexed_input:
			case e_execution_graph_node_type::k_indexed_output:
				// Should not encounter these node types
				wl_unreachable();
				break;

			case e_execution_graph_node_type::k_input:
				break;

			case e_execution_graph_node_type::k_output:
			case e_execution_graph_node_type::k_temporary_reference:
				// Should not encounter these node types
				wl_unreachable();
				break;

			default:
				wl_unreachable();
			}

			m_execution_graph->remove_node(node_reference, m_on_node_removed, m_on_node_removed_context);
		}
	}

	m_visited_nodes.clear();
}

void c_execution_graph_trimmer::add_pending_node(c_node_reference node_reference) {
	if (m_execution_graph->get_node_type(node_reference) == e_execution_graph_node_type::k_indexed_output) {
		// Jump to the source node
		node_reference = m_execution_graph->get_node_incoming_edge_reference(node_reference, 0);
	}

	if (m_visited_nodes.find(node_reference) == m_visited_nodes.end()) {
		m_pending_nodes.push(node_reference);
		m_visited_nodes.insert(node_reference);
	}
}

s_compiler_result c_execution_graph_optimizer::optimize_graph(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	std::vector<s_compiler_result> &errors_out) {
	s_compiler_result result;
	result.clear();

	remove_useless_nodes(execution_graph);

	// Keep making passes over the graph until no more optimizations can be applied
	bool optimization_performed;
	do {
		optimization_performed = false;

		for (c_node_reference node_reference = execution_graph->nodes_begin();
			node_reference.is_valid();
			node_reference = execution_graph->nodes_next(node_reference)) {
			optimization_performed |= optimize_node(
				native_module_library_contexts,
				execution_graph,
				instrument_globals,
				node_reference,
				&errors_out);
		}

		remove_useless_nodes(execution_graph);
	} while (optimization_performed);

	execution_graph->remove_unused_nodes_and_reassign_node_indices();

	deduplicate_nodes(execution_graph);
	execution_graph->remove_unused_nodes_and_reassign_node_indices();

	validate_optimized_constants(execution_graph, errors_out);
	if (!errors_out.empty()) {
		result.result = e_compiler_result::k_graph_error;
		result.message = "Graph error(s) detected";
	}

	return result;
}

static void build_array_value(
	const c_execution_graph *execution_graph,
	c_node_reference array_node_reference,
	c_native_module_array &array_value_out) {
	wl_assert(execution_graph->get_node_type(array_node_reference) == e_execution_graph_node_type::k_constant);
	wl_assert(execution_graph->get_constant_node_data_type(array_node_reference).is_array());
	wl_assert(array_value_out.get_array().empty());

	size_t array_count = execution_graph->get_node_incoming_edge_count(array_node_reference);
	array_value_out.get_array().reserve(array_count);

	for (size_t array_index = 0; array_index < array_count; array_index++) {
		// Jump past the indexed input node
		c_node_reference input_node_reference =
			execution_graph->get_node_incoming_edge_reference(array_node_reference, array_index);
		c_node_reference value_node_reference =
			execution_graph->get_node_incoming_edge_reference(input_node_reference, 0);
		array_value_out.get_array().push_back(value_node_reference);
	}
}

static bool sanitize_array_index(
	const c_execution_graph *execution_graph,
	c_node_reference array_node_reference,
	real32 array_index,
	uint32 &sanitized_array_index_out) {
	if (std::isnan(array_index) || std::isinf(array_index)) {
		return false;
	}

	// Casting to int will floor
	int32 array_index_signed = static_cast<int32>(std::floor(array_index));
	if (array_index_signed < 0) {
		return false;
	}

	uint32 sanitized_array_index = cast_integer_verify<uint32>(array_index_signed);
	if (sanitized_array_index >= execution_graph->get_node_incoming_edge_count(array_node_reference)) {
		return false;
	}

	sanitized_array_index_out = sanitized_array_index;
	return true;
}

static void native_module_diagnostic_callback(
	void *context,
	e_diagnostic_level diagnostic_level,
	const std::string &message) {
	s_native_module_diagnostic_callback_context *native_module_diagnostic_callback_context =
		static_cast<s_native_module_diagnostic_callback_context *>(context);
	wl_assert(native_module_diagnostic_callback_context);

	if (diagnostic_level == e_diagnostic_level::k_error) {
		s_compiler_result error;
		error.result = e_compiler_result::k_native_module_compile_time_call;
		error.source_location.clear();
		error.message = "Native module '" +
			std::string(native_module_diagnostic_callback_context->native_module->name.get_string()) + "' call: " +
			message;
		native_module_diagnostic_callback_context->errors->push_back(error);
	} else {
		// $TODO $DIAGNOSTIC report messages and warnings
	}
}

static void execute_compile_time_call(
	c_wrapped_array<void *> native_module_library_contexts,
	const c_execution_graph *execution_graph,
	c_node_interface *node_interface,
	const s_instrument_globals *instrument_globals,
	const s_native_module &native_module,
	c_native_module_compile_time_argument_list arguments,
	std::vector<s_compiler_result> *errors) {
	wl_assert(execution_graph);

	// Make the compile time call to resolve the outputs
	s_native_module_context native_module_context;
	zero_type(&native_module_context);

	s_native_module_diagnostic_callback_context native_module_diagnostic_callback_context;
	native_module_diagnostic_callback_context.errors = errors;
	native_module_diagnostic_callback_context.native_module = &native_module;

	c_diagnostic diagnostic(native_module_diagnostic_callback, &native_module_diagnostic_callback_context);

	native_module_context.diagnostic = &diagnostic;
	native_module_context.node_interface = node_interface;
	native_module_context.instrument_globals = instrument_globals;

	uint32 library_index =
		c_native_module_registry::get_native_module_library_index(native_module.uid.get_library_id());
	native_module_context.library_context = native_module_library_contexts[library_index];

	native_module_context.arguments = &arguments;

	native_module.compile_time_call(native_module_context);
}

static bool optimize_node(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	c_node_reference node_reference,
	std::vector<s_compiler_result> *errors) {
	bool optimized = false;

	switch (execution_graph->get_node_type(node_reference)) {
	case e_execution_graph_node_type::k_invalid:
		// This node was removed, skip it
		break;

	case e_execution_graph_node_type::k_constant:
		// We can't directly optimize constant nodes
		break;

	case e_execution_graph_node_type::k_native_module_call:
		optimized = optimize_native_module_call(
			native_module_library_contexts,
			execution_graph,
			instrument_globals,
			node_reference,
			errors);
		break;

	case e_execution_graph_node_type::k_indexed_input:
		// Indexed inputs can't be optimized
		break;

	case e_execution_graph_node_type::k_indexed_output:
		// Indexed outputs can't be optimized
		break;

	case e_execution_graph_node_type::k_input:
		// Inputs can't be optimized
		break;

	case e_execution_graph_node_type::k_output:
		// Outputs can't be optimized
		break;

	case e_execution_graph_node_type::k_temporary_reference:
		// These should have all been removed
		wl_unreachable();
		break;

	default:
		wl_unreachable();
	}

	return optimized;
}

static bool optimize_native_module_call(
	c_wrapped_array<void *> native_module_library_contexts,
	c_execution_graph *execution_graph,
	const s_instrument_globals *instrument_globals,
	c_node_reference node_reference,
	std::vector<s_compiler_result> *errors) {
	wl_assert(execution_graph->get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);
	const s_native_module &native_module = c_native_module_registry::get_native_module(
		execution_graph->get_native_module_call_native_module_index(node_reference));

	// Determine if all inputs are constants
	if (native_module.compile_time_call) {
		bool all_constant_inputs = true;
		size_t input_count = execution_graph->get_node_incoming_edge_count(node_reference);
		for (size_t edge = 0; all_constant_inputs && edge < input_count; edge++) {
			c_node_reference input_node_reference =
				execution_graph->get_node_incoming_edge_reference(node_reference, edge);
			c_node_reference input_type_node_reference =
				execution_graph->get_node_incoming_edge_reference(input_node_reference, 0);
			if (execution_graph->get_node_type(input_type_node_reference) != e_execution_graph_node_type::k_constant) {
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
					c_node_reference input_node_reference =
						execution_graph->get_node_incoming_edge_reference(node_reference, next_input);
					c_node_reference constant_node_reference =
						execution_graph->get_node_incoming_edge_reference(input_node_reference, 0);
					c_native_module_data_type type =
						execution_graph->get_constant_node_data_type(constant_node_reference);

					if (type.is_array()) {
						build_array_value(execution_graph, constant_node_reference, compile_time_argument.array_value);
					} else {
						switch (type.get_primitive_type()) {
						case e_native_module_primitive_type::k_real:
							compile_time_argument.real_value =
								execution_graph->get_constant_node_real_value(constant_node_reference);
							break;

						case e_native_module_primitive_type::k_bool:
							compile_time_argument.bool_value =
								execution_graph->get_constant_node_bool_value(constant_node_reference);
							break;

						case e_native_module_primitive_type::k_string:
							compile_time_argument.string_value.get_string() =
								execution_graph->get_constant_node_string_value(constant_node_reference);
							break;

						default:
							wl_unreachable();
						}
					}

					next_input++;
				} else {
					wl_assert(argument.type.get_qualifier() == e_native_module_qualifier::k_out);
					compile_time_argument.real_value = 0.0f;
					next_output++;
				}

				arg_list.push_back(compile_time_argument);
			}

			wl_assert(next_input == native_module.in_argument_count);
			wl_assert(next_output == native_module.out_argument_count);

			// Make the compile time call to resolve the outputs
			c_node_interface node_interface(execution_graph);
			execute_compile_time_call(
				native_module_library_contexts,
				execution_graph,
				&node_interface,
				instrument_globals,
				native_module,
				c_native_module_compile_time_argument_list(
					arg_list.empty() ? nullptr : &arg_list.front(),
					arg_list.size()),
				errors);

			next_output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				s_native_module_argument argument = native_module.arguments[arg];
				if (argument.type.get_qualifier() == e_native_module_qualifier::k_out) {
					const s_native_module_compile_time_argument &compile_time_argument = arg_list[arg];

					// Create a constant node for this output
					c_node_reference constant_node_reference = c_node_reference();
					const c_native_module_array *argument_array = nullptr;

					if (argument.type.get_data_type().is_array()) {
						constant_node_reference =
							execution_graph->add_constant_array_node(argument.type.get_data_type().get_element_type());
						argument_array = &compile_time_argument.array_value;
					} else {
						switch (argument.type.get_data_type().get_primitive_type()) {
						case e_native_module_primitive_type::k_real:
							constant_node_reference =
								execution_graph->add_constant_node(compile_time_argument.real_value);
							break;

						case e_native_module_primitive_type::k_bool:
							constant_node_reference =
								execution_graph->add_constant_node(compile_time_argument.bool_value);
							break;

						case e_native_module_primitive_type::k_string:
							constant_node_reference =
								execution_graph->add_constant_node(compile_time_argument.string_value.get_string());
							break;

						default:
							wl_unreachable();
						}
					}

					if (argument_array) {
						// Hook up array inputs
						for (size_t index = 0; index < argument_array->get_array().size(); index++) {
							execution_graph->add_constant_array_value(
								constant_node_reference, argument_array->get_array()[index]);
						}
					}

					// Hook up all outputs with this node instead
					c_node_reference output_node_reference =
						execution_graph->get_node_outgoing_edge_reference(node_reference, next_output);
					while (execution_graph->get_node_outgoing_edge_count(output_node_reference) > 0) {
						c_node_reference to_reference =
							execution_graph->get_node_outgoing_edge_reference(output_node_reference, 0);
						execution_graph->remove_edge(output_node_reference, to_reference);
						execution_graph->add_edge(constant_node_reference, to_reference);
					}

					next_output++;
				}
			}

			wl_assert(next_output == native_module.out_argument_count);

			// Remove any newly created but unused constant nodes
			for (c_node_reference constant_node_reference : node_interface.get_created_node_references()) {
				if (execution_graph->get_node_outgoing_edge_count(constant_node_reference) == 0) {
					execution_graph->remove_node(constant_node_reference);
				}
			}

			// Finally, remove the native module call entirely
			execution_graph->remove_node(node_reference);
			return true;
		}
	}

	// Try to apply all the optimization rules
	for (uint32 rule = 0; rule < c_native_module_registry::get_optimization_rule_count(); rule++) {
		if (try_to_apply_optimization_rule(execution_graph, node_reference, rule)) {
			return true;
		}
	}

	return false;
}

class c_optimization_rule_applier {
private:
	// Used to determine if the rule matches
	struct s_match_state {
		c_node_reference current_node_reference;
		c_node_reference current_node_output_reference;
		uint32 next_input_index;
	};

	c_execution_graph *m_execution_graph;
	c_node_reference m_source_root_node_reference;
	const s_native_module_optimization_rule *m_rule;

	std::stack<s_match_state> m_match_state_stack;

	// Node indices for the values and constants we've matched in the source pattern
	std::array<c_node_reference, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_variable_node_references;
	std::array<c_node_reference, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_constant_node_references;

	bool has_more_inputs(const s_match_state &match_state) const {
		return match_state.next_input_index <
			m_execution_graph->get_node_incoming_edge_count(match_state.current_node_reference);
	}

	c_node_reference follow_next_input(s_match_state &match_state, c_node_reference &output_node_reference) {
		wl_assert(has_more_inputs(match_state));

		// Advance twice to skip the module input node
		c_node_reference input_node_reference = m_execution_graph->get_node_incoming_edge_reference(
			match_state.current_node_reference, match_state.next_input_index);
		c_node_reference new_node_reference = m_execution_graph->get_node_incoming_edge_reference(
			input_node_reference, 0);

		if (m_execution_graph->get_node_type(new_node_reference) == e_execution_graph_node_type::k_indexed_output) {
			// We need to advance an additional node for the case (module <- input <- output <- module)
			output_node_reference = new_node_reference;
			new_node_reference = m_execution_graph->get_node_incoming_edge_reference(new_node_reference, 0);
		}

		match_state.next_input_index++;
		return new_node_reference;
	}

	void store_match(const s_native_module_optimization_symbol &symbol, c_node_reference node_reference) {
		wl_assert(node_reference.is_valid());
		if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
			wl_assert(!m_matched_variable_node_references[symbol.data.index].is_valid());
			m_matched_variable_node_references[symbol.data.index] = node_reference;
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
			wl_assert(!m_matched_constant_node_references[symbol.data.index].is_valid());
			m_matched_constant_node_references[symbol.data.index] = node_reference;
		} else {
			wl_unreachable();
		}
	}

	c_node_reference load_match(const s_native_module_optimization_symbol &symbol) const {
		if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
			wl_assert(m_matched_variable_node_references[symbol.data.index].is_valid());
			return m_matched_variable_node_references[symbol.data.index];
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
			wl_assert(m_matched_constant_node_references[symbol.data.index].is_valid());
			return m_matched_constant_node_references[symbol.data.index];
		} else {
			wl_unreachable();
			return c_node_reference();
		}
	}

	bool handle_source_native_module_symbol_match(uint32 native_module_index) const {
		const s_match_state &current_state = m_match_state_stack.top();
		c_node_reference node_reference = current_state.current_node_reference;

		// It's a match if the current node is a native module of the same index
		return (m_execution_graph->get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call)
			&& (m_execution_graph->get_native_module_call_native_module_index(node_reference) == native_module_index);
	}

	bool handle_source_value_symbol_match(
		const s_native_module_optimization_symbol &symbol,
		c_node_reference node_reference,
		c_node_reference node_output_reference) {
		if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
			// Match anything except for constants
			if (m_execution_graph->get_node_type(node_reference) == e_execution_graph_node_type::k_constant) {
				return false;
			}

			// If this is a module node, it means we had to pass through an output node to get here. Store
			// the output node as the match, because that's what inputs will be hooked up to.
			c_node_reference matched_node_reference =
				node_output_reference.is_valid() ? node_output_reference : node_reference;
			store_match(symbol, matched_node_reference);
			return true;
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
			// Match only constants
			if (m_execution_graph->get_node_type(node_reference) != e_execution_graph_node_type::k_constant) {
				return false;
			}

			store_match(symbol, node_reference);
			return true;
		} else {
			// Match only constants with the given value
			if (m_execution_graph->get_node_type(node_reference) != e_execution_graph_node_type::k_constant) {
				return false;
			}

			// get_constant_node_<type>_value will assert that there's no type mismatch
			if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
				return (symbol.data.real_value == m_execution_graph->get_constant_node_real_value(node_reference));
			} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
				return (symbol.data.bool_value == m_execution_graph->get_constant_node_bool_value(node_reference));
			} else {
				wl_unreachable();
				return false;
			}
		}
	}

	bool try_to_match_source_pattern() {
		IF_ASSERTS_ENABLED(bool should_be_done = false);

		for (size_t sym = 0;
			sym < m_rule->source.symbols.get_count() && m_rule->source.symbols[sym].is_valid();
			sym++) {
			wl_assert(!should_be_done);

			const s_native_module_optimization_symbol &symbol = m_rule->source.symbols[sym];
			switch (symbol.type) {
			case e_native_module_optimization_symbol_type::k_native_module:
			{
				wl_assert(symbol.data.native_module_uid != s_native_module_uid::k_invalid);
				uint32 native_module_index =
					c_native_module_registry::get_native_module_index(symbol.data.native_module_uid);

				if (m_match_state_stack.empty()) {
					// This is the beginning of the source pattern, so add the initial module call to the stack
					s_match_state initial_state;
					initial_state.current_node_output_reference = c_node_reference();
					initial_state.current_node_reference = m_source_root_node_reference;
					initial_state.next_input_index = 0;
					m_match_state_stack.push(initial_state);
				} else {
					// Try to advance to the next input
					s_match_state &current_state = m_match_state_stack.top();
					wl_vassert(has_more_inputs(current_state), "Rule inputs don't match native module definition");

					s_match_state new_state;
					new_state.current_node_output_reference = c_node_reference();
					new_state.current_node_reference =
						follow_next_input(current_state, new_state.current_node_output_reference);
					new_state.next_input_index = 0;
					m_match_state_stack.push(new_state);
				}

				// The module described by the rule should match the top of the state stack
				if (!handle_source_native_module_symbol_match(native_module_index)) {
					return false;
				}

				break;
			}

			case e_native_module_optimization_symbol_type::k_native_module_end:
			{
				wl_assert(!m_match_state_stack.empty());
				// We expect that if we were able to match and enter a module, we should also match when leaving
				// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
				wl_assert(!has_more_inputs(m_match_state_stack.top()));
				IF_ASSERTS_ENABLED(should_be_done = (m_match_state_stack.size() == 1));
				m_match_state_stack.pop();
				break;
			}

			case e_native_module_optimization_symbol_type::k_array_dereference:
				wl_vhalt("Array dereference not allowed in source pattern");
				break;

			case e_native_module_optimization_symbol_type::k_variable:
			case e_native_module_optimization_symbol_type::k_constant:
			case e_native_module_optimization_symbol_type::k_real_value:
			case e_native_module_optimization_symbol_type::k_bool_value:
			{
				// Try to advance to the next input
				wl_assert(!m_match_state_stack.empty());
				s_match_state &current_state = m_match_state_stack.top();
				wl_vassert(has_more_inputs(current_state), "Rule inputs don't match native module definition");

				c_node_reference new_node_output_reference = c_node_reference();
				c_node_reference new_node_reference = follow_next_input(current_state, new_node_output_reference);

				if (!handle_source_value_symbol_match(symbol, new_node_reference, new_node_output_reference)) {
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

	c_node_reference handle_target_array_dereference_symbol_match(
		const s_native_module_optimization_symbol &array_symbol,
		const s_native_module_optimization_symbol &index_symbol) {
		// Validate the array and index symbols
		wl_assert(array_symbol.is_valid());
		wl_assert(array_symbol.type == e_native_module_optimization_symbol_type::k_constant);
		c_node_reference array_node_reference = load_match(array_symbol);
		wl_assert(m_execution_graph->get_constant_node_data_type(array_node_reference).is_array());

		wl_assert(index_symbol.is_valid());
		wl_assert(index_symbol.type == e_native_module_optimization_symbol_type::k_constant);
		c_node_reference index_node_reference = load_match(index_symbol);
		wl_assert(m_execution_graph->get_constant_node_data_type(index_node_reference) ==
			c_native_module_data_type(e_native_module_primitive_type::k_real));

		// Obtain the array index
		uint32 array_index = 0;
		bool valid_array_index = sanitize_array_index(
			m_execution_graph,
			array_node_reference,
			m_execution_graph->get_constant_node_real_value(index_node_reference),
			array_index);

		// If array index is out of bounds, we fall back to a default value
		c_node_reference dereferenced_node_reference = c_node_reference();
		if (!valid_array_index) {
			switch (m_execution_graph->get_constant_node_data_type(array_node_reference).get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				dereferenced_node_reference = m_execution_graph->add_constant_node(0.0f);
				break;

			case e_native_module_primitive_type::k_bool:
				dereferenced_node_reference = m_execution_graph->add_constant_node(false);
				break;

			case e_native_module_primitive_type::k_string:
				dereferenced_node_reference = m_execution_graph->add_constant_node("");
				break;

			default:
				wl_unreachable();
			}
		} else {
			// Follow the incoming index specified from the array node
			c_node_reference input_node_reference =
				m_execution_graph->get_node_incoming_edge_reference(array_node_reference, array_index);
			dereferenced_node_reference = m_execution_graph->get_node_incoming_edge_reference(input_node_reference, 0);
		}

		return dereferenced_node_reference;
	}

	c_node_reference handle_target_value_symbol_match(const s_native_module_optimization_symbol &symbol) {
		if (symbol.type == e_native_module_optimization_symbol_type::k_variable
			|| symbol.type == e_native_module_optimization_symbol_type::k_constant) {
			return load_match(symbol);
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
			// Create a constant node with this value
			return m_execution_graph->add_constant_node(symbol.data.real_value);
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
			// Create a constant node with this value
			return m_execution_graph->add_constant_node(symbol.data.bool_value);
		} else {
			wl_unreachable();
			return c_node_reference();
		}
	}

	c_node_reference build_target_pattern() {
		// Replaced the matched source pattern nodes with the ones defined by the target in the rule
		c_node_reference root_node_reference = c_node_reference();

		IF_ASSERTS_ENABLED(bool should_be_done = false);

		for (size_t sym = 0;
			sym < m_rule->target.symbols.get_count() && m_rule->target.symbols[sym].is_valid();
			sym++) {
			wl_assert(!should_be_done);

			const s_native_module_optimization_symbol &symbol = m_rule->target.symbols[sym];
			switch (symbol.type) {
			case e_native_module_optimization_symbol_type::k_native_module:
			{
				uint32 native_module_index =
					c_native_module_registry::get_native_module_index(symbol.data.native_module_uid);
				c_node_reference node_reference = m_execution_graph->add_native_module_call_node(native_module_index);
				// We currently only allow a single outgoing edge, due to the way we express rules
				wl_assert(m_execution_graph->get_node_outgoing_edge_count(node_reference) == 1);

				if (!root_node_reference.is_valid()) {
					// This is the root node of the target pattern
					wl_assert(m_match_state_stack.empty());
					root_node_reference = node_reference;
				} else {
					s_match_state &current_state = m_match_state_stack.top();
					wl_assert(has_more_inputs(current_state));

					c_node_reference input_node_reference = m_execution_graph->get_node_incoming_edge_reference(
						current_state.current_node_reference, current_state.next_input_index);
					c_node_reference output_node_reference = m_execution_graph->get_node_outgoing_edge_reference(
						node_reference, 0);
					m_execution_graph->add_edge(output_node_reference, input_node_reference);
					current_state.next_input_index++;
				}

				s_match_state new_state;
				new_state.current_node_output_reference = c_node_reference();
				new_state.current_node_reference = node_reference;
				new_state.next_input_index = 0;
				m_match_state_stack.push(new_state);
				break;
			}

			case e_native_module_optimization_symbol_type::k_native_module_end:
			{
				wl_assert(!m_match_state_stack.empty());
				// We expect that if we were able to match and enter a module, we should also match when leaving
				// If not, it means the rule does not match the definition of the module (e.g. too few arguments)
				wl_assert(!has_more_inputs(m_match_state_stack.top()));
				IF_ASSERTS_ENABLED(should_be_done = m_match_state_stack.size() == 1);
				m_match_state_stack.pop();
				break;
			}

			case e_native_module_optimization_symbol_type::k_array_dereference:
			case e_native_module_optimization_symbol_type::k_variable:
			case e_native_module_optimization_symbol_type::k_constant:
			case e_native_module_optimization_symbol_type::k_real_value:
			case e_native_module_optimization_symbol_type::k_bool_value:
			{
				c_node_reference matched_node_reference;
				if (symbol.type == e_native_module_optimization_symbol_type::k_array_dereference) {
					// Read the next two symbols - must be constant array and constant index
					wl_assert(sym + 2 <= m_rule->target.symbols.get_count());
					sym++;
					const s_native_module_optimization_symbol &array_symbol = m_rule->target.symbols[sym];
					sym++;
					const s_native_module_optimization_symbol &index_symbol = m_rule->target.symbols[sym];

					matched_node_reference = handle_target_array_dereference_symbol_match(array_symbol, index_symbol);
				} else {
					matched_node_reference = handle_target_value_symbol_match(symbol);
				}

				// Hook up this input and advance
				if (!root_node_reference.is_valid()) {
					wl_assert(m_match_state_stack.empty());
					root_node_reference = matched_node_reference;
					IF_ASSERTS_ENABLED(should_be_done = true);
				} else {
					wl_assert(!m_match_state_stack.empty());
					s_match_state &current_state = m_match_state_stack.top();
					wl_assert(has_more_inputs(current_state));

					c_node_reference input_node_reference = m_execution_graph->get_node_incoming_edge_reference(
						current_state.current_node_reference, current_state.next_input_index);
					current_state.next_input_index++;
					m_execution_graph->add_edge(matched_node_reference, input_node_reference);
				}
				break;
			}

			default:
				wl_unreachable();
			}
		}

		wl_assert(m_match_state_stack.empty());
		return root_node_reference;
	}

	void reroute_source_to_target(c_node_reference target_root_node_reference) {
		// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
		// deleting the old set of nodes, they will automatically be cleaned up later.

		// We currently only support modules with a single output
		wl_assert(m_execution_graph->get_node_outgoing_edge_count(m_source_root_node_reference) == 1);
		c_node_reference old_output_node =
			m_execution_graph->get_node_outgoing_edge_reference(m_source_root_node_reference, 0);

		switch (m_execution_graph->get_node_type(target_root_node_reference)) {
		case e_execution_graph_node_type::k_native_module_call:
		{
			wl_assert(m_execution_graph->get_node_outgoing_edge_count(target_root_node_reference) == 1);
			c_node_reference new_output_node =
				m_execution_graph->get_node_outgoing_edge_reference(target_root_node_reference, 0);
			transfer_outputs(m_execution_graph, new_output_node, old_output_node);
			break;
		}

		case e_execution_graph_node_type::k_constant:
		case e_execution_graph_node_type::k_indexed_output:
			transfer_outputs(m_execution_graph, target_root_node_reference, old_output_node);
			break;

		case e_execution_graph_node_type::k_temporary_reference:
			// These should have all been removed
			wl_unreachable();
			break;

		default:
			wl_unreachable();
		}
	}

public:
	c_optimization_rule_applier(
		c_execution_graph *execution_graph, c_node_reference source_root_node_reference, uint32 rule_index) {
		m_execution_graph = execution_graph;
		m_source_root_node_reference = source_root_node_reference;
		m_rule = &c_native_module_registry::get_optimization_rule(rule_index);
	}

	bool try_to_apply_optimization_rule() {
		wl_assert(m_rule);

		// Initially we haven't matched any values
		std::fill(
			m_matched_variable_node_references.begin(), m_matched_variable_node_references.end(), c_node_reference());
		std::fill(
			m_matched_constant_node_references.begin(), m_matched_constant_node_references.end(), c_node_reference());

		if (try_to_match_source_pattern()) {
			c_node_reference target_root_node_reference = build_target_pattern();
			reroute_source_to_target(target_root_node_reference);
			return true;
		}

		return false;
	}
};

static bool try_to_apply_optimization_rule(
	c_execution_graph *execution_graph,
	c_node_reference node_reference,
	uint32 rule_index) {
	c_optimization_rule_applier optimization_rule_applier(execution_graph, node_reference, rule_index);
	return optimization_rule_applier.try_to_apply_optimization_rule();
}

void remove_useless_nodes(c_execution_graph *execution_graph) {
	std::stack<c_node_reference> node_stack;
	std::set<c_node_reference> nodes_visited;

	// Start at the output nodes and work backwards, marking each node as it is encountered
	for (c_node_reference node_reference = execution_graph->nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph->nodes_next(node_reference)) {
		e_execution_graph_node_type type = execution_graph->get_node_type(node_reference);
		if (type == e_execution_graph_node_type::k_output) {
			node_stack.push(node_reference);
			nodes_visited.insert(node_reference);
		}
	}

	while (!node_stack.empty()) {
		c_node_reference node_reference = node_stack.top();
		node_stack.pop();

		for (size_t edge = 0; edge < execution_graph->get_node_incoming_edge_count(node_reference); edge++) {
			c_node_reference input_reference = execution_graph->get_node_incoming_edge_reference(node_reference, edge);
			if (nodes_visited.find(input_reference) == nodes_visited.end()) {
				node_stack.push(input_reference);
				nodes_visited.insert(input_reference);
			}
		}
	}

	// Remove all unvisited nodes
	for (c_node_reference node_reference = execution_graph->nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph->nodes_next(node_reference)) {
		if (nodes_visited.find(node_reference) == nodes_visited.end()) {
			// A few exceptions:
			// - Don't directly remove inputs/outputs, since they automatically get removed along with their nodes
			// - Avoid removing invalid nodes, because they're already removed
			// - Don't remove unused inputs or we would get unexpected graph incompatibility errors if an input is
			//   unused because input node count defines the number of inputs
			e_execution_graph_node_type type = execution_graph->get_node_type(node_reference);
			if (type != e_execution_graph_node_type::k_invalid
				&& type != e_execution_graph_node_type::k_indexed_input
				&& type != e_execution_graph_node_type::k_indexed_output
				&& type != e_execution_graph_node_type::k_input) {
				execution_graph->remove_node(node_reference);
			}
		}
	}
}

static void transfer_outputs(
	c_execution_graph *execution_graph,
	c_node_reference destination_reference,
	c_node_reference source_reference) {
	// Redirect the inputs of input_node directly to the outputs of output_node
	while (execution_graph->get_node_outgoing_edge_count(source_reference) > 0) {
		c_node_reference to_node_reference = execution_graph->get_node_outgoing_edge_reference(source_reference, 0);
		execution_graph->remove_edge(source_reference, to_node_reference);
		execution_graph->add_edge(destination_reference, to_node_reference);
	}
}

static void deduplicate_nodes(c_execution_graph *execution_graph) {
	// 1) deduplicate all constant nodes with the same value
	// 2) combine any native modules and arrays with the same type and inputs
	// 3) repeat (2) until no changes occur

	// Combine all equivalent constant nodes. Currently n^2, we could easily do better if it's worth the speedup.
	for (c_node_reference node_a_reference = execution_graph->nodes_begin();
		node_a_reference.is_valid();
		node_a_reference = execution_graph->nodes_next(node_a_reference)) {
		if (execution_graph->get_node_type(node_a_reference) != e_execution_graph_node_type::k_constant) {
			continue;
		}

		for (c_node_reference node_b_reference = execution_graph->nodes_next(node_a_reference);
			node_b_reference.is_valid();
			node_b_reference = execution_graph->nodes_next(node_b_reference)) {
			if (execution_graph->get_node_type(node_b_reference) != e_execution_graph_node_type::k_constant) {
				continue;
			}

			// Both nodes are constant nodes
			c_native_module_data_type type = execution_graph->get_constant_node_data_type(node_a_reference);
			if (type != execution_graph->get_constant_node_data_type(node_b_reference)) {
				continue;
			}

			bool equal = false;

			if (type.is_array()) {
				// Arrays are deduplicated later
			} else {
				switch (type.get_primitive_type()) {
				case e_native_module_primitive_type::k_real:
					equal = (execution_graph->get_constant_node_real_value(node_a_reference) ==
						execution_graph->get_constant_node_real_value(node_b_reference));
					break;

				case e_native_module_primitive_type::k_bool:
					equal = (execution_graph->get_constant_node_bool_value(node_a_reference) ==
						execution_graph->get_constant_node_bool_value(node_b_reference));
					break;

				case e_native_module_primitive_type::k_string:
					equal = (strcmp(execution_graph->get_constant_node_string_value(node_a_reference),
						execution_graph->get_constant_node_string_value(node_b_reference)) == 0);
					break;

				default:
					wl_unreachable();
				}
			}

			if (equal) {
				// Redirect node B's outputs as outputs of node A
				while (execution_graph->get_node_outgoing_edge_count(node_b_reference) > 0) {
					c_node_reference to_node_reference = execution_graph->get_node_outgoing_edge_reference(
						node_b_reference,
						execution_graph->get_node_outgoing_edge_count(node_b_reference) - 1);
					execution_graph->remove_edge(node_b_reference, to_node_reference);
					execution_graph->add_edge(node_a_reference, to_node_reference);
				}

				// Remove node B, since it has no outgoing edges left
				execution_graph->remove_node(node_b_reference);
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
		for (c_node_reference node_a_reference = execution_graph->nodes_begin();
			node_a_reference.is_valid();
			node_a_reference = execution_graph->nodes_next(node_a_reference)) {
			if (execution_graph->get_node_type(node_a_reference) == e_execution_graph_node_type::k_invalid) {
				continue;
			}

			if (!execution_graph->does_node_use_indexed_inputs(node_a_reference)) {
				continue;
			}

			for (c_node_reference node_b_reference = execution_graph->nodes_next(node_a_reference);
				node_b_reference.is_valid();
				node_b_reference = execution_graph->nodes_next(node_b_reference)) {
				if (execution_graph->get_node_type(node_a_reference) !=
					execution_graph->get_node_type(node_b_reference)) {
					continue;
				}

				e_execution_graph_node_type node_type = execution_graph->get_node_type(node_a_reference);
				if (node_type == e_execution_graph_node_type::k_native_module_call) {
					// If native module indices don't match, skip
					if (execution_graph->get_native_module_call_native_module_index(node_a_reference) !=
						execution_graph->get_native_module_call_native_module_index(node_b_reference)) {
						continue;
					}
				} else if (node_type == e_execution_graph_node_type::k_constant) {
					wl_assert(execution_graph->get_constant_node_data_type(node_a_reference).is_array());

					// If array types don't match, skip
					if (execution_graph->get_constant_node_data_type(node_a_reference) !=
						execution_graph->get_constant_node_data_type(node_b_reference)) {
						continue;
					}

					wl_assert(execution_graph->get_constant_node_data_type(node_b_reference).is_array());

					// If array sizes don't match, skip
					if (execution_graph->get_node_incoming_edge_count(node_a_reference) !=
						execution_graph->get_node_incoming_edge_count(node_b_reference)) {
						continue;
					}
				} else {
					// What other node types use indexed inputs?
					wl_unreachable();
				}

				wl_assert(execution_graph->get_node_incoming_edge_count(node_a_reference) ==
					execution_graph->get_node_incoming_edge_count(node_b_reference));

				uint32 identical_inputs = true;
				for (size_t edge = 0;
					identical_inputs && edge < execution_graph->get_node_incoming_edge_count(node_a_reference);
					edge++) {
					// Skip past the "input" nodes directly to the source
					c_node_reference source_node_a = execution_graph->get_node_incoming_edge_reference(
						execution_graph->get_node_incoming_edge_reference(node_a_reference, edge), 0);
					c_node_reference source_node_b = execution_graph->get_node_incoming_edge_reference(
						execution_graph->get_node_incoming_edge_reference(node_b_reference, edge), 0);

					identical_inputs = (source_node_a == source_node_b);
				}

				if (identical_inputs) {
					if (execution_graph->does_node_use_indexed_outputs(node_a_reference)) {
						// Remap each indexed output
						for (size_t edge = 0;
							edge < execution_graph->get_node_outgoing_edge_count(node_a_reference);
							edge++) {
							c_node_reference output_node_a =
								execution_graph->get_node_outgoing_edge_reference(node_a_reference, edge);
							c_node_reference output_node_b =
								execution_graph->get_node_outgoing_edge_reference(node_b_reference, edge);

							while (execution_graph->get_node_outgoing_edge_count(output_node_b) > 0) {
								c_node_reference to_node_reference = execution_graph->get_node_outgoing_edge_reference(
									output_node_b,
									execution_graph->get_node_outgoing_edge_count(output_node_b) - 1);
								execution_graph->remove_edge(output_node_b, to_node_reference);
								execution_graph->add_edge(output_node_a, to_node_reference);
							}
						}
					} else {
						// Directly remap outputs
						while (execution_graph->get_node_outgoing_edge_count(node_b_reference) > 0) {
							c_node_reference to_node_reference = execution_graph->get_node_outgoing_edge_reference(
								node_b_reference,
								execution_graph->get_node_outgoing_edge_count(node_b_reference) - 1);
							execution_graph->remove_edge(node_b_reference, to_node_reference);
							execution_graph->add_edge(node_a_reference, to_node_reference);
						}
					}

					// Remove node B, since it has no outgoing edges left
					execution_graph->remove_node(node_b_reference);
					graph_changed = true;
				}
			}
		}
	} while (graph_changed);
}

static void validate_optimized_constants(
	const c_execution_graph *execution_graph,
	std::vector<s_compiler_result> &errors_out) {
	for (c_node_reference node_reference = execution_graph->nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph->nodes_next(node_reference)) {
		if (execution_graph->get_node_type(node_reference) != e_execution_graph_node_type::k_native_module_call) {
			continue;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			execution_graph->get_native_module_call_native_module_index(node_reference));

		wl_assert(native_module.in_argument_count == execution_graph->get_node_incoming_edge_count(node_reference));
		// For each constant input, verify that a constant node is linked up
		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			e_native_module_qualifier qualifier = native_module.arguments[arg].type.get_qualifier();
			if (qualifier == e_native_module_qualifier::k_in) {
				input++;
			} else if (qualifier == e_native_module_qualifier::k_constant) {
				// Validate that this input is constant
				c_node_reference input_node_reference =
					execution_graph->get_node_incoming_edge_reference(node_reference, input);
				c_node_reference constant_node_reference =
					execution_graph->get_node_incoming_edge_reference(input_node_reference, 0);

				if (execution_graph->get_node_type(constant_node_reference) !=
					e_execution_graph_node_type::k_constant) {
					s_compiler_result error;
					error.result = e_compiler_result::k_constant_expected;
					error.source_location.clear();
					error.message = "Input argument to native module call '" +
						std::string(native_module.name.get_string()) + "' does not resolve to a constant";
					errors_out.push_back(error);
				}

				input++;
			}
		}

		wl_assert(input == native_module.in_argument_count);
	}
}
