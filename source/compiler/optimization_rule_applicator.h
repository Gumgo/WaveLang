#pragma once

#include "common/common.h"

#include "instrument/native_module_graph.h"
#include "instrument/native_module_registry.h"

#include <stack>
#include <unordered_map>
#include <vector>

class c_optimization_rule_applicator {
public:
	c_optimization_rule_applicator(c_native_module_graph &native_module_graph);

	// Returns the handle of the rule applied or invalid if no rule was applied
	h_native_module_optimization_rule try_apply_optimization_rule(h_graph_node node_handle);

private:
	static constexpr size_t k_invalid_stage_index = static_cast<size_t>(-1);
	static constexpr size_t k_invalid_advance_index = static_cast<size_t>(-1);
	static constexpr size_t k_invalid_match_state_index = static_cast<size_t>(-1);

	struct s_match_stage {
		// Rule handle if this stage completes a rule
		h_native_module_optimization_rule resolved_rule_handle = h_native_module_optimization_rule::invalid();

		// List of advances if this stage doesn't resolve a rule
		size_t first_advance_index = k_invalid_advance_index;
	};

	struct s_advance {
		const s_native_module_optimization_symbol *symbol_to_match;	// The symbol to match in order to advance
		size_t stage_index;											// Stage to advance to
		size_t next_advance_index;									// Index of the next advance in this node's list
	};

	struct s_graph_location {
		h_graph_node current_node_handle;	// Current node in the graph
		uint32 next_input_index;			// The next input to visit
		size_t previous_match_state_index;	// Index of the match state containing the previous location
	};

	struct s_match_state {
		size_t advance_index;				// The index of the advance to try
		bool match_attempted;				// Whether this advance has been attempted
		bool match_succeeded;				// Whether this advance was successful
		s_graph_location graph_location;	// Current graph location to advance from
	};

	// Follows the next native module input. The node that produces the input value is returned (which may be a native
	// module node) and the input value itself is stored in value_node_handle_out (which may be an indexed output node).
	// If there are no more inputs, invalid is returned.
	h_graph_node get_next_input(const s_graph_location &graph_location, h_graph_node &value_node_handle_out) const;

	// Attempts to follow the advance on the top of the match state stack. If successful, matched values are pushed onto
	// the stack and a new graph location is stored in new_graph_location_out.
	bool try_advance(s_graph_location &new_graph_location_out);

	// Pops any matched values stored from a successful advance
	void revert_advance();

	h_graph_node build_target_pattern(h_native_module_optimization_rule rule_handle);
	void reroute_source_to_target(h_graph_node source_root_node_handle, h_graph_node target_root_node_handle);
	void transfer_outputs(h_graph_node destination_handle, h_graph_node source_handle);

	c_native_module_graph &m_native_module_graph;
	std::unordered_map<h_native_module, size_t> m_initial_stages;
	std::vector<s_match_stage> m_stages;
	std::vector<s_advance> m_advances;

	std::vector<h_graph_node> m_matched_values;
	std::vector<s_match_state> m_match_stack;
	uint32 m_upsample_factor = 0;

	// Used to build up the target - previous_match_state_index is unused (previous is just the previous stack element)
	std::stack<s_graph_location> m_target_graph_location_stack;
};
