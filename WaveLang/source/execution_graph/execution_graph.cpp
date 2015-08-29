#include "execution_graph/execution_graph.h"
#include "common/utility/file_utility.h"
#include <string>
#include <algorithm>
#include <fstream>

static const char k_format_identifier[] = { 'w', 'a', 'v', 'e', 'l', 'a', 'n', 'g' };

c_execution_graph::c_execution_graph() {
	ZERO_STRUCT(&m_globals);
}

// $TODO do we care about being endian-correct?

e_execution_graph_result c_execution_graph::save(const char *fname) const {
	wl_assert(validate());

#if PREDEFINED(ASSERTS_ENABLED)
	// Make sure there are no invalid nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		wl_assert(m_nodes[index].type != k_execution_graph_node_type_invalid);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	std::ofstream out(fname, std::ios::binary);
	if (!out.is_open()) {
		return k_execution_graph_result_failed_to_write;
	}

	// Write identifier at the beginning
	for (size_t ch = 0; ch < NUMBEROF(k_format_identifier); ch++) {
		write(out, k_format_identifier[ch]);
	}
	write(out, k_execution_graph_format_version);

	// Write the globals
	write(out, m_globals.max_voices);

	// Write the node count, and also count up all edges
	uint32 node_count = static_cast<uint32>(m_nodes.size());
	uint32 edge_count = 0;
#if PREDEFINED(ASSERTS_ENABLED)
	uint32 edge_count_verify = 0;
#endif // PREDEFINED(ASSERTS_ENABLED)
	write(out, node_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &node = m_nodes[index];
		edge_count += static_cast<uint32>(node.outgoing_edge_indices.size());
#if PREDEFINED(ASSERTS_ENABLED)
		edge_count_verify += static_cast<uint32>(node.incoming_edge_indices.size());
#endif // PREDEFINED(ASSERTS_ENABLED)

		uint32 node_type = static_cast<uint32>(node.type);
		write(out, node_type);

		switch (node.type) {
		case k_execution_graph_node_type_constant:
			write(out, node.node_data.constant.type);
			if (node.node_data.constant.type == k_native_module_argument_type_real) {
				write(out, node.node_data.constant.real_value);
			} else if (node.node_data.constant.type == k_native_module_argument_type_bool) {
				write(out, node.node_data.constant.bool_value);
			} else if (node.node_data.constant.type == k_native_module_argument_type_string) {
				write(out, node.node_data.constant.string_index);
			} else {
				wl_unreachable();
			}
			break;

		case k_execution_graph_node_type_native_module_call:
			wl_assert(node.node_data.native_module_call.native_module_index != k_native_module_noop);
			write(out, node.node_data.native_module_call.native_module_index);
			break;

		case k_execution_graph_node_type_native_module_input:
		case k_execution_graph_node_type_native_module_output:
			break;

		case k_execution_graph_node_type_output:
			write(out, node.node_data.output.output_index);
			break;

		default:
			wl_unreachable();
		}
	}

	wl_assert(edge_count == edge_count_verify);

	// Write edges
	write(out, edge_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &node = m_nodes[index];
		for (size_t edge = 0; edge < node.outgoing_edge_indices.size(); edge++) {
			// Write pair (a, b)
			write(out, index);
			write(out, node.outgoing_edge_indices[edge]);
		}
	}

	// Write string table
	uint32 string_table_size = static_cast<uint32>(m_string_table.get_table_size());
	write(out, string_table_size);
	out.write(m_string_table.get_table_pointer(), m_string_table.get_table_size());

	if (out.fail()) {
		return k_execution_graph_result_failed_to_write;
	}

	return k_execution_graph_result_success;
}

e_execution_graph_result c_execution_graph::load(const char *fname) {
	wl_assert(m_nodes.empty());
	wl_assert(m_string_table.get_table_size() == 0);

	std::ifstream in(fname, std::ios::binary);
	if (!in.is_open()) {
		return k_execution_graph_result_failed_to_read;
	}

	// If we get a EOF error, it means that we successfully read but the file was shorter than expected, indicating that
	// we should return an invalid format error.

	// Read the identifiers at the beginning of the file
	char format_identifier_buffer[NUMBEROF(k_format_identifier)];
	for (size_t ch = 0; ch < NUMBEROF(k_format_identifier); ch++) {
		if (!read(in, format_identifier_buffer[ch])) {
			return in.eof() ? k_execution_graph_result_invalid_header : k_execution_graph_result_failed_to_read;
		}
	}

	uint32 format_version;
	if (!read(in, format_version)) {
		return in.eof() ? k_execution_graph_result_invalid_header : k_execution_graph_result_failed_to_read;
	}

	if (memcmp(k_format_identifier, format_identifier_buffer, sizeof(k_format_identifier)) != 0) {
		return k_execution_graph_result_invalid_header;
	}

	if (format_version != k_execution_graph_format_version) {
		return k_execution_graph_result_version_mismatch;
	}

	// Read the globals
	if (!read(in, m_globals.max_voices)) {
		return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
	}

	// Read the node count
	uint32 node_count;
	if (!read(in, node_count)) {
		return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
	}

	m_nodes.reserve(node_count);
	for (uint32 index = 0; index < node_count; index++) {
		s_node node;

		uint32 node_type;
		if (!read(in, node_type)) {
			return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
		}

		node.type = static_cast<e_execution_graph_node_type>(node_type);

		switch (node.type) {
		case k_execution_graph_node_type_constant:
			if (!read(in, node.node_data.constant.type)) {
				return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
			}

			if (node.node_data.constant.type == k_native_module_argument_type_real) {
				if (!read(in, node.node_data.constant.real_value)) {
					return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
				}
			} else if (node.node_data.constant.type == k_native_module_argument_type_bool) {
				if (!read(in, node.node_data.constant.bool_value)) {
					return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
				}
			} else if (node.node_data.constant.type == k_native_module_argument_type_string) {
				if (!read(in, node.node_data.constant.string_index)) {
					return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
				}
			} else {
				return k_execution_graph_result_invalid_graph;
			}

			break;

		case k_execution_graph_node_type_native_module_call:
			if (!read(in, node.node_data.native_module_call.native_module_index)) {
				return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
			}

			// Don't allow no-ops
			if (node.node_data.native_module_call.native_module_index == k_native_module_noop) {
				return k_execution_graph_result_invalid_graph;
			}
			break;

		case k_execution_graph_node_type_native_module_input:
		case k_execution_graph_node_type_native_module_output:
			break;

		case k_execution_graph_node_type_output:
			if (!read(in, node.node_data.output.output_index)) {
				return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
			}
			break;

		default:
			return k_execution_graph_result_invalid_graph;
		}

		m_nodes.push_back(node);
	}

	// Read edges
	uint32 edge_count;
	if (!read(in, edge_count)) {
		return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
	}

	for (uint32 index = 0; index < edge_count; index++) {
		uint32 from_index, to_index;
		if (!read(in, from_index) ||
			!read(in, to_index)) {
			return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
		}

		if (!add_edge_for_load(from_index, to_index)) {
			return k_execution_graph_result_invalid_graph;
		}
	}

	uint32 string_table_size;
	if (!read(in, string_table_size)) {
		return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
	}

	char *string_table_pointer = m_string_table.initialize_for_load(string_table_size);
	in.read(string_table_pointer, m_string_table.get_table_size());
	if (in.fail()) {
		return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
	}

	if (!validate()) {
		return k_execution_graph_result_invalid_graph;
	}

	return k_execution_graph_result_success;
}

bool c_execution_graph::validate() const {
	size_t output_node_count = 0;

	// Validate each node, validate each edge, check for cycles
	for (uint32 index = 0; index < get_node_count(); index++) {
		if (get_node_type(index) == k_execution_graph_node_type_output) {
			output_node_count++;
		}

		if (!validate_node(index)) {
			return false;
		}

		// Only bother validating outgoing edges, since it would be redundant to check both directions
		for (size_t edge = 0; edge < m_nodes[index].outgoing_edge_indices.size(); edge++) {
			if (!validate_edge(index, m_nodes[index].outgoing_edge_indices[edge])) {
				return false;
			}
		}
	}

	// Output indices for the n output nodes should be unique and map to the range [0,n-1]
	std::vector<bool> output_nodes_found(output_node_count, false);
	for (uint32 index = 0; index < get_node_count(); index++) {
		if (get_node_type(index) == k_execution_graph_node_type_output) {
			uint32 output_index = get_output_node_output_index(index);

			if (!VALID_INDEX(output_index, output_node_count)) {
				// Not in the range [0,n-1]
				return false;
			}

			if (output_nodes_found[output_index]) {
				// Duplicate output index
				return false;
			}

			output_nodes_found[output_index] = true;
		}
	}

	// We should have found exactly n unique outputs
#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = 0; index < output_nodes_found.size(); index++) {
		wl_assert(output_nodes_found[index]);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Check for cycles
	std::vector<bool> nodes_visited(m_nodes.size());
	std::vector<bool> nodes_marked(m_nodes.size());
	while (true) {
		// Find an unvisited node
		size_t index;
		for (index = 0; index < m_nodes.size(); index++) {
			if (!nodes_visited[index]) {
				break;
			}
		}

		if (index < m_nodes.size()) {
			if (!visit_node_for_cycle_detection(index, nodes_visited, nodes_marked)) {
				return false;
			}
		} else {
			break;
		}
	}

	if (!validate_constants()) {
		return false;
	}

	if (!validate_string_table()) {
		return false;
	}

	// Validate globals
	if (m_globals.max_voices < 1) {
		return false;
	}

	return true;
}

uint32 c_execution_graph::allocate_node() {
	uint32 index = static_cast<uint32>(m_nodes.size());
	m_nodes.push_back(s_node());
	ZERO_STRUCT(&m_nodes.back().node_data);
	return index;
}

uint32 c_execution_graph::add_constant_node(real32 constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = k_native_module_argument_type_real;
	node.node_data.constant.real_value = constant_value;
	return index;
}

uint32 c_execution_graph::add_constant_node(bool constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = k_native_module_argument_type_bool;
	node.node_data.constant.bool_value = constant_value;
	return index;
}

uint32 c_execution_graph::add_constant_node(const std::string &constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = k_native_module_argument_type_string;
	node.node_data.constant.string_index = static_cast<uint32>(m_string_table.add_string(constant_value.c_str()));
	return index;
}

uint32 c_execution_graph::add_native_module_call_node(uint32 native_module_index) {
	wl_assert(VALID_INDEX(native_module_index, c_native_module_registry::get_native_module_count()));

	uint32 index = allocate_node();
	// Don't store a reference to the node, it gets invalidated if the vector is resized when adding inputs/outputs
	m_nodes[index].type = k_execution_graph_node_type_native_module_call;
	m_nodes[index].node_data.native_module_call.native_module_index = native_module_index;

	// Add nodes for each argument; the return value is output 0
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		uint32 argument_node_index = allocate_node();
		s_node &argument_node = m_nodes[argument_node_index];
		if (argument.qualifier == k_native_module_argument_qualifier_in ||
			argument.qualifier == k_native_module_argument_qualifier_constant) {
			argument_node.type = k_execution_graph_node_type_native_module_input;
			argument_node.outgoing_edge_indices.push_back(index);
			m_nodes[index].incoming_edge_indices.push_back(argument_node_index);
		} else {
			wl_assert(argument.qualifier == k_native_module_argument_qualifier_out);
			argument_node.type = k_execution_graph_node_type_native_module_output;
			argument_node.incoming_edge_indices.push_back(index);
			m_nodes[index].outgoing_edge_indices.push_back(argument_node_index);
		}
	}

	return index;
}

uint32 c_execution_graph::add_output_node(uint32 output_index) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_output;
	node.node_data.output.output_index = output_index;
	return index;
}

void c_execution_graph::remove_node(uint32 node_index) {
	s_node &node = m_nodes[node_index];

	if (node.type == k_execution_graph_node_type_native_module_call) {
		// Remove input and output nodes - this will naturally break edges
		while (!node.incoming_edge_indices.empty()) {
			if (node.incoming_edge_indices.back() != k_invalid_index) {
				remove_node(node.incoming_edge_indices.back());
			}
		}

		while (!node.outgoing_edge_indices.empty()) {
			if (node.outgoing_edge_indices.back() != k_invalid_index) {
				remove_node(node.outgoing_edge_indices.back());
			}
		}
	} else {
		// Break edges
		while (!node.incoming_edge_indices.empty()) {
			if (node.incoming_edge_indices.back() != k_invalid_index) {
				remove_edge_internal(node.incoming_edge_indices.back(), node_index);
			}
		}

		while (!node.outgoing_edge_indices.empty()) {
			if (node.outgoing_edge_indices.back() != k_invalid_index) {
				remove_edge_internal(node_index, node.outgoing_edge_indices.back());
			}
		}
	}

	// Set to invalid and clear
	node.type = k_execution_graph_node_type_invalid;
	ZERO_STRUCT(&node.node_data);
}

void c_execution_graph::add_edge(uint32 from_index, uint32 to_index) {
#if PREDEFINED(ASSERTS_ENABLED)
	// The user-facing function should never touch a module node directly
	s_node &from_node = m_nodes[from_index];
	s_node &to_node = m_nodes[to_index];
	wl_assert(from_node.type != k_execution_graph_node_type_native_module_call);
	wl_assert(to_node.type != k_execution_graph_node_type_native_module_call);
#endif // PREDEFINED(ASSERTS_ENABLED)

	add_edge_internal(from_index, to_index);
}

void c_execution_graph::add_edge_internal(uint32 from_index, uint32 to_index) {
	s_node &from_node = m_nodes[from_index];
	s_node &to_node = m_nodes[to_index];

	// Add the edge only if it does not already exist
	bool exists = false;
	for (size_t from_out_index = 0;
		 !exists && from_out_index < from_node.outgoing_edge_indices.size();
		 from_out_index++) {
		if (from_node.outgoing_edge_indices[from_out_index] == to_index) {
			exists = true;
		}
	}

#if PREDEFINED(ASSERTS_ENABLED)
	bool validate_exists = false;
	for (size_t to_in_index = 0;
		 !validate_exists && to_in_index < to_node.incoming_edge_indices.size();
		 to_in_index++) {
		if (to_node.incoming_edge_indices[to_in_index] == from_index) {
			validate_exists = true;
		}
	}

	wl_assert(exists == validate_exists);
#endif // PREDEFINED(ASSERTS_ENABLED)

	if (!exists) {
		wl_assert(validate_edge(from_index, to_index));
		from_node.outgoing_edge_indices.push_back(to_index);
		to_node.incoming_edge_indices.push_back(from_index);
	}
}

bool c_execution_graph::add_edge_for_load(uint32 from_index, uint32 to_index) {
	// Don't perform validation, and return false if the edge is invalid or already exists
	if (!VALID_INDEX(from_index, m_nodes.size()) || !VALID_INDEX(to_index, m_nodes.size())) {
		return false;
	}

	s_node &from_node = m_nodes[from_index];
	s_node &to_node = m_nodes[to_index];

	for (size_t from_out_index = 0; from_out_index < from_node.outgoing_edge_indices.size(); from_out_index++) {
		if (from_node.outgoing_edge_indices[from_out_index] == to_index) {
			return false;
		}
	}

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t to_in_index = 0; to_in_index < to_node.incoming_edge_indices.size(); to_in_index++) {
		// Validate that it doesn't exist in both directions
		wl_assert(to_node.incoming_edge_indices[to_in_index] != from_index);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	from_node.outgoing_edge_indices.push_back(to_index);
	to_node.incoming_edge_indices.push_back(from_index);
	return true;
}

void c_execution_graph::remove_edge(uint32 from_index, uint32 to_index) {
#if PREDEFINED(ASSERTS_ENABLED)
	// The user-facing function should never touch a module node directly
	s_node &from_node = m_nodes[from_index];
	s_node &to_node = m_nodes[to_index];
	wl_assert(from_node.type != k_execution_graph_node_type_native_module_call);
	wl_assert(to_node.type != k_execution_graph_node_type_native_module_call);
#endif // PREDEFINED(ASSERTS_ENABLED)

	remove_edge_internal(from_index, to_index);
}

void c_execution_graph::remove_edge_internal(uint32 from_index, uint32 to_index) {
	s_node &from_node = m_nodes[from_index];
	s_node &to_node = m_nodes[to_index];

	// Remove the edge only if it exists
	bool found_out = false;
	for (size_t from_out_index = 0;
		 !found_out && from_out_index < from_node.outgoing_edge_indices.size();
		 from_out_index++) {
		if (from_node.outgoing_edge_indices[from_out_index] == to_index) {
			from_node.outgoing_edge_indices.erase(from_node.outgoing_edge_indices.begin() + from_out_index);
			found_out = true;
		}
	}

	bool found_in = false;
	for (size_t to_in_index = 0;
		 !found_in && to_in_index < to_node.incoming_edge_indices.size();
		 to_in_index++) {
		if (to_node.incoming_edge_indices[to_in_index] == from_index) {
			to_node.incoming_edge_indices.erase(to_node.incoming_edge_indices.begin() + to_in_index);
			found_in = true;
		}
	}

	// They should either both have been found, or both have not been found
	wl_assert(found_out == found_in);
}

bool c_execution_graph::validate_node(uint32 index) const {
	const s_node &node = m_nodes[index];

	// Validate edge counts. To validate each edge individually use validate_edge.
	switch (node.type) {
	case k_execution_graph_node_type_constant:
		return
			node.incoming_edge_indices.empty();

	case k_execution_graph_node_type_native_module_call:
	{
		if (!VALID_INDEX(node.node_data.native_module_call.native_module_index, 
			c_native_module_registry::get_native_module_count())) {
			return false;
		}

		const s_native_module &native_module =
			c_native_module_registry::get_native_module(node.node_data.native_module_call.native_module_index);

		return
			(node.incoming_edge_indices.size() == native_module.in_argument_count) &&
			(node.outgoing_edge_indices.size() == native_module.out_argument_count);
	}

	case k_execution_graph_node_type_native_module_input:
		return
			(node.incoming_edge_indices.size() == 1) &&
			(node.outgoing_edge_indices.size() == 1);

	case k_execution_graph_node_type_native_module_output:
		return
			(node.incoming_edge_indices.size() == 1);

	case k_execution_graph_node_type_output:
		return
			(node.incoming_edge_indices.size() == 1) &&
			node.outgoing_edge_indices.empty();

	default:
		// Unknown type
		return false;
	}

	return true;
}

bool c_execution_graph::validate_edge(uint32 from_index, uint32 to_index) const {
	// $TODO value types match
	if (!VALID_INDEX(from_index, m_nodes.size()) ||
		!VALID_INDEX(to_index, m_nodes.size())) {
		return false;
	}

	bool check_type = false;
	const s_node &from_node = m_nodes[from_index];
	const s_node &to_node = m_nodes[to_index];

	switch (from_node.type) {
	case k_execution_graph_node_type_constant:
		if ((to_node.type != k_execution_graph_node_type_native_module_input) &&
			(to_node.type != k_execution_graph_node_type_output)) {
			return false;
		}
		check_type = true;
		break;

	case k_execution_graph_node_type_native_module_call:
		if (to_node.type != k_execution_graph_node_type_native_module_output) {
			return false;
		}
		// Don't check type for native module call to native module output
		break;

	case k_execution_graph_node_type_native_module_input:
		if (to_node.type != k_execution_graph_node_type_native_module_call) {
			return false;
		}
		// Don't check type for native module input to native module call
		break;

	case k_execution_graph_node_type_native_module_output:
		if ((to_node.type != k_execution_graph_node_type_native_module_input) &&
			(to_node.type != k_execution_graph_node_type_output)) {
			return false;
		}
		check_type = true;
		break;

	case k_execution_graph_node_type_output:
		return false;

	default:
		// Unknown type
		return false;
	}

	if (check_type) {
		e_native_module_argument_type from_type;
		e_native_module_argument_type to_type;
		if (!get_type_from_node(from_index, from_type) ||
			!get_type_from_node(to_index, to_type)) {
			return false;
		}

		if (from_type != to_type) {
			return false;
		}
	}

	return true;
}

bool c_execution_graph::validate_constants() const {
	// This should only be called after all nodes and edges have been validated
	for (uint32 node_index = 0; node_index < m_nodes.size(); node_index++) {
		if (get_node_type(node_index) != k_execution_graph_node_type_native_module_call) {
			continue;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			get_native_module_call_native_module_index(node_index));

		wl_assert(native_module.in_argument_count == get_node_incoming_edge_count(node_index));
		// For each constant input, verify that a constant node is linked up
		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			s_native_module_argument argument = native_module.arguments[arg];
			if (argument.qualifier == k_native_module_argument_qualifier_in) {
				input++;
			} else if (argument.qualifier == k_native_module_argument_qualifier_constant) {
				// Validate that this input is constant
				uint32 input_node_index = get_node_incoming_edge_index(node_index, input);
				uint32 constant_node_index = get_node_incoming_edge_index(input_node_index, 0);

				if (get_node_type(constant_node_index) != k_execution_graph_node_type_constant) {
					return false;
				}

				input++;
			}
		}

		wl_assert(input == native_module.in_argument_count);
	}

	return true;
}

bool c_execution_graph::validate_string_table() const {
	// If we have any strings, they should be null-terminated
	if (m_string_table.get_table_size() > 0 &&
		m_string_table.get_table_pointer()[m_string_table.get_table_size()-1] != '\0') {
		return false;
	}

	// Each constant string node should be pointing to a valid string
	for (size_t index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];

		if (node.type != k_execution_graph_node_type_constant ||
			node.node_data.constant.type != k_native_module_argument_type_string) {
			continue;
		}

		if (!VALID_INDEX(node.node_data.constant.string_index, m_string_table.get_table_size())) {
			return false;
		}
	}

	return true;
}

bool c_execution_graph::get_type_from_node(uint32 node_index, e_native_module_argument_type &out_type) const {
	const s_node &node = m_nodes[node_index];

	switch (node.type) {
	case k_execution_graph_node_type_constant:
		out_type = node.node_data.constant.type;
		return true;

	case k_execution_graph_node_type_native_module_call:
		wl_vhalt("We shouldn't be checking types for this node, but instead for its argument nodes");
		return false;

	case k_execution_graph_node_type_native_module_input:
	{
		// Find the index of this input
		if (node.outgoing_edge_indices.size() != 1) {
			return false;
		}

		uint32 module_call_node_index = node.outgoing_edge_indices[0];
		if (!VALID_INDEX(module_call_node_index, m_nodes.size())) {
			return false;
		}

		const s_node &module_call_node = m_nodes[module_call_node_index];
		if (module_call_node.type != k_execution_graph_node_type_native_module_call) {
			return false;
		}

		if (!validate_node(module_call_node_index)) {
			return false;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			module_call_node.node_data.native_module_call.native_module_index);

		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			if (native_module.arguments[arg].qualifier == k_native_module_argument_qualifier_in ||
				native_module.arguments[arg].qualifier == k_native_module_argument_qualifier_constant) {
				if (module_call_node.incoming_edge_indices[input] == node_index) {
					// We've found the matching argument - return its type
					out_type = native_module.arguments[arg].type;
					return true;
				}
				input++;
			}
		}
		return false;
	}

	case k_execution_graph_node_type_native_module_output:
	{
		// Find the index of this output
		if (node.incoming_edge_indices.size() != 1) {
			return false;
		}

		uint32 module_call_node_index = node.incoming_edge_indices[0];
		if (!VALID_INDEX(module_call_node_index, m_nodes.size())) {
			return false;
		}

		const s_node &module_call_node = m_nodes[module_call_node_index];
		if (module_call_node.type != k_execution_graph_node_type_native_module_call) {
			return false;
		}

		if (!validate_node(module_call_node_index)) {
			return false;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			module_call_node.node_data.native_module_call.native_module_index);

		size_t output = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			if (native_module.arguments[arg].qualifier == k_native_module_argument_qualifier_out) {
				if (module_call_node.outgoing_edge_indices[output] == node_index) {
					// We've found the matching argument - return its type
					out_type = native_module.arguments[arg].type;
					return true;
				}
				output++;
			}
		}
		return false;
	}

	case k_execution_graph_node_type_output:
		out_type = k_native_module_argument_type_real;
		return true;

	default:
		// Unknown type
		return false;
	}
}


bool c_execution_graph::visit_node_for_cycle_detection(uint32 node_index,
	std::vector<bool> &nodes_visited, std::vector<bool> &nodes_marked) const {
	if (nodes_marked[node_index]) {
		// Cycle
		return false;
	}

	if (!nodes_visited[node_index]) {
		nodes_marked[node_index] = true;

		const s_node &node = m_nodes[node_index];
		for (size_t edge = 0; edge < node.outgoing_edge_indices.size(); edge++) {
			if (!visit_node_for_cycle_detection(node.outgoing_edge_indices[edge], nodes_visited, nodes_marked)) {
				return false;
			}
		}

		nodes_marked[node_index] = false;
		nodes_visited[node_index] = true;
	}

	return true;
}

void c_execution_graph::remove_unused_strings() {
	// We could be more efficient about this, but it's easier to just build another table and swap
	c_string_table new_string_table;
	for (size_t index = 0; index < m_nodes.size(); index++) {
		s_node &node = m_nodes[index];

		if (node.type != k_execution_graph_node_type_constant ||
			node.node_data.constant.type != k_native_module_argument_type_string) {
			continue;
		}

		uint32 new_string_index = static_cast<uint32>(new_string_table.add_string(
			m_string_table.get_string(node.node_data.constant.string_index)));
		node.node_data.constant.string_index = new_string_index;
	}

	m_string_table.swap(new_string_table);
}

uint32 c_execution_graph::get_node_count() const {
	return static_cast<uint32>(m_nodes.size());
}

e_execution_graph_node_type c_execution_graph::get_node_type(uint32 node_index) const {
	return m_nodes[node_index].type;
}

e_native_module_argument_type c_execution_graph::get_constant_node_data_type(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	return node.node_data.constant.type;
}

real32 c_execution_graph::get_constant_node_real_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == k_native_module_argument_type_real);
	return node.node_data.constant.real_value;
}

bool c_execution_graph::get_constant_node_bool_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == k_native_module_argument_type_bool);
	return node.node_data.constant.bool_value;
}

const char *c_execution_graph::get_constant_node_string_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == k_native_module_argument_type_string);
	return m_string_table.get_string(node.node_data.constant.string_index);
}

uint32 c_execution_graph::get_native_module_call_native_module_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_native_module_call);
	return node.node_data.native_module_call.native_module_index;
}

uint32 c_execution_graph::get_output_node_output_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_output);
	return node.node_data.output.output_index;
}

size_t c_execution_graph::get_node_incoming_edge_count(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	return node.incoming_edge_indices.size();
}

uint32 c_execution_graph::get_node_incoming_edge_index(uint32 node_index, size_t edge) const {
	const s_node &node = m_nodes[node_index];
	return node.incoming_edge_indices[edge];
}

size_t c_execution_graph::get_node_outgoing_edge_count(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	return node.outgoing_edge_indices.size();
}

uint32 c_execution_graph::get_node_outgoing_edge_index(uint32 node_index, size_t edge) const {
	const s_node &node = m_nodes[node_index];
	return node.outgoing_edge_indices[edge];
}

void c_execution_graph::set_globals(const s_execution_graph_globals &globals) {
	m_globals = globals;
}

const s_execution_graph_globals c_execution_graph::get_globals() const {
	return m_globals;
}

void c_execution_graph::remove_unused_nodes_and_reassign_node_indices() {
	std::vector<size_t> old_to_new_indices(m_nodes.size(), k_invalid_index);

	size_t next_new_index = 0;
	for (size_t old_index = 0; old_index < m_nodes.size(); old_index++) {
		if (m_nodes[old_index].type == k_execution_graph_node_type_invalid) {
			// This node was removed, don't assign it a new index
		} else {
			// Compress nodes down - use the move operator if possible to avoid copying edge list unnecessarily
			std::swap(m_nodes[old_index], m_nodes[next_new_index]);

			old_to_new_indices[old_index] = next_new_index;
			next_new_index++;
		}
	}

	m_nodes.resize(next_new_index);

	// Remap edge indices
	for (size_t old_index = 0; old_index < m_nodes.size(); old_index++) {
		s_node &node = m_nodes[old_index];
		for (size_t in = 0; in < node.incoming_edge_indices.size(); in++) {
			node.incoming_edge_indices[in] = old_to_new_indices[node.incoming_edge_indices[in]];
			wl_assert(node.incoming_edge_indices[in] != k_invalid_index);
		}
		for (size_t out = 0; out < node.outgoing_edge_indices.size(); out++) {
			node.outgoing_edge_indices[out] = old_to_new_indices[node.outgoing_edge_indices[out]];
			wl_assert(node.outgoing_edge_indices[out] != k_invalid_index);
		}
	}

	remove_unused_strings();
}

#if PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
void c_execution_graph::output_to_file() const {
	std::ofstream out("execution_graph.txt");

	out << "digraph ExecutionGraph {\n";

	// Declare the nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];
		const char *shape = nullptr;
		std::string label;
		bool skip = false;

		switch (node.type) {
		case k_execution_graph_node_type_constant:
			shape = "circle";
			if (node.node_data.constant.type == k_native_module_argument_type_real) {
				label = std::to_string(node.node_data.constant.real_value);
			} else if (node.node_data.constant.type == k_native_module_argument_type_bool) {
				label = m_string_table.get_string(node.node_data.constant.bool_value);
			} else if (node.node_data.constant.type == k_native_module_argument_type_string) {
				label = m_string_table.get_string(node.node_data.constant.string_index);
			} else {
				wl_unreachable();
			}
			break;

		case k_execution_graph_node_type_native_module_call:
			shape = "box";
			label = c_native_module_registry::get_native_module(
				node.node_data.native_module_call.native_module_index).name;
			break;

		case k_execution_graph_node_type_native_module_input:
			shape = "circle";
			label = "input";
			break;

		case k_execution_graph_node_type_native_module_output:
			shape = "circle";
			label = "output";
			break;

		case k_execution_graph_node_type_output:
			shape = "box";
			label = "out";
			break;

		default:
			// Don't add a node
			skip = true;
		}

		if (!skip) {
			out << "node [shape=" << shape << ", label=\"" << label << "\"]; node_" << index << ";\n";
		}
	}

	// Declare the edges
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];
		for (size_t edge = 0; edge < node.outgoing_edge_indices.size(); edge++) {
			uint32 to_index = node.outgoing_edge_indices[edge];

			out << "node_" << index << " -> node_" << to_index << ";\n";
		}
	}

	//out << "overlap=false\n";

	out << "}\n";
}
#endif // PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)