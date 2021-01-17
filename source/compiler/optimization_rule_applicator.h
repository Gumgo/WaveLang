#pragma once

#include "common/common.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module_registry.h"

#include <stack>

class c_optimization_rule_applicator {
public:
	c_optimization_rule_applicator(c_execution_graph &execution_graph);
	bool try_apply_optimization_rule(h_graph_node node_handle, uint32 rule_index);

private:
	// Used to determine if the rule matches
	struct s_match_state {
		h_graph_node current_node_handle;
		h_graph_node current_node_output_handle;
		uint32 next_input_index;
	};

	bool has_more_inputs(const s_match_state &match_state) const;
	h_graph_node follow_next_input(s_match_state &match_state, h_graph_node &output_node_handle_out);

	bool try_to_match_source_pattern();
	bool handle_source_native_module_symbol_match(h_native_module native_module_handle) const;
	bool handle_source_value_symbol_match(
		const s_native_module_optimization_symbol &symbol,
		h_graph_node node_handle,
		h_graph_node output_node_handle);

	void store_match(const s_native_module_optimization_symbol &symbol, h_graph_node node_handle);
	h_graph_node load_match(const s_native_module_optimization_symbol &symbol) const;

	h_graph_node build_target_pattern();
	h_graph_node handle_target_value_symbol_match(const s_native_module_optimization_symbol &symbol);
	void reroute_source_to_target(h_graph_node target_root_node_handle);
	void transfer_outputs(h_graph_node destination_handle, h_graph_node source_handle);

	c_execution_graph &m_execution_graph;
	h_graph_node m_source_root_node_handle;
	const s_native_module_optimization_rule *m_rule = nullptr;

	std::stack<s_match_state> m_match_state_stack;

	// Node indices for the values and constants we've matched in the source pattern
	s_static_array<h_graph_node, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_variable_node_handles;
	s_static_array<h_graph_node, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_constant_node_handles;
};
