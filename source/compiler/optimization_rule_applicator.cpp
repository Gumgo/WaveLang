#include "compiler/optimization_rule_applicator.h"

c_optimization_rule_applicator::c_optimization_rule_applicator(c_execution_graph &execution_graph)
	: m_execution_graph(execution_graph) {}

bool c_optimization_rule_applicator::try_apply_optimization_rule(c_node_reference node_reference, uint32 rule_index) {
	m_source_root_node_reference = node_reference;
	m_rule = &c_native_module_registry::get_optimization_rule(rule_index);

	// Initially we haven't matched any values

	for (size_t index = 0; index < m_matched_variable_node_references.get_count(); index++) {
		m_matched_variable_node_references[index] = c_node_reference();
	}

	for (size_t index = 0; index < m_matched_constant_node_references.get_count(); index++) {
		m_matched_constant_node_references[index] = c_node_reference();
	}

	if (try_to_match_source_pattern()) {
		c_node_reference target_root_node_reference = build_target_pattern();
		reroute_source_to_target(target_root_node_reference);
		return true;
	}

	return false;
}

bool c_optimization_rule_applicator::has_more_inputs(const s_match_state &match_state) const {
	size_t input_count = m_execution_graph.get_node_incoming_edge_count(match_state.current_node_reference);
	return match_state.next_input_index < input_count;
}

c_node_reference c_optimization_rule_applicator::follow_next_input(
	s_match_state &match_state,
	c_node_reference &output_node_reference_out) {
	output_node_reference_out = c_node_reference();
	wl_assert(has_more_inputs(match_state));

	// Grab the input
	c_node_reference new_node_reference = m_execution_graph.get_node_indexed_input_incoming_edge_reference(
		match_state.current_node_reference,
		match_state.next_input_index,
		0);

	if (m_execution_graph.get_node_type(new_node_reference) == e_execution_graph_node_type::k_indexed_output) {
		// We need to advance an additional node for the case (module <- input <- output <- module)
		output_node_reference_out = new_node_reference;
		new_node_reference = m_execution_graph.get_node_incoming_edge_reference(new_node_reference, 0);
	}

	match_state.next_input_index++;
	return new_node_reference;
}

bool c_optimization_rule_applicator::try_to_match_source_pattern() {
	size_t symbol_count;
	for (symbol_count = 0;
		symbol_count < m_rule->source.symbols.get_count() && m_rule->source.symbols[symbol_count].is_valid();
		symbol_count++);

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
				initial_state.current_node_output_reference = c_node_reference();
				initial_state.current_node_reference = m_source_root_node_reference;
				initial_state.next_input_index = 0;
				m_match_state_stack.push(initial_state);
			} else {
				// Try to advance to the next input
				s_match_state &current_state = m_match_state_stack.top();
				wl_assertf(has_more_inputs(current_state), "Rule inputs don't match native module definition");

				s_match_state new_state;
				new_state.current_node_output_reference = c_node_reference();
				new_state.current_node_reference =
					follow_next_input(current_state, new_state.current_node_output_reference);
				new_state.next_input_index = 0;
				m_match_state_stack.push(new_state);
			}

			// The module described by the rule should match the top of the state stack
			if (!handle_source_native_module_symbol_match(native_module_handle)) {
				return false;
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

			c_node_reference new_output_node_reference = c_node_reference();
			c_node_reference new_node_reference = follow_next_input(current_state, new_output_node_reference);

			if (!handle_source_value_symbol_match(symbol, new_node_reference, new_output_node_reference)) {
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
	c_node_reference node_reference = current_state.current_node_reference;

	// It's a match if the current node is a native module of the same index
	return (m_execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call)
		&& (m_execution_graph.get_native_module_call_native_module_handle(node_reference) == native_module_handle);
}

bool c_optimization_rule_applicator::handle_source_value_symbol_match(
	const s_native_module_optimization_symbol &symbol,
	c_node_reference node_reference,
	c_node_reference output_node_reference) {
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable) {
		// Match anything except for constants
		if (m_execution_graph.is_node_constant(node_reference)) {
			return false;
		}

		// If this is a module node, it means we had to pass through an output node to get here. Store the output node
		// as the match, because that's what inputs will be hooked up to.
		store_match(symbol, output_node_reference.is_valid() ? output_node_reference : node_reference);
		return true;
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		// Match only constants
		if (!m_execution_graph.is_node_constant(node_reference)) {
			return false;
		}

		store_match(symbol, node_reference);
		return true;
	} else {
		// Match only constants with the given value
		if (m_execution_graph.get_node_type(node_reference) != e_execution_graph_node_type::k_constant) {
			return false;
		}

		// get_constant_node_<type>_value will assert that there's no type mismatch
		if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
			return (symbol.data.real_value == m_execution_graph.get_constant_node_real_value(node_reference));
		} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
			return (symbol.data.bool_value == m_execution_graph.get_constant_node_bool_value(node_reference));
		} else {
			wl_unreachable();
			return false;
		}
	}
}

void c_optimization_rule_applicator::store_match(
	const s_native_module_optimization_symbol &symbol,
	c_node_reference node_reference) {
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

c_node_reference c_optimization_rule_applicator::load_match(const s_native_module_optimization_symbol &symbol) const {
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

c_node_reference c_optimization_rule_applicator::build_target_pattern() {
	// Replace the matched source pattern nodes with the ones defined by the target in the rule
	c_node_reference root_node_reference = c_node_reference();

	size_t symbol_count;
	for (symbol_count = 0;
		symbol_count < m_rule->source.symbols.get_count() && m_rule->source.symbols[symbol_count].is_valid();
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
			c_node_reference node_reference = m_execution_graph.add_native_module_call_node(native_module_handle);

			// We currently only allow a single outgoing edge, due to the way we express rules (out arguments aren't
			// currently supported)
			wl_assert(m_execution_graph.get_node_outgoing_edge_count(node_reference) == 1);

			if (!root_node_reference.is_valid()) {
				// This is the root node of the target pattern
				wl_assert(m_match_state_stack.empty());
				root_node_reference = node_reference;
			} else {
				s_match_state &current_state = m_match_state_stack.top();
				wl_assert(has_more_inputs(current_state));

				c_node_reference input_node_reference = m_execution_graph.get_node_incoming_edge_reference(
					current_state.current_node_reference,
					current_state.next_input_index);
				c_node_reference output_node_reference = m_execution_graph.get_node_outgoing_edge_reference(
					node_reference,
					0);
				m_execution_graph.add_edge(output_node_reference, input_node_reference);
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
			c_node_reference matched_node_reference = handle_target_value_symbol_match(symbol);

			// Hook up this input and advance
			if (!root_node_reference.is_valid()) {
				wl_assert(m_match_state_stack.empty());
				root_node_reference = matched_node_reference;
				IF_ASSERTS_ENABLED(should_be_done = true);
			} else {
				wl_assert(!m_match_state_stack.empty());
				s_match_state &current_state = m_match_state_stack.top();
				wl_assert(has_more_inputs(current_state));

				c_node_reference input_node_reference = m_execution_graph.get_node_incoming_edge_reference(
					current_state.current_node_reference,
					current_state.next_input_index);
				current_state.next_input_index++;
				m_execution_graph.add_edge(matched_node_reference, input_node_reference);
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

c_node_reference c_optimization_rule_applicator::handle_target_value_symbol_match(
	const s_native_module_optimization_symbol &symbol) {
	if (symbol.type == e_native_module_optimization_symbol_type::k_variable
		|| symbol.type == e_native_module_optimization_symbol_type::k_constant) {
		return load_match(symbol);
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_real_value) {
		// Create a constant node with this value
		return m_execution_graph.add_constant_node(symbol.data.real_value);
	} else if (symbol.type == e_native_module_optimization_symbol_type::k_bool_value) {
		// Create a constant node with this value
		return m_execution_graph.add_constant_node(symbol.data.bool_value);
	} else {
		wl_unreachable();
		return c_node_reference();
	}
}


void c_optimization_rule_applicator::reroute_source_to_target(c_node_reference target_root_node_reference) {
	// Reroute the output of the old module to the output of the new one. Don't worry about disconnecting inputs or
	// deleting the old set of nodes, they will automatically be cleaned up later.

	// We currently only support modules with a single output
	wl_assert(m_execution_graph.get_node_outgoing_edge_count(m_source_root_node_reference) == 1);
	c_node_reference old_output_node =
		m_execution_graph.get_node_outgoing_edge_reference(m_source_root_node_reference, 0);

	switch (m_execution_graph.get_node_type(target_root_node_reference)) {
	case e_execution_graph_node_type::k_native_module_call:
	{
		wl_assert(m_execution_graph.get_node_outgoing_edge_count(target_root_node_reference) == 1);
		c_node_reference new_output_node =
			m_execution_graph.get_node_outgoing_edge_reference(target_root_node_reference, 0);
		transfer_outputs(new_output_node, old_output_node);
		break;
	}

	case e_execution_graph_node_type::k_constant:
	case e_execution_graph_node_type::k_array:
	case e_execution_graph_node_type::k_indexed_output:
		transfer_outputs(target_root_node_reference, old_output_node);
		break;

	case e_execution_graph_node_type::k_temporary_reference:
		// These should have all been removed
		wl_unreachable();
		break;

	default:
		wl_unreachable();
	}
}

void c_optimization_rule_applicator::transfer_outputs(
	c_node_reference destination_reference,
	c_node_reference source_reference) {
	// Redirect the inputs of input_node directly to the outputs of output_node
	while (m_execution_graph.get_node_outgoing_edge_count(source_reference) > 0) {
		c_node_reference to_node_reference = m_execution_graph.get_node_outgoing_edge_reference(source_reference, 0);
		m_execution_graph.remove_edge(source_reference, to_node_reference);
		m_execution_graph.add_edge(destination_reference, to_node_reference);
	}
}
