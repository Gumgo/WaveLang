#include "compiler/components/instrument_variant_optimizer.h"
#include "compiler/graph_trimmer.h"
#include "compiler/optimization_rule_applicator.h"
#include "compiler/try_call_native_module.h"

#include "instrument/native_module_graph.h"

#include <stack>
#include <unordered_map>
#include <unordered_set>

class c_native_module_graph_optimizer {
public:
	c_native_module_graph_optimizer(
		c_compiler_context &context,
		c_native_module_graph &native_module_graph,
		const s_instrument_globals &instrument_globals);

	bool optimize();

private:
	void remove_unused_nodes();

	bool try_optimize_node(h_graph_node node_handle, bool &did_optimize_out);

	bool deduplicate_nodes();
	bool deduplicate_constants();
	bool deduplicate_arrays_and_native_modules();

	// Any edges (original -> X) get swapped out for (remapped -> X)
	void remap_outputs(h_graph_node original_node_handle, h_graph_node remapped_node_handle);

	c_compiler_context &m_context;
	c_native_module_graph &m_native_module_graph;
	const s_instrument_globals &m_instrument_globals;

	c_graph_trimmer m_graph_trimmer;
	c_optimization_rule_applicator m_optimization_rule_applicator;
};

bool c_instrument_variant_optimizer::optimize_instrument_variant(
	c_compiler_context &context,
	c_instrument_variant &instrument_variant) {
	if (instrument_variant.get_voice_native_module_graph()) {
		c_native_module_graph_optimizer native_module_graph_optimizer(
			context,
			*instrument_variant.get_voice_native_module_graph(),
			instrument_variant.get_instrument_globals());

		if (!native_module_graph_optimizer.optimize()) {
			return false;
		}
	}

	if (instrument_variant.get_fx_native_module_graph()) {
		c_native_module_graph_optimizer native_module_graph_optimizer(
			context,
			*instrument_variant.get_fx_native_module_graph(),
			instrument_variant.get_instrument_globals());

		if (!native_module_graph_optimizer.optimize()) {
			return false;
		}
	}

	return true;
}

c_native_module_graph_optimizer::c_native_module_graph_optimizer(
	c_compiler_context &context,
	c_native_module_graph &native_module_graph,
	const s_instrument_globals &instrument_globals)
	: m_context(context)
	, m_native_module_graph(native_module_graph)
	, m_instrument_globals(instrument_globals)
	, m_graph_trimmer(native_module_graph)
	, m_optimization_rule_applicator(native_module_graph) {}

bool c_native_module_graph_optimizer::optimize() {
	remove_unused_nodes();

	// Keep trying to optimize nodes until no optimizations can be applied
	bool optimization_performed;
	do {
		optimization_performed = false;

		while (true) {
			bool node_optimized = false;
			for (h_graph_node node_handle : m_native_module_graph.iterate_nodes()) {
				// $PERF: we can only apply one optimization per loop because iteration gets invalidated when the graph
				// is modified
				if (!try_optimize_node(node_handle, node_optimized)) {
					return false;
				} else if (optimization_performed) {
					optimization_performed = true;
					break;
				}
			}

			if (node_optimized) {
				remove_unused_nodes();
			} else {
				break;
			}
		}

		optimization_performed |= deduplicate_nodes();
	} while (optimization_performed);

	m_native_module_graph.remove_unused_nodes_and_reassign_node_indices();
	return true;
}

void c_native_module_graph_optimizer::remove_unused_nodes() {
	std::stack<h_graph_node> node_stack;
	std::unordered_set<h_graph_node> nodes_visited;

	// Start at the output nodes and work backwards, marking each node as it is encountered
	for (h_graph_node node_handle : m_native_module_graph.iterate_nodes()) {
		e_native_module_graph_node_type type = m_native_module_graph.get_node_type(node_handle);
		if (type == e_native_module_graph_node_type::k_output) {
			node_stack.push(node_handle);
			nodes_visited.insert(node_handle);
		}
	}

	while (!node_stack.empty()) {
		h_graph_node node_handle = node_stack.top();
		node_stack.pop();

		for (size_t edge = 0; edge < m_native_module_graph.get_node_incoming_edge_count(node_handle); edge++) {
			h_graph_node input_handle = m_native_module_graph.get_node_incoming_edge_handle(node_handle, edge);
			if (nodes_visited.find(input_handle) == nodes_visited.end()) {
				node_stack.push(input_handle);
				nodes_visited.insert(input_handle);
			}
		}
	}

	// Remove all unvisited nodes
	for (h_graph_node node_handle : m_native_module_graph.iterate_nodes()) {
		if (nodes_visited.find(node_handle) == nodes_visited.end()) {
			// A few exceptions:
			// - Don't directly remove inputs/outputs, since they automatically get removed along with their nodes
			// - Don't remove unused inputs or we would get unexpected graph incompatibility errors if an input is
			//   unused because input node count defines the number of inputs
			e_native_module_graph_node_type type = m_native_module_graph.get_node_type(node_handle);
			if (type != e_native_module_graph_node_type::k_indexed_input
				&& type != e_native_module_graph_node_type::k_indexed_output
				&& type != e_native_module_graph_node_type::k_input) {
				m_native_module_graph.remove_node(node_handle);
			}
		}
	}
}

bool c_native_module_graph_optimizer::try_optimize_node(h_graph_node node_handle, bool &did_optimize_out) {
	did_optimize_out = false;

	if (m_native_module_graph.get_node_type(node_handle) != e_native_module_graph_node_type::k_native_module_call) {
		// The only type of node we can optimize are native module nodes
		return true;
	}

	// First, see if we can call the native module. This would be unusual because we attempt to fold constants when
	// initially building the graph, but it's possible for an optimization rule to convert a non-constant into a
	// constant. An example of this is multiplying by 0.
	int32 output_latency;
	if (!try_call_native_module(
		m_context,
		m_graph_trimmer,
		m_instrument_globals,
		node_handle,
		0, // No latency at optimization time, we've already resolved latency compensation
		s_compiler_source_location(), // $TODO $COMPILER we could add "compiler context" pointer to graph nodes
		did_optimize_out,
		output_latency)) {
		return false;
	}

	if (did_optimize_out) {
		return true;
	}

	// Try to apply an optimization rule
	if (m_optimization_rule_applicator.try_apply_optimization_rule(node_handle).is_valid()) {
		did_optimize_out = true;
		return true;
	}

	return true;
}

bool c_native_module_graph_optimizer::deduplicate_nodes() {
	bool any_changes = deduplicate_constants();
	any_changes |= deduplicate_arrays_and_native_modules();
	return any_changes;
}

bool c_native_module_graph_optimizer::deduplicate_constants() {
	bool any_changes = false;
	std::unordered_map<real32, h_graph_node> deduplicated_reals;
	std::unordered_map<bool, h_graph_node> deduplicated_bools;
	std::unordered_map<std::string, h_graph_node> deduplicated_strings;

	for (h_graph_node node_handle : m_native_module_graph.iterate_nodes()) {
		if (m_native_module_graph.get_node_type(node_handle) != e_native_module_graph_node_type::k_constant) {
			continue;
		}

		c_native_module_qualified_data_type type = m_native_module_graph.get_node_data_type(node_handle);
		h_graph_node deduplicated_node_handle = h_graph_node::invalid();
		switch (type.get_primitive_type()) {
		case e_native_module_primitive_type::k_real:
		{
			real32 value = m_native_module_graph.get_constant_node_real_value(node_handle);
			auto iter = deduplicated_reals.find(value);
			if (iter == deduplicated_reals.end()) {
				deduplicated_reals.insert(std::make_pair(value, node_handle));
			} else {
				deduplicated_node_handle = iter->second;
			}
			break;
		}

		case e_native_module_primitive_type::k_bool:
		{
			bool value = m_native_module_graph.get_constant_node_bool_value(node_handle);
			auto iter = deduplicated_bools.find(value);
			if (iter == deduplicated_bools.end()) {
				deduplicated_bools.insert(std::make_pair(value, node_handle));
			} else {
				deduplicated_node_handle = iter->second;
			}
			break;
		}

		case e_native_module_primitive_type::k_string:
		{
			std::string value = m_native_module_graph.get_constant_node_string_value(node_handle);
			auto iter = deduplicated_strings.find(value);
			if (iter == deduplicated_strings.end()) {
				deduplicated_strings.insert(std::make_pair(value, node_handle));
			} else {
				deduplicated_node_handle = iter->second;
			}
			break;
		}

		default:
			wl_unreachable();
		}

		if (deduplicated_node_handle.is_valid()) {
			// Redirect node_handle's outputs as outputs of deduplicated_node_handle
			remap_outputs(node_handle, deduplicated_node_handle);
			any_changes = true;
		}
	}

	remove_unused_nodes();
	return any_changes;
}

bool c_native_module_graph_optimizer::deduplicate_arrays_and_native_modules() {
	// Find any module calls or arrays with identical type and inputs and merge them. (The common factor here is
	// nodes which use indexed inputs.)
	// Currently n^2 but we could do better easily.

	// Repeat until the graph doesn't change. If we topologically sorted and deduplicated in that order, we wouldn't
	// need to run multiple passes.
	bool any_changes = false;
	bool graph_changed;
	do {
		graph_changed = false;
		std::unordered_set<h_graph_node> deduplicated_nodes;

		for (h_graph_node node_a_handle : m_native_module_graph.iterate_nodes()) {
			if (m_native_module_graph.get_node_type(node_a_handle) == e_native_module_graph_node_type::k_invalid) {
				continue;
			}

			if (!m_native_module_graph.does_node_use_indexed_inputs(node_a_handle)) {
				continue;
			}

			for (h_graph_node node_b_handle = m_native_module_graph.get_next_node(node_a_handle);
				node_b_handle.is_valid();
				node_b_handle = m_native_module_graph.get_next_node(node_b_handle)) {
				if (deduplicated_nodes.contains(node_b_handle)) {
					// We already deduplicated this node - it has no outputs
					continue;
				}

				if (m_native_module_graph.get_node_type(node_a_handle) !=
					m_native_module_graph.get_node_type(node_b_handle)) {
					continue;
				}

				e_native_module_graph_node_type node_type = m_native_module_graph.get_node_type(node_a_handle);
				if (node_type == e_native_module_graph_node_type::k_native_module_call) {
					// If native module indices don't match, skip
					if (m_native_module_graph.get_native_module_call_node_native_module_handle(node_a_handle) !=
						m_native_module_graph.get_native_module_call_node_native_module_handle(node_b_handle)) {
						continue;
					}
				} else if (node_type == e_native_module_graph_node_type::k_array) {
					wl_assert(m_native_module_graph.get_node_data_type(node_a_handle).is_array());

					// If array types don't match, skip
					if (m_native_module_graph.get_node_data_type(node_a_handle) !=
						m_native_module_graph.get_node_data_type(node_b_handle)) {
						continue;
					}

					wl_assert(m_native_module_graph.get_node_data_type(node_b_handle).is_array());

					// If array sizes don't match, skip
					if (m_native_module_graph.get_node_incoming_edge_count(node_a_handle) !=
						m_native_module_graph.get_node_incoming_edge_count(node_b_handle)) {
						continue;
					}
				} else {
					// No other node types use indexed inputs
					wl_unreachable();
				}

				size_t edge_count = m_native_module_graph.get_node_incoming_edge_count(node_a_handle);
				wl_assert(edge_count == m_native_module_graph.get_node_incoming_edge_count(node_b_handle));

				uint32 identical_inputs = true;
				for (size_t edge = 0; identical_inputs && edge < edge_count; edge++) {
					// Skip past the "input" nodes directly to the source
					h_graph_node source_node_a =
						m_native_module_graph.get_node_indexed_input_incoming_edge_handle(node_a_handle, edge, 0);
					h_graph_node source_node_b =
						m_native_module_graph.get_node_indexed_input_incoming_edge_handle(node_b_handle, edge, 0);

					identical_inputs = (source_node_a == source_node_b);
				}

				if (identical_inputs) {
					// Remap all outputs from node B to be outputs of node A
					remap_outputs(node_b_handle, node_a_handle);

					// Add to the list of deduplicated nodes so we don't bother looking at this node again
					deduplicated_nodes.insert(node_b_handle);

					graph_changed = true;
				}
			}
		}

		remove_unused_nodes();
		any_changes |= graph_changed;
	} while (graph_changed);

	return any_changes;
}

void c_native_module_graph_optimizer::remap_outputs(
	h_graph_node original_node_handle,
	h_graph_node remapped_node_handle) {
	wl_assert(m_native_module_graph.does_node_use_indexed_outputs(original_node_handle)
		== m_native_module_graph.does_node_use_indexed_outputs(remapped_node_handle));

	if (m_native_module_graph.does_node_use_indexed_outputs(original_node_handle)) {
		// Remap each indexed output
		size_t edge_count = m_native_module_graph.get_node_outgoing_edge_count(original_node_handle);
		wl_assert(edge_count == m_native_module_graph.get_node_outgoing_edge_count(remapped_node_handle));
		for (size_t edge = 0; edge < edge_count; edge++) {
			h_graph_node original_output_node =
				m_native_module_graph.get_node_outgoing_edge_handle(original_node_handle, edge);
			h_graph_node remapped_output_node =
				m_native_module_graph.get_node_outgoing_edge_handle(remapped_node_handle, edge);

			while (m_native_module_graph.get_node_outgoing_edge_count(original_output_node) > 0) {
				h_graph_node to_node_handle = m_native_module_graph.get_node_outgoing_edge_handle(
					original_output_node,
					m_native_module_graph.get_node_outgoing_edge_count(original_output_node) - 1);
				m_native_module_graph.remove_edge(original_output_node, to_node_handle);
				m_native_module_graph.add_edge(remapped_output_node, to_node_handle);
			}
		}
	} else {
		// Directly remap outputs
		while (m_native_module_graph.get_node_outgoing_edge_count(original_node_handle) > 0) {
			h_graph_node to_node_handle = m_native_module_graph.get_node_outgoing_edge_handle(
				original_node_handle,
				m_native_module_graph.get_node_outgoing_edge_count(original_node_handle) - 1);
			m_native_module_graph.remove_edge(original_node_handle, to_node_handle);
			m_native_module_graph.add_edge(remapped_node_handle, to_node_handle);
		}
	}
}
