#include "compiler/graph_trimmer.h"

c_graph_trimmer::c_graph_trimmer(c_execution_graph &execution_graph)
	: m_execution_graph(execution_graph) {}

void c_graph_trimmer ::set_on_node_removed(f_on_node_removed on_node_removed, void *context) {
	m_on_node_removed = on_node_removed;
	m_on_node_removed_context = context;
}

c_execution_graph &c_graph_trimmer::get_execution_graph() {
	return m_execution_graph;
}

void c_graph_trimmer::add_temporary_reference(h_graph_node node_handle) {
	if (node_handle.is_valid()) {
		h_graph_node temporary_node_handle = m_execution_graph.add_temporary_reference_node();
		m_execution_graph.add_edge(node_handle, temporary_node_handle);
	}
}

void c_graph_trimmer::remove_temporary_reference(h_graph_node node_handle, bool try_trim) {
	if (node_handle.is_valid()) {
		IF_ASSERTS_ENABLED(bool found = false;)
		size_t edge_count = m_execution_graph.get_node_outgoing_edge_count(node_handle);
		for (size_t edge_index = 0; edge_index < edge_count; edge_index++) {
			h_graph_node to_node_handle =
				m_execution_graph.get_node_outgoing_edge_handle(node_handle, edge_index);

			e_execution_graph_node_type to_node_type = m_execution_graph.get_node_type(to_node_handle);
			if (to_node_type == e_execution_graph_node_type::k_temporary_reference) {
				m_execution_graph.remove_node(to_node_handle);
				if (try_trim) {
					try_trim_node(node_handle);
				}
				IF_ASSERTS_ENABLED(found = true;)
				break;
			}
		}

		wl_assert(found);
	}
}

void c_graph_trimmer::try_trim_node(h_graph_node node_handle) {
	{
		e_execution_graph_node_type node_type = m_execution_graph.get_node_type(node_handle);
		if (node_type == e_execution_graph_node_type::k_indexed_output) {
			// Jump to the source node
			node_handle = m_execution_graph.get_node_incoming_edge_handle(node_handle, 0);
		}
	}

	m_pending_nodes.push(node_handle);

	while (!m_pending_nodes.empty()) {
		h_graph_node node_handle = m_pending_nodes.top();
		m_pending_nodes.pop();

		// Check if this node has any outputs
		bool has_any_outputs = false;
		switch (m_execution_graph.get_node_type(node_handle)) {
		case e_execution_graph_node_type::k_constant:
		case e_execution_graph_node_type::k_array:
			has_any_outputs = m_execution_graph.get_node_outgoing_edge_count(node_handle) > 0;
			break;

		case e_execution_graph_node_type::k_native_module_call:
			for (size_t edge_index = 0;
				!has_any_outputs && edge_index < m_execution_graph.get_node_outgoing_edge_count(node_handle);
				edge_index++) {
				has_any_outputs |=
					m_execution_graph.get_node_indexed_output_outgoing_edge_count(node_handle, edge_index) > 0;
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
			bool remove_indexed_inputs = false;
			switch (m_execution_graph.get_node_type(node_handle)) {
			case e_execution_graph_node_type::k_constant:
				break;

			case e_execution_graph_node_type::k_array:
				wl_assert(m_execution_graph.get_node_data_type(node_handle).is_array());
				wl_assert(m_execution_graph.does_node_use_indexed_inputs(node_handle));
				remove_indexed_inputs = true;
				break;

			case e_execution_graph_node_type::k_native_module_call:
				wl_assert(m_execution_graph.does_node_use_indexed_inputs(node_handle));
				remove_indexed_inputs = true;
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

			if (remove_indexed_inputs) {
				size_t indexed_input_count = m_execution_graph.get_node_incoming_edge_count(node_handle);
				for (size_t edge_index = 0; edge_index < indexed_input_count; edge_index++) {
					h_graph_node in_node_handle =
						m_execution_graph.get_node_incoming_edge_handle(node_handle, edge_index);
					size_t in_edge_count = m_execution_graph.get_node_incoming_edge_count(in_node_handle);
					for (size_t in_edge_index = 0; in_edge_index < in_edge_count; in_edge_index++) {
						h_graph_node source_node_handle =
							m_execution_graph.get_node_incoming_edge_handle(in_node_handle, in_edge_index);
						add_pending_node(source_node_handle);
					}
				}
			}

			m_execution_graph.remove_node(node_handle, m_on_node_removed, m_on_node_removed_context);
		}
	}

	m_visited_nodes.clear();
}

void c_graph_trimmer::add_pending_node(h_graph_node node_handle) {
	if (m_execution_graph.get_node_type(node_handle) == e_execution_graph_node_type::k_indexed_output) {
		// Jump to the source node
		node_handle = m_execution_graph.get_node_incoming_edge_handle(node_handle, 0);
	}

	if (m_visited_nodes.find(node_handle) == m_visited_nodes.end()) {
		m_pending_nodes.push(node_handle);
		m_visited_nodes.insert(node_handle);
	}
}
