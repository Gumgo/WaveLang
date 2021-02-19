#include "compiler/optimization_rule_applicator.h"

c_optimization_rule_applicator::c_optimization_rule_applicator(c_native_module_graph &native_module_graph)
	: m_native_module_graph(native_module_graph) {
	// Build the match stage tree
	for (h_native_module_optimization_rule rule_handle : c_native_module_registry::iterate_optimization_rules()) {
		const s_native_module_optimization_rule &rule = c_native_module_registry::get_optimization_rule(rule_handle);

		size_t stage_index = k_invalid_stage_index;
		for (const s_native_module_optimization_symbol &symbol : rule.source.symbols) {
			if (!symbol.is_valid()) {
				wl_assert(stage_index != k_invalid_stage_index);
				break;
			}

			if (stage_index == k_invalid_stage_index) {
				// The first symbol should always be a native module
				wl_assert(symbol.type == e_native_module_optimization_symbol_type::k_native_module);
				h_native_module native_module_handle =
					c_native_module_registry::get_native_module_handle(symbol.data.native_module_uid);

				auto iter = m_initial_stages.find(native_module_handle);
				if (iter == m_initial_stages.end()) {
					stage_index = m_stages.size();
					m_stages.push_back({});
					m_initial_stages.insert(std::pair(native_module_handle, stage_index));
				} else {
					stage_index = iter->second;
				}

				continue;
			}

			bool found = false;
			size_t last_advance_index = k_invalid_advance_index;
			size_t advance_index = m_stages[stage_index].first_advance_index;
			while (advance_index != k_invalid_advance_index) {
				last_advance_index = advance_index;
				const s_advance &advance = m_advances[advance_index];

				if (*advance.symbol_to_match == symbol) {
					stage_index = advance.stage_index;
					found = true;
					break;
				}

				advance_index = advance.next_advance_index;
			}

			if (found) {
				continue;
			}

			// Add a new stage and advance
			size_t new_stage_index = m_stages.size();
			size_t new_advance_index = m_advances.size();
			if (last_advance_index == k_invalid_advance_index) {
				m_stages[stage_index].first_advance_index = new_advance_index;
			} else {
				m_advances[last_advance_index].next_advance_index = new_advance_index;
			}

			m_advances.push_back({ &symbol, new_stage_index, k_invalid_advance_index });
			m_stages.push_back({});

			stage_index = new_stage_index;
		}

		wl_assert(stage_index != k_invalid_stage_index);

		// If we hit this, it means we have two identical rules
		wl_assert(!m_stages[stage_index].resolved_rule_handle.is_valid());
		m_stages[stage_index].resolved_rule_handle = rule_handle;
	}
}

h_native_module_optimization_rule c_optimization_rule_applicator::try_apply_optimization_rule(
	h_graph_node node_handle) {
	if (m_native_module_graph.get_node_type(node_handle) != e_native_module_graph_node_type::k_native_module_call) {
		return h_native_module_optimization_rule::invalid();
	}

	h_native_module native_module_handle =
		m_native_module_graph.get_native_module_call_node_native_module_handle(node_handle);
	m_upsample_factor =
		m_native_module_graph.get_native_module_call_node_upsample_factor(node_handle);

	// Find our starting stage
	auto first_stage_iter = m_initial_stages.find(native_module_handle);
	if (first_stage_iter == m_initial_stages.end()) {
		// No optimization rules exist that start with this native module
		return h_native_module_optimization_rule::invalid();
	}

	size_t first_stage_index = first_stage_iter->second;
	wl_assert(!m_stages[first_stage_index].resolved_rule_handle.is_valid());
	s_graph_location first_graph_location{ node_handle, 0, k_invalid_match_state_index };
	m_match_stack.push_back({ m_stages[first_stage_index].first_advance_index, false, false, first_graph_location });

	while (!m_match_stack.empty()) {
		s_match_state &match_state = m_match_stack.back();

		if (match_state.advance_index == k_invalid_advance_index) {
			// We tried all advances from this stage and they all failed, so pop it
			m_match_stack.pop_back();
		} else if (match_state.match_attempted) {
			// We have already tried to match with this advance and failed so move onto the next advance
			// First revert any changes this advance may have caused
			if (match_state.match_succeeded) {
				revert_advance();
			}

			// Try the next advance from this stage
			match_state.advance_index = m_advances[match_state.advance_index].next_advance_index;
			match_state.match_attempted = false;
			match_state.match_succeeded = false;
		} else {
			// We haven't tried to match with this advance yet
			wl_assert(!match_state.match_succeeded);

			s_graph_location new_graph_location;
			if (try_advance(new_graph_location)) {
				// Success - move to the next stage
				match_state.match_succeeded = true;
				const s_advance &advance = m_advances[match_state.advance_index];
				const s_match_stage &stage = m_stages[advance.stage_index];

				if (stage.resolved_rule_handle.is_valid()) {
					// Success! We've matched with a rule. Now apply it.
					h_graph_node target_root_node_handle = build_target_pattern(stage.resolved_rule_handle);
					reroute_source_to_target(node_handle, target_root_node_handle);

					// Clear state and return the rule handle
					m_matched_values.clear();
					m_match_stack.clear();
					m_upsample_factor = 0;
					return stage.resolved_rule_handle;
				} else {
					// If this stage doesn't resolve a rule, it must have more stages to advance to
					wl_assert(stage.first_advance_index != k_invalid_advance_index);
					m_match_stack.push_back({ stage.first_advance_index, false, false, new_graph_location });
				}
			}
		}
	}

	// Our stacks should be empty
	wl_assert(m_matched_values.empty());
	wl_assert(m_match_stack.empty());
	m_upsample_factor = 0;
	return h_native_module_optimization_rule::invalid();
}

h_graph_node c_optimization_rule_applicator::get_next_input(
	const s_graph_location &graph_location,
	h_graph_node &value_node_handle_out) const {
	value_node_handle_out = h_graph_node::invalid();
	size_t input_count = m_native_module_graph.get_node_incoming_edge_count(graph_location.current_node_handle);
	if (graph_location.next_input_index >= input_count) {
		return h_graph_node::invalid();
	}

	// Grab the input
	h_graph_node new_node_handle = m_native_module_graph.get_node_indexed_input_incoming_edge_handle(
		graph_location.current_node_handle,
		graph_location.next_input_index,
		0);
	value_node_handle_out = new_node_handle;

	if (m_native_module_graph.get_node_type(new_node_handle) == e_native_module_graph_node_type::k_indexed_output) {
		// We need to advance an additional node for the case (module <- input <- output <- module)
		new_node_handle = m_native_module_graph.get_node_incoming_edge_handle(new_node_handle, 0);
	}

	return new_node_handle;
}

bool c_optimization_rule_applicator::try_advance(s_graph_location &new_graph_location_out) {
	new_graph_location_out = { h_graph_node::invalid(), 0, k_invalid_match_state_index };

	size_t match_state_index = m_match_stack.size() - 1;
	s_match_state &match_state = m_match_stack.back();
	wl_assert(!match_state.match_attempted);
	wl_assert(!match_state.match_succeeded);
	match_state.match_attempted = true;

	const s_native_module_optimization_symbol &symbol = *m_advances[match_state.advance_index].symbol_to_match;

	h_graph_node value_node_handle;
	h_graph_node next_node_handle = get_next_input(match_state.graph_location, value_node_handle);

	switch (symbol.type) {
	case e_native_module_optimization_symbol_type::k_native_module:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		e_native_module_graph_node_type next_node_type = m_native_module_graph.get_node_type(next_node_handle);
		if (next_node_type != e_native_module_graph_node_type::k_native_module_call) {
			return false;
		}

		wl_assert(symbol.data.native_module_uid.is_valid());
		h_native_module native_module_handle =
			c_native_module_registry::get_native_module_handle(symbol.data.native_module_uid);

		h_native_module next_node_native_module_handle =
			m_native_module_graph.get_native_module_call_node_native_module_handle(next_node_handle);
		if (next_node_native_module_handle != native_module_handle) {
			return false;
		}

		// We don't support optimization rules with mixed upsample factors (you can't even specify upsample factor in
		// the rule syntax)
		wl_assert(
			m_native_module_graph.get_native_module_call_node_upsample_factor(next_node_handle) == m_upsample_factor);

		new_graph_location_out = { next_node_handle, 0, match_state_index };
		return true;
	}

	case e_native_module_optimization_symbol_type::k_native_module_end:
	{
		// We expect that if we were able to match and enter a module, we should also match when leaving. If not, it
		// means the rule does not match the definition of the module (e.g. too few arguments).
		wl_assert(!next_node_handle.is_valid());

		if (match_state.graph_location.previous_match_state_index != k_invalid_match_state_index) {
			// We're at the end of this native module call, so jump back up to the previous call's next input
			new_graph_location_out =
				m_match_stack[match_state.graph_location.previous_match_state_index].graph_location;
			new_graph_location_out.next_input_index++;
		} else {
			// We're at the root native module call that this rule was invoked on - nothing to do
		}

		return true;
	}

	case e_native_module_optimization_symbol_type::k_variable:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match anything except for constants
		e_native_module_graph_node_type node_type = m_native_module_graph.get_node_type(next_node_handle);
		if (node_type == e_native_module_graph_node_type::k_array) {
			e_native_module_data_mutability data_mutability =
				m_native_module_graph.get_node_data_type(next_node_handle).get_data_mutability();
			if (data_mutability == e_native_module_data_mutability::k_constant) {
				return false;
			}
		} else if (node_type == e_native_module_graph_node_type::k_constant) {
			return false;
		}

		m_matched_values.push_back(value_node_handle);
		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;
	}

	case e_native_module_optimization_symbol_type::k_constant:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match only constants
		e_native_module_graph_node_type node_type = m_native_module_graph.get_node_type(next_node_handle);
		if (node_type == e_native_module_graph_node_type::k_array) {
			e_native_module_data_mutability data_mutability =
				m_native_module_graph.get_node_data_type(next_node_handle).get_data_mutability();
			if (data_mutability != e_native_module_data_mutability::k_constant) {
				return false;
			}
		} else if (node_type != e_native_module_graph_node_type::k_constant) {
			return false;
		}

		m_matched_values.push_back(value_node_handle);
		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;
	}

	case e_native_module_optimization_symbol_type::k_variable_or_constant:
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match either variable or constant (so, everything)
		m_matched_values.push_back(value_node_handle);
		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;

	case e_native_module_optimization_symbol_type::k_back_reference:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match only the exact node handle we've matched before
		if (m_matched_values[symbol.data.index] != value_node_handle) {
			return false;
		}

		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;
	}

	case e_native_module_optimization_symbol_type::k_real_value:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match only constants with the given value
		// get_constant_node_<type>_value will assert that there's no type mismatch
		if (m_native_module_graph.get_node_type(next_node_handle) != e_native_module_graph_node_type::k_constant
			|| symbol.data.real_value != m_native_module_graph.get_constant_node_real_value(next_node_handle)) {
			return false;
		}

		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;
	}

	case e_native_module_optimization_symbol_type::k_bool_value:
	{
		wl_assertf(next_node_handle.is_valid(), "Rule inputs don't match native module definition");

		// Match only constants with the given value
		// get_constant_node_<type>_value will assert that there's no type mismatch
		if (m_native_module_graph.get_node_type(next_node_handle) != e_native_module_graph_node_type::k_constant
			|| symbol.data.bool_value != m_native_module_graph.get_constant_node_bool_value(next_node_handle)) {
			return false;
		}

		new_graph_location_out = match_state.graph_location;
		new_graph_location_out.next_input_index++;
		return true;
	}

	default:
		wl_unreachable();
		return false;
	}
}

void c_optimization_rule_applicator::revert_advance() {
	size_t match_state_index = m_match_stack.size() - 1;
	const s_match_state &match_state = m_match_stack.back();
	wl_assert(match_state.match_attempted);
	wl_assert(match_state.match_succeeded);

	const s_native_module_optimization_symbol &symbol = *m_advances[match_state.advance_index].symbol_to_match;

	switch (symbol.type) {
	case e_native_module_optimization_symbol_type::k_native_module:
		return;

	case e_native_module_optimization_symbol_type::k_native_module_end:
		return;

	case e_native_module_optimization_symbol_type::k_variable:
	case e_native_module_optimization_symbol_type::k_constant:
	case e_native_module_optimization_symbol_type::k_variable_or_constant:
		m_matched_values.pop_back();
		break;

	case e_native_module_optimization_symbol_type::k_back_reference:
	case e_native_module_optimization_symbol_type::k_real_value:
	case e_native_module_optimization_symbol_type::k_bool_value:
		return;

	default:
		wl_unreachable();
	}
}

h_graph_node c_optimization_rule_applicator::build_target_pattern(h_native_module_optimization_rule rule_handle) {
	wl_assert(m_target_graph_location_stack.empty());
	const s_native_module_optimization_rule &rule = c_native_module_registry::get_optimization_rule(rule_handle);

	// Replace the matched source pattern nodes with the ones defined by the target in the rule
	h_graph_node root_node_handle = h_graph_node::invalid();

	IF_ASSERTS_ENABLED(bool should_be_done = false;)
	IF_ASSERTS_ENABLED(h_graph_node value_node_handle_unused;)
	for (const s_native_module_optimization_symbol &symbol : rule.target.symbols) {
		if (!symbol.is_valid()) {
			break;
		}

		wl_assert(!should_be_done);

		switch (symbol.type) {
		case e_native_module_optimization_symbol_type::k_native_module:
		{
			h_native_module native_module_handle =
				c_native_module_registry::get_native_module_handle(symbol.data.native_module_uid);
			h_graph_node node_handle =
				m_native_module_graph.add_native_module_call_node(native_module_handle, m_upsample_factor);

			// We currently only allow a single outgoing edge, due to the way we express rules (out arguments aren't
			// currently supported)
			wl_assert(m_native_module_graph.get_node_outgoing_edge_count(node_handle) == 1);

			if (!root_node_handle.is_valid()) {
				// This is the root node of the target pattern
				wl_assert(m_target_graph_location_stack.empty());
				root_node_handle = node_handle;
			} else {
				s_graph_location &current_graph_location = m_target_graph_location_stack.top();
				h_graph_node input_node_handle = m_native_module_graph.get_node_incoming_edge_handle(
					current_graph_location.current_node_handle,
					current_graph_location.next_input_index);
				h_graph_node output_node_handle = m_native_module_graph.get_node_outgoing_edge_handle(
					node_handle,
					0);
				m_native_module_graph.add_edge(output_node_handle, input_node_handle);
				current_graph_location.next_input_index++;
			}

			m_target_graph_location_stack.push({ node_handle, 0, k_invalid_match_state_index });
			break;
		}

		case e_native_module_optimization_symbol_type::k_native_module_end:
		{
			wl_assert(!m_target_graph_location_stack.empty());
			// We expect that if we were able to match and enter a module, we should also match when leaving. If not, it
			// means the rule does not match the definition of the module (e.g. too few arguments).
			wl_assert(!get_next_input(m_target_graph_location_stack.top(), value_node_handle_unused).is_valid());
			IF_ASSERTS_ENABLED(should_be_done = m_target_graph_location_stack.size() == 1;)
			m_target_graph_location_stack.pop();
			break;
		}

		case e_native_module_optimization_symbol_type::k_variable:
		case e_native_module_optimization_symbol_type::k_constant:
		case e_native_module_optimization_symbol_type::k_variable_or_constant:
			// These should not occur in target patterns
			wl_halt();
			break;

		case e_native_module_optimization_symbol_type::k_back_reference:
		case e_native_module_optimization_symbol_type::k_real_value:
		case e_native_module_optimization_symbol_type::k_bool_value:
		{
			h_graph_node value_node_handle;
			switch (symbol.type) {
			case e_native_module_optimization_symbol_type::k_back_reference:
				value_node_handle = m_matched_values[symbol.data.index];
				break;

			case e_native_module_optimization_symbol_type::k_real_value:
				value_node_handle = m_native_module_graph.add_constant_node(symbol.data.real_value);
				break;

			case e_native_module_optimization_symbol_type::k_bool_value:
				value_node_handle = m_native_module_graph.add_constant_node(symbol.data.bool_value);
				break;

			default:
				wl_unreachable();
			}

			wl_assert(value_node_handle.is_valid());

			// Hook up this input and advance
			if (!root_node_handle.is_valid()) {
				wl_assert(m_target_graph_location_stack.empty());
				root_node_handle = value_node_handle;
				IF_ASSERTS_ENABLED(should_be_done = true;)
			} else {
				wl_assert(!m_target_graph_location_stack.empty());
				s_graph_location &current_graph_location = m_target_graph_location_stack.top();
				h_graph_node input_node_handle = m_native_module_graph.get_node_incoming_edge_handle(
					current_graph_location.current_node_handle,
					current_graph_location.next_input_index);
				current_graph_location.next_input_index++;
				m_native_module_graph.add_edge(value_node_handle, input_node_handle);
			}

			break;
		}

		default:
			wl_unreachable();
		}
	}

	wl_assert(m_target_graph_location_stack.empty());
	return root_node_handle;
}

void c_optimization_rule_applicator::reroute_source_to_target(
	h_graph_node source_root_node_handle,
	h_graph_node target_root_node_handle) {
	// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
	// deleting the old set of nodes, they will automatically be cleaned up later.

	// We currently only support modules with a single output
	wl_assert(m_native_module_graph.get_node_outgoing_edge_count(source_root_node_handle) == 1);
	h_graph_node old_output_node =
		m_native_module_graph.get_node_outgoing_edge_handle(source_root_node_handle, 0);

	switch (m_native_module_graph.get_node_type(target_root_node_handle)) {
	case e_native_module_graph_node_type::k_native_module_call:
	{
		wl_assert(m_native_module_graph.get_node_outgoing_edge_count(target_root_node_handle) == 1);
		h_graph_node new_output_node =
			m_native_module_graph.get_node_outgoing_edge_handle(target_root_node_handle, 0);
		transfer_outputs(new_output_node, old_output_node);
		break;
	}

	case e_native_module_graph_node_type::k_constant:
	case e_native_module_graph_node_type::k_array:
	case e_native_module_graph_node_type::k_indexed_output:
		transfer_outputs(target_root_node_handle, old_output_node);
		break;

	case e_native_module_graph_node_type::k_temporary_reference:
		// These should have all been removed
		wl_unreachable();
		break;

	default:
		wl_unreachable();
	}
}

void c_optimization_rule_applicator::transfer_outputs(
	h_graph_node destination_handle,
	h_graph_node source_handle) {
	// Redirect the inputs of input_node directly to the outputs of output_node
	while (m_native_module_graph.get_node_outgoing_edge_count(source_handle) > 0) {
		h_graph_node to_node_handle = m_native_module_graph.get_node_outgoing_edge_handle(source_handle, 0);
		m_native_module_graph.remove_edge(source_handle, to_node_handle);
		m_native_module_graph.add_edge(destination_handle, to_node_handle);
	}
}
