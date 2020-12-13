#include "compiler/components/instrument_variant_optimizer.h"
#include "compiler/graph_trimmer.h"
#include "compiler/optimization_rule_applicator.h"
#include "compiler/try_call_native_module.h"

#include "execution_graph/execution_graph.h"

#include <stack>
#include <unordered_map>
#include <unordered_set>

class c_execution_graph_optimizer {
public:
	c_execution_graph_optimizer(
		c_compiler_context &context,
		c_execution_graph &execution_graph,
		const s_instrument_globals &instrument_globals);

	bool optimize();

private:
	void remove_unused_nodes();

	bool try_optimize_node(c_node_reference node_reference, bool &did_optimize_out);

	void deduplicate_nodes();
	void deduplicate_constants();
	void deduplicate_arrays_and_native_modules();

	// Any edges (original -> X) get swapped out for (remapped -> X)
	void remap_outputs(c_node_reference original_node_reference, c_node_reference remapped_node_reference);

	c_compiler_context &m_context;
	c_execution_graph &m_execution_graph;
	const s_instrument_globals &m_instrument_globals;

	c_graph_trimmer m_graph_trimmer;
	c_optimization_rule_applicator m_optimization_rule_applicator;
};

bool c_instrument_variant_optimizer::optimize_instrument_variant(
	c_compiler_context &context,
	c_instrument_variant &instrument_variant) {
	if (instrument_variant.get_voice_execution_graph()) {
		c_execution_graph_optimizer execution_graph_optimizer(
			context,
			*instrument_variant.get_voice_execution_graph(),
			instrument_variant.get_instrument_globals());

		if (!execution_graph_optimizer.optimize()) {
			return false;
		}
	}

	if (instrument_variant.get_fx_execution_graph()) {
		c_execution_graph_optimizer execution_graph_optimizer(
			context,
			*instrument_variant.get_fx_execution_graph(),
			instrument_variant.get_instrument_globals());

		if (!execution_graph_optimizer.optimize()) {
			return false;
		}
	}

	return true;
}

c_execution_graph_optimizer::c_execution_graph_optimizer(
	c_compiler_context &context,
	c_execution_graph &execution_graph,
	const s_instrument_globals &instrument_globals)
	: m_context(context)
	, m_execution_graph(execution_graph)
	, m_instrument_globals(instrument_globals)
	, m_graph_trimmer(execution_graph)
	, m_optimization_rule_applicator(execution_graph) {}

bool c_execution_graph_optimizer::optimize() {
	remove_unused_nodes();

	// Keep trying to optimize nodes until no optimizations can be applied
	while (true) {
		bool optimization_performed = false;
		for (c_node_reference node_reference = m_execution_graph.nodes_begin();
			!optimization_performed && node_reference.is_valid();
			node_reference = m_execution_graph.nodes_next(node_reference)) {
			// $PERF: we can only apply one optimization per loop because iteration gets invalidated when the graph is
			// modified
			if (!try_optimize_node(node_reference, optimization_performed)) {
				return false;
			}
		}

		if (optimization_performed) {
			remove_unused_nodes();
		} else {
			break;
		}
	}

	deduplicate_nodes();
	m_execution_graph.remove_unused_nodes_and_reassign_node_indices();

	return true;
}

void c_execution_graph_optimizer::remove_unused_nodes() {
	std::stack<c_node_reference> node_stack;
	std::unordered_set<c_node_reference> nodes_visited;

	// Start at the output nodes and work backwards, marking each node as it is encountered
	for (c_node_reference node_reference = m_execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = m_execution_graph.nodes_next(node_reference)) {
		e_execution_graph_node_type type = m_execution_graph.get_node_type(node_reference);
		if (type == e_execution_graph_node_type::k_output) {
			node_stack.push(node_reference);
			nodes_visited.insert(node_reference);
		}
	}

	while (!node_stack.empty()) {
		c_node_reference node_reference = node_stack.top();
		node_stack.pop();

		for (size_t edge = 0; edge < m_execution_graph.get_node_incoming_edge_count(node_reference); edge++) {
			c_node_reference input_reference = m_execution_graph.get_node_incoming_edge_reference(node_reference, edge);
			if (nodes_visited.find(input_reference) == nodes_visited.end()) {
				node_stack.push(input_reference);
				nodes_visited.insert(input_reference);
			}
		}
	}

	// Remove all unvisited nodes
	for (c_node_reference node_reference = m_execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = m_execution_graph.nodes_next(node_reference)) {
		if (nodes_visited.find(node_reference) == nodes_visited.end()) {
			// A few exceptions:
			// - Don't directly remove inputs/outputs, since they automatically get removed along with their nodes
			// - Don't remove unused inputs or we would get unexpected graph incompatibility errors if an input is
			//   unused because input node count defines the number of inputs
			e_execution_graph_node_type type = m_execution_graph.get_node_type(node_reference);
			if (type != e_execution_graph_node_type::k_indexed_input
				&& type != e_execution_graph_node_type::k_indexed_output
				&& type != e_execution_graph_node_type::k_input) {
				m_execution_graph.remove_node(node_reference);
			}
		}
	}
}

bool c_execution_graph_optimizer::try_optimize_node(c_node_reference node_reference, bool &did_optimize_out) {
	did_optimize_out = false;

	if (m_execution_graph.get_node_type(node_reference) != e_execution_graph_node_type::k_native_module_call) {
		// The only type of node we can optimize are native module nodes
		return true;
	}

	// First, see if we can call the native module. This would be unusual because we attempt to fold constants when
	// initially building the graph, but it's possible for an optimization rule to convert a non-constant into a
	// constant. An example of this is multiplying by 0.
	if (!try_call_native_module(
		m_context,
		m_graph_trimmer,
		m_instrument_globals,
		node_reference,
		s_compiler_source_location(), // $TODO $COMPILER we could add "compiler context" pointer to graph nodes
		did_optimize_out)) {
		return false;
	}

	if (did_optimize_out) {
		return true;
	}

	// Try to apply all the optimization rules
	for (uint32 rule_index = 0; rule_index < c_native_module_registry::get_optimization_rule_count(); rule_index++) {
		if (m_optimization_rule_applicator.try_apply_optimization_rule(node_reference, rule_index)) {
			did_optimize_out = true;
			return true;
		}
	}

	return true;
}

void c_execution_graph_optimizer::deduplicate_nodes() {
	deduplicate_constants();
	deduplicate_arrays_and_native_modules();
}

void c_execution_graph_optimizer::deduplicate_constants() {
	std::unordered_map<real32, c_node_reference> deduplicated_reals;
	std::unordered_map<bool, c_node_reference> deduplicated_bools;
	std::unordered_map<std::string, c_node_reference> deduplicated_strings;

	for (c_node_reference node_reference = m_execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = m_execution_graph.nodes_next(node_reference)) {
		if (m_execution_graph.get_node_type(node_reference) != e_execution_graph_node_type::k_constant) {
			continue;
		}

		c_native_module_data_type type = m_execution_graph.get_node_data_type(node_reference);
		c_node_reference deduplicated_node_reference;
		switch (type.get_primitive_type()) {
		case e_native_module_primitive_type::k_real:
		{
			real32 value = m_execution_graph.get_constant_node_real_value(node_reference);
			auto iter = deduplicated_reals.find(value);
			if (iter == deduplicated_reals.end()) {
				deduplicated_reals.insert(std::make_pair(value, node_reference));
			} else {
				deduplicated_node_reference = iter->second;
			}
			break;
		}

		case e_native_module_primitive_type::k_bool:
		{
			bool value = m_execution_graph.get_constant_node_bool_value(node_reference);
			auto iter = deduplicated_bools.find(value);
			if (iter == deduplicated_bools.end()) {
				deduplicated_bools.insert(std::make_pair(value, node_reference));
			} else {
				deduplicated_node_reference = iter->second;
			}
			break;
		}

		case e_native_module_primitive_type::k_string:
		{
			std::string value = m_execution_graph.get_constant_node_string_value(node_reference);
			auto iter = deduplicated_strings.find(value);
			if (iter == deduplicated_strings.end()) {
				deduplicated_strings.insert(std::make_pair(value, node_reference));
			} else {
				deduplicated_node_reference = iter->second;
			}
			break;
		}

		default:
			wl_unreachable();
		}

		if (deduplicated_node_reference.is_valid()) {
			// Redirect node_reference's outputs as outputs of deduplicated_node_reference
			remap_outputs(node_reference, deduplicated_node_reference);
		}
	}

	remove_unused_nodes();
}

void c_execution_graph_optimizer::deduplicate_arrays_and_native_modules() {
	// Find any module calls or arrays with identical type and inputs and merge them. (The common factor here is
	// nodes which use indexed inputs.)
	// Currently n^2 but we could do better easily.

	// Repeat until the graph doesn't change. If we topologically sorted and deduplicated in that order, we wouldn't
	// need to run multiple passes.
	bool graph_changed;
	do {
		graph_changed = false;
		std::unordered_set<c_node_reference> deduplicated_nodes;

		for (c_node_reference node_a_reference = m_execution_graph.nodes_begin();
			node_a_reference.is_valid();
			node_a_reference = m_execution_graph.nodes_next(node_a_reference)) {
			if (m_execution_graph.get_node_type(node_a_reference) == e_execution_graph_node_type::k_invalid) {
				continue;
			}

			if (!m_execution_graph.does_node_use_indexed_inputs(node_a_reference)) {
				continue;
			}

			for (c_node_reference node_b_reference = m_execution_graph.nodes_next(node_a_reference);
				node_b_reference.is_valid();
				node_b_reference = m_execution_graph.nodes_next(node_b_reference)) {
				if (deduplicated_nodes.contains(node_b_reference)) {
					// We already deduplicated this node - it has no outputs
					continue;
				}

				if (m_execution_graph.get_node_type(node_a_reference) !=
					m_execution_graph.get_node_type(node_b_reference)) {
					continue;
				}

				e_execution_graph_node_type node_type = m_execution_graph.get_node_type(node_a_reference);
				if (node_type == e_execution_graph_node_type::k_native_module_call) {
					// If native module indices don't match, skip
					if (m_execution_graph.get_native_module_call_native_module_handle(node_a_reference) !=
						m_execution_graph.get_native_module_call_native_module_handle(node_b_reference)) {
						continue;
					}
				} else if (node_type == e_execution_graph_node_type::k_array) {
					wl_assert(m_execution_graph.get_node_data_type(node_a_reference).is_array());

					// If array types don't match, skip
					if (m_execution_graph.get_node_data_type(node_a_reference) !=
						m_execution_graph.get_node_data_type(node_b_reference)) {
						continue;
					}

					wl_assert(m_execution_graph.get_node_data_type(node_b_reference).is_array());

					// If array sizes don't match, skip
					if (m_execution_graph.get_node_incoming_edge_count(node_a_reference) !=
						m_execution_graph.get_node_incoming_edge_count(node_b_reference)) {
						continue;
					}
				} else {
					// No other node types use indexed inputs
					wl_unreachable();
				}

				size_t edge_count = m_execution_graph.get_node_incoming_edge_count(node_a_reference);
				wl_assert(edge_count == m_execution_graph.get_node_incoming_edge_count(node_b_reference));

				uint32 identical_inputs = true;
				for (size_t edge = 0; identical_inputs && edge < edge_count; edge++) {
					// Skip past the "input" nodes directly to the source
					c_node_reference source_node_a =
						m_execution_graph.get_node_indexed_input_incoming_edge_reference(node_a_reference, edge, 0);
					c_node_reference source_node_b =
						m_execution_graph.get_node_indexed_input_incoming_edge_reference(node_b_reference, edge, 0);

					identical_inputs = (source_node_a == source_node_b);
				}

				if (identical_inputs) {
					// Remap all outputs from node B to be outputs of node A
					remap_outputs(node_b_reference, node_a_reference);

					// Add to the list of deduplicated nodes so we don't bother looking at this node again
					deduplicated_nodes.insert(node_b_reference);

					graph_changed = true;
				}
			}
		}

		remove_unused_nodes();
	} while (graph_changed);
}

void c_execution_graph_optimizer::remap_outputs(
	c_node_reference original_node_reference,
	c_node_reference remapped_node_reference) {
	wl_assert(m_execution_graph.does_node_use_indexed_outputs(original_node_reference)
		== m_execution_graph.does_node_use_indexed_outputs(remapped_node_reference));

	if (m_execution_graph.does_node_use_indexed_outputs(original_node_reference)) {
		// Remap each indexed output
		size_t edge_count = m_execution_graph.get_node_outgoing_edge_count(original_node_reference);
		wl_assert(edge_count == m_execution_graph.get_node_outgoing_edge_count(remapped_node_reference));
		for (size_t edge = 0; edge < edge_count; edge++) {
			c_node_reference original_output_node =
				m_execution_graph.get_node_outgoing_edge_reference(original_node_reference, edge);
			c_node_reference remapped_output_node =
				m_execution_graph.get_node_outgoing_edge_reference(remapped_node_reference, edge);

			while (m_execution_graph.get_node_outgoing_edge_count(original_output_node) > 0) {
				c_node_reference to_node_reference = m_execution_graph.get_node_outgoing_edge_reference(
					original_output_node,
					m_execution_graph.get_node_outgoing_edge_count(original_output_node) - 1);
				m_execution_graph.remove_edge(original_output_node, to_node_reference);
				m_execution_graph.add_edge(remapped_output_node, to_node_reference);
			}
		}
	} else {
		// Directly remap outputs
		while (m_execution_graph.get_node_outgoing_edge_count(original_node_reference) > 0) {
			c_node_reference to_node_reference = m_execution_graph.get_node_outgoing_edge_reference(
				original_node_reference,
				m_execution_graph.get_node_outgoing_edge_count(original_node_reference) - 1);
			m_execution_graph.remove_edge(original_node_reference, to_node_reference);
			m_execution_graph.add_edge(remapped_node_reference, to_node_reference);
		}
	}
}
