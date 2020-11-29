#pragma once

#include "common/common.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module_registry.h"

#include <stack>

class c_optimization_rule_applicator {
public:
	c_optimization_rule_applicator(c_execution_graph &execution_graph);
	bool try_apply_optimization_rule(c_node_reference node_reference, uint32 rule_index);

private:
	// Used to determine if the rule matches
	struct s_match_state {
		c_node_reference current_node_reference;
		c_node_reference current_node_output_reference;
		uint32 next_input_index;
	};

	bool has_more_inputs(const s_match_state &match_state) const;
	c_node_reference follow_next_input(s_match_state &match_state, c_node_reference &output_node_reference_out);

	bool try_to_match_source_pattern();
	bool handle_source_native_module_symbol_match(h_native_module native_module_handle) const;
	bool handle_source_value_symbol_match(
		const s_native_module_optimization_symbol &symbol,
		c_node_reference node_reference,
		c_node_reference output_node_reference);

	void store_match(const s_native_module_optimization_symbol &symbol, c_node_reference node_reference);
	c_node_reference load_match(const s_native_module_optimization_symbol &symbol) const;

	c_node_reference build_target_pattern();
	c_node_reference handle_target_value_symbol_match(const s_native_module_optimization_symbol &symbol);
	void reroute_source_to_target(c_node_reference target_root_node_reference);
	void transfer_outputs(c_node_reference destination_reference, c_node_reference source_reference);

	c_execution_graph &m_execution_graph;
	c_node_reference m_source_root_node_reference;
	const s_native_module_optimization_rule *m_rule = nullptr;

	std::stack<s_match_state> m_match_state_stack;

	// Node indices for the values and constants we've matched in the source pattern
	s_static_array<c_node_reference, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_variable_node_references;
	s_static_array<c_node_reference, s_native_module_optimization_symbol::k_max_matched_symbols>
		m_matched_constant_node_references;
};
