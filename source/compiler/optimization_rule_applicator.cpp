#include "compiler/optimization_rule_applicator.h"

c_optimization_rule_applicator::c_optimization_rule_applicator(c_native_module_graph &native_module_graph)
	: m_native_module_graph(native_module_graph) {}

bool c_optimization_rule_applicator::try_apply_optimization_rule(h_graph_node node_handle, uint32 rule_index) {
	m_source_root_node_handle = node_handle;
	m_rule = &c_native_module_registry::get_optimization_rule(rule_index);

	// Initially we haven't matched any values

	for (size_t index = 0; index < m_matched_variable_node_handles.get_count(); index++) {
		m_matched_variable_node_handles[index] = h_graph_node::invalid();
	}

	for (size_t index = 0; index < m_matched_constant_node_handles.get_count(); index++) {
		m_matched_constant_node_handles[index] = h_graph_node::invalid();
	}

	if (try_to_match_source_pattern()) {
		h_graph_node target_root_node_handle = build_target_pattern();
		reroute_source_to_target(target_root_node_handle);

		wl_assert(m_match_state_stack.empty());
		return true;
	}

	while (!m_match_state_stack.empty()) {
		m_match_state_stack.pop();
	}

	return false;
}

bool c_optimization_rule_applicator::has_more_inputs(const s_match_state &match_state) const {
	size_t input_count = m_native_module_graph.get_node_incoming_edge_count(match_state.current_node_handle);
	return match_state.next_input_index < input_count;
}

h_graph_node c_optimization_rule_applicator::follow_next_input(
	s_match_state &match_state,
	h_graph_node &output_node_handle_out) {
	output_node_handle_out = h_graph_node::invalid();
	wl_assert(has_more_inputs(match_state));

	// Grab the input
	h_graph_node new_node_handle = m_native_module_graph.get_node_indexed_input_incoming_edge_handle(
		match_state.current_node_handle,
		match_state.next_input_index,
		0);

	if (m_native_module_graph.get_node_type(new_node_handle) == e_native_module_graph_node_type::k_indexed_output) {
		// We need to advance an additional node for the case (module <- input <- output <- module)
		output_node_handle_out = new_node_handle;
		new_node_handle = m_native_module_graph.get_node_incoming_edge_handle(new_node_handle, 0);
	}

	match_state.next_input_index++;
	return new_node_handle;
}

bool c_optimization_rule_applicator::try_to_match_source_pattern() {
	size_t symbol_count;
	for (symbol_count = 0;
		symbol_count < m_rule->source.symbols.get_count() && m_rule->source.symbols[symbol_count].is_valid();
		symbol_count++);

	m_upsample_factor = 0;

	IF_ASSERTS_ENABLED(bool should_be_done = false);
	for (size_t symbol_index = 0; symbol_index < symbol_count; symbol_index++) {
		wl_assert(!should_be_done);

		const s_native_module_optimization_symbol &symbol = m_rule->source.symbols[symbol_index];
		switch (symbol.type) {
		case e_native_module_optimization_symbol_type::k_native_module:
		{
			wl_assert(symbol.data.native_module_uid.is_valid());
			h_native_module native_module_handle =
				c_native_module_registry::get_native_module_handle(symbol.data.native_module_uid);

			if (m_match_state_stack.empty()) {
				// This is the beginning of the source pattern, so add the initial module call to the stack
				s_match_state initial_state;
				initial_state.current_node_output_handle = h_graph_node::invalid();
				initial_state.current_node_handle = m_source_root_node_handle;
				initial_state.next_input_index = 0;
				m_match_state_stack.push(initial_state);
			} else {
				// Try to advance to the next input
				s_match_state &current_state = m_match_state_stack.top();
				wl_assertf(has_more_inputs(current_state), "Rule inputs don't match native module definition");

				s_match_state new_state;
				new_state.current_node_output_handle = h_graph_node::invalid();
				new_state.current_node_handle =
					follow_next_input(current_state, new_state.current_node_output_handle);
				new_state.next_input_index = 0;
				m_match_state_stack.push(new_state);
			}

			// The module described by the rule should match the top of the state stack
			if (!handle_source_native_module_symbol_match(native_module_handle)) {
				return false;
			}

			uint32 upsample_factor = m_native_module_graph.get_native_module_call_node_upsample_factor(
				m_match_state_stack.top().current_node_handle);
			if (m_upsample_factor == 0) {
				m_upsample_factor = upsample_factor;
			} else {
				// We don't support optimization rules with mixed upsample factors (you can't even specify upsample
				// factor in the rule syntax)
				wl_assert(upsample_factor == m_upsample_factor);
			}

			break;
		}

		case e_native_module_optimization_symbol_type::k_native_module_end:
		{
			wl_assert(!m_match_state_stack.empty());
			// We expect that if we were able to match and enter a module, we should also match when leaving. If not, it
			// means the rule does not match the definition of the module (e.g. too few arguments).
			wl_assert(!has_more_inputs(m_match_state_stack.top()));
			IF_ASSERTS_ENABLED(should_be_done = (m_match_state_stack.size() == 1));
			m_match_state_stack.pop();
			break;
		}

		case e_native_module_optimization_symbol_type::k_variable:
		case e_native_module_optimization_symbol_type::k_constant:
		case e_native_module_optimization_symbol_type::k_real_value:
		case e_native_module_optimization_symbol_type::k_bool_value:
		{
			// Try to advance to the next input
			wl_assert(!m_match_state_stack.empty());
			s_match_state &current_state = m_match_state_stack.top();
			wl_assertf(has_more_inputs(current_state), "Rule inputs don't match native module definition");

			h_graph_node new_output_node_handle = h_graph_node::invalid();
			h_graph_node new_node_handle = follow_next_input(current_state, new_output_node_handle);

			if (!handle_source_value_symbol_match(symbol, new_node_handle, new_output_node_handle)) {
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

bool c_optimization_rule_applicator::handle_source_native_module_symbol_match(
	h_native_module native_module_handle) const {
	const s_match_state &current_state = m_match_state_stack.top();
	h_graph_node node_handle = current_state.current_node_handle;

	// It's a match if the current node is a native module of the same index
	return (m_native_module_graph.get_node_type(node_handle) == e_native_module_graph_node_type::k_native_module_call)
		&& (m_native_module_graph.get_native_module_call_node_native_module_handle(node_handle) ==
			native_module_handle);
}

bool c_optimization_rule_applicator::handle_source_value_symbol_match(
	const s_native_module_optimization_symbol &symbol,
	h_graph_node node_handle,
	h_graph_node output_node_handle) {
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
		// Match anything except for constants
		e_native_module_graph_node_type node_type = m_native_module_graph.get_node_type(node_handle);
		if (node_type == e_native_module_graph_node_type::k_array) {
			e_native_module_data_mutability data_mutability =
				m_native_module_graph.get_node_data_type(node_handle).get_data_mutability();
			if (data_mutability == e_native_module_data_mutability::k_constant) {
				return false;
			}
		} else if (node_type == e_native_module_graph_node_type::k_constant) {
			return false;
		}

		// If this is a module node, it means we had to pass through an output node to get here. Store the output node
		// as the match, because that's what inputs will be hooked up to.
		store_match(symbol, output_node_handle.is_valid() ? output_node_handle : node_handle);
		return true;
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		// Match only constants
		e_native_module_graph_node_type node_type = m_native_module_graph.get_node_type(node_handle);
		if (node_type == e_native_module_graph_node_type::k_array) {
			e_native_module_data_mutability data_mutability =
				m_native_module_graph.get_node_data_type(node_handle).get_data_mutability();
			if (data_mutability != e_native_module_data_mutability::k_constant) {
				return false;
			}
		} else if (node_type != e_native_module_graph_node_type::k_constant) {
			return false;
		}

		store_match(symbol, node_handle);
		return true;
	} else {
		// Match only constants with the given value
		if (m_native_module_graph.get_node_type(node_handle) != e_native_module_graph_node_type::k_constant) {
			return false;
		}

		// get_constant_node_<type>_value will assert that there's no type mismatch
		if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
			return (symbol.data.real_value == m_native_module_graph.get_constant_node_real_value(node_handle));
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
			return (symbol.data.bool_value == m_native_module_graph.get_constant_node_bool_value(node_handle));
		} else {
			wl_unreachable();
			return false;
		}
	}
}

void c_optimization_rule_applicator::store_match(
	const s_native_module_optimization_symbol &symbol,
	h_graph_node node_handle) {
	wl_assert(node_handle.is_valid());
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
		wl_assert(!m_matched_variable_node_handles[symbol.data.index].is_valid());
		m_matched_variable_node_handles[symbol.data.index] = node_handle;
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		wl_assert(!m_matched_constant_node_handles[symbol.data.index].is_valid());
		m_matched_constant_node_handles[symbol.data.index] = node_handle;
	} else {
		wl_unreachable();
	}
}

h_graph_node c_optimization_rule_applicator::load_match(const s_native_module_optimization_symbol &symbol) const {
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
		wl_assert(m_matched_variable_node_handles[symbol.data.index].is_valid());
		return m_matched_variable_node_handles[symbol.data.index];
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		wl_assert(m_matched_constant_node_handles[symbol.data.index].is_valid());
		return m_matched_constant_node_handles[symbol.data.index];
	} else {
		wl_unreachable();
		return h_graph_node::invalid();
	}
}

h_graph_node c_optimization_rule_applicator::build_target_pattern() {
	// Replace the matched source pattern nodes with the ones defined by the target in the rule
	h_graph_node root_node_handle = h_graph_node::invalid();

	size_t symbol_count;
	for (symbol_count = 0;
		symbol_count < m_rule->target.symbols.get_count() && m_rule->target.symbols[symbol_count].is_valid();
		symbol_count++);

	IF_ASSERTS_ENABLED(bool should_be_done = false);
	for (size_t symbol_index = 0; symbol_index < symbol_count; symbol_index++) {
		wl_assert(!should_be_done);

		const s_native_module_optimization_symbol &symbol = m_rule->target.symbols[symbol_index];
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
				wl_assert(m_match_state_stack.empty());
				root_node_handle = node_handle;
			} else {
				s_match_state &current_state = m_match_state_stack.top();
				wl_assert(has_more_inputs(current_state));

				h_graph_node input_node_handle = m_native_module_graph.get_node_incoming_edge_handle(
					current_state.current_node_handle,
					current_state.next_input_index);
				h_graph_node output_node_handle = m_native_module_graph.get_node_outgoing_edge_handle(
					node_handle,
					0);
				m_native_module_graph.add_edge(output_node_handle, input_node_handle);
				current_state.next_input_index++;
			}

			s_match_state new_state;
			new_state.current_node_output_handle = h_graph_node::invalid();
			new_state.current_node_handle = node_handle;
			new_state.next_input_index = 0;
			m_match_state_stack.push(new_state);
			break;
		}

		case e_native_module_optimization_symbol_type::k_native_module_end:
		{
			wl_assert(!m_match_state_stack.empty());
			// We expect that if we were able to match and enter a module, we should also match when leaving If not, it
			// means the rule does not match the definition of the module (e.g. too few arguments).
			wl_assert(!has_more_inputs(m_match_state_stack.top()));
			IF_ASSERTS_ENABLED(should_be_done = m_match_state_stack.size() == 1);
			m_match_state_stack.pop();
			break;
		}

		case e_native_module_optimization_symbol_type::k_variable:
		case e_native_module_optimization_symbol_type::k_constant:
		case e_native_module_optimization_symbol_type::k_real_value:
		case e_native_module_optimization_symbol_type::k_bool_value:
		{
			h_graph_node matched_node_handle = handle_target_value_symbol_match(symbol);

			// Hook up this input and advance
			if (!root_node_handle.is_valid()) {
				wl_assert(m_match_state_stack.empty());
				root_node_handle = matched_node_handle;
				IF_ASSERTS_ENABLED(should_be_done = true);
			} else {
				wl_assert(!m_match_state_stack.empty());
				s_match_state &current_state = m_match_state_stack.top();
				wl_assert(has_more_inputs(current_state));

				h_graph_node input_node_handle = m_native_module_graph.get_node_incoming_edge_handle(
					current_state.current_node_handle,
					current_state.next_input_index);
				current_state.next_input_index++;
				m_native_module_graph.add_edge(matched_node_handle, input_node_handle);
			}
			break;
		}

		default:
			wl_unreachable();
		}
	}

	wl_assert(m_match_state_stack.empty());
	return root_node_handle;
}

h_graph_node c_optimization_rule_applicator::handle_target_value_symbol_match(
	const s_native_module_optimization_symbol &symbol) {
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable
		|| symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		return load_match(symbol);
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
		// Create a constant node with this value
		return m_native_module_graph.add_constant_node(symbol.data.real_value);
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
		// Create a constant node with this value
		return m_native_module_graph.add_constant_node(symbol.data.bool_value);
	} else {
		wl_unreachable();
		return h_graph_node::invalid();
	}
}


void c_optimization_rule_applicator::reroute_source_to_target(h_graph_node target_root_node_handle) {
	// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
	// deleting the old set of nodes, they will automatically be cleaned up later.

	// We currently only support modules with a single output
	wl_assert(m_native_module_graph.get_node_outgoing_edge_count(m_source_root_node_handle) == 1);
	h_graph_node old_output_node =
		m_native_module_graph.get_node_outgoing_edge_handle(m_source_root_node_handle, 0);

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
