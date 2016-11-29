#include "common/utility/file_utility.h"
#include "common/utility/graphviz_generator.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <string>

const uint32 c_execution_graph::k_invalid_index;

static std::string format_real_for_graph_output(real32 value) {
	std::string str = std::to_string(value);

	// Trim trailing zeros
	size_t decimal_pos = str.find_first_of('.');
	if (decimal_pos != std::string::npos) {
		while (str.back() == '0') {
			str.pop_back();
		}

		// If we've removed all the zeros, remove the dot
		if (str.back() == '.') {
			str.pop_back();
		}
	}

	return str;
}

static e_instrument_result invalid_graph_or_read_failure(const std::ifstream &in) {
	return in.eof() ? k_instrument_result_invalid_graph : k_instrument_result_failed_to_read;
}

c_execution_graph::c_execution_graph() {
}

e_instrument_result c_execution_graph::save(std::ofstream &out) const {
	wl_assert(validate());

#if IS_TRUE(ASSERTS_ENABLED)
	// Make sure there are no invalid nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		wl_assert(m_nodes[index].type != k_execution_graph_node_type_invalid);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	c_binary_file_writer writer(out);

	// Write the node count, and also count up all edges
	uint32 node_count = cast_integer_verify<uint32>(m_nodes.size());
	uint32 edge_count = 0;
#if IS_TRUE(ASSERTS_ENABLED)
	uint32 edge_count_verify = 0;
#endif // IS_TRUE(ASSERTS_ENABLED)
	writer.write(node_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &node = m_nodes[index];
		edge_count += cast_integer_verify<uint32>(node.outgoing_edge_indices.size());
#if IS_TRUE(ASSERTS_ENABLED)
		edge_count_verify += cast_integer_verify<uint32>(node.incoming_edge_indices.size());
#endif // IS_TRUE(ASSERTS_ENABLED)

		uint32 node_type = static_cast<uint32>(node.type);
		writer.write(node_type);

		switch (node.type) {
		case k_execution_graph_node_type_constant:
		{
			writer.write(node.node_data.constant.type.write());
			if (node.node_data.constant.type.is_array()) {
				// No additional data
			} else {
				switch (node.node_data.constant.type.get_primitive_type()) {
				case k_native_module_primitive_type_real:
					writer.write(node.node_data.constant.real_value);
					break;

				case k_native_module_primitive_type_bool:
					writer.write(node.node_data.constant.bool_value);
					break;

				case k_native_module_primitive_type_string:
					writer.write(node.node_data.constant.string_index);
					break;

				default:
					wl_unreachable();
				}
			}
			break;
		}

		case k_execution_graph_node_type_native_module_call:
		{
			// Save out the UID, not the index
			const s_native_module &native_module =
				c_native_module_registry::get_native_module(node.node_data.native_module_call.native_module_index);
			writer.write(native_module.uid);
			break;
		}

		case k_execution_graph_node_type_indexed_input:
		case k_execution_graph_node_type_indexed_output:
			break;

		case k_execution_graph_node_type_input:
			writer.write(node.node_data.input.input_index);
			break;

		case k_execution_graph_node_type_output:
			writer.write(node.node_data.output.output_index);
			break;

		default:
			wl_unreachable();
		}
	}

	wl_assert(edge_count == edge_count_verify);

	// Write edges
	writer.write(edge_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &node = m_nodes[index];
		for (size_t edge = 0; edge < node.outgoing_edge_indices.size(); edge++) {
			// Write pair (a, b)
			writer.write(index);
			writer.write(node.outgoing_edge_indices[edge]);
		}
	}

	// Write string table
	uint32 string_table_size = cast_integer_verify<uint32>(m_string_table.get_table_size());
	writer.write(string_table_size);
	out.write(m_string_table.get_table_pointer(), m_string_table.get_table_size());

	if (out.fail()) {
		return k_instrument_result_failed_to_write;
	}

	return k_instrument_result_success;
}

e_instrument_result c_execution_graph::load(std::ifstream &in) {
	wl_assert(m_nodes.empty());
	wl_assert(m_string_table.get_table_size() == 0);

	c_binary_file_reader reader(in);

	// If we get a EOF error, it means that we successfully read but the file was shorter than expected, indicating that
	// we should return an invalid format error.

	// Read the node count
	uint32 node_count;
	if (!reader.read(node_count)) {
		return invalid_graph_or_read_failure(in);
	}

	m_nodes.reserve(node_count);
	for (uint32 index = 0; index < node_count; index++) {
		s_node node;

		uint32 node_type;
		if (!reader.read(node_type)) {
			return invalid_graph_or_read_failure(in);
		}

		node.type = static_cast<e_execution_graph_node_type>(node_type);

		switch (node.type) {
		case k_execution_graph_node_type_constant:
		{
			uint32 type_data;
			if (!reader.read(type_data)) {
				return invalid_graph_or_read_failure(in);
			}

			if (!node.node_data.constant.type.read(type_data)) {
				return k_instrument_result_invalid_graph;
			}

			if (node.node_data.constant.type.is_array()) {
				// No additional data
			} else {
				switch (node.node_data.constant.type.get_primitive_type()) {
				case k_native_module_primitive_type_real:
					if (!reader.read(node.node_data.constant.real_value)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				case k_native_module_primitive_type_bool:
					if (!reader.read(node.node_data.constant.bool_value)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				case k_native_module_primitive_type_string:
					if (!reader.read(node.node_data.constant.string_index)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				default:
					return k_instrument_result_invalid_graph;
				}
			}

			break;
		}

		case k_execution_graph_node_type_native_module_call:
		{
			// Read the native module UID and convert it to an index
			s_native_module_uid uid;
			if (!reader.read(uid)) {
				return invalid_graph_or_read_failure(in);
			}

			uint32 native_module_index = c_native_module_registry::get_native_module_index(uid);
			if (native_module_index == k_invalid_native_module_index) {
				return k_instrument_result_unregistered_native_module;
			}

			node.node_data.native_module_call.native_module_index = native_module_index;
			break;
		}

		case k_execution_graph_node_type_indexed_input:
		case k_execution_graph_node_type_indexed_output:
			break;

		case k_execution_graph_node_type_input:
			if (!reader.read(node.node_data.input.input_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		case k_execution_graph_node_type_output:
			if (!reader.read(node.node_data.output.output_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		default:
			return k_instrument_result_invalid_graph;
		}

		m_nodes.push_back(node);
	}

	// Read edges
	uint32 edge_count;
	if (!reader.read(edge_count)) {
		return invalid_graph_or_read_failure(in);
	}

	for (uint32 index = 0; index < edge_count; index++) {
		uint32 from_index, to_index;
		if (!reader.read(from_index) ||
			!reader.read(to_index)) {
			return invalid_graph_or_read_failure(in);
		}

		if (!add_edge_for_load(from_index, to_index)) {
			return k_instrument_result_invalid_graph;
		}
	}

	uint32 string_table_size;
	if (!reader.read(string_table_size)) {
		return invalid_graph_or_read_failure(in);
	}

	char *string_table_pointer = m_string_table.initialize_for_load(string_table_size);
	in.read(string_table_pointer, m_string_table.get_table_size());
	if (in.fail()) {
		return invalid_graph_or_read_failure(in);
	}

	if (!validate()) {
		return k_instrument_result_invalid_graph;
	}

	return k_instrument_result_success;
}

bool c_execution_graph::validate() const {
	size_t input_node_count = 0;
	size_t output_node_count = 0;

	// Validate each node, validate each edge, check for cycles
	for (uint32 index = 0; index < get_node_count(); index++) {
		input_node_count += (get_node_type(index) == k_execution_graph_node_type_input);
		output_node_count += (get_node_type(index) == k_execution_graph_node_type_output);

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

	if (output_node_count == 0) {
		// Need at least one output node for the remain-active result
		return false;
	}

	// Input indices for the n input nodes should be unique and map to the range [0,n-1]. This is also true for output
	// nodes, but with one additional node for the remain-active result
	std::vector<bool> input_nodes_found(input_node_count, false);
	std::vector<bool> output_nodes_found(output_node_count - 1, false);
	bool remain_active_output_node_found = false;
	for (uint32 index = 0; index < get_node_count(); index++) {
		if (get_node_type(index) == k_execution_graph_node_type_input) {
			uint32 input_index = get_input_node_input_index(index);

			if (!VALID_INDEX(input_index, input_node_count)) {
				// Not in the range [0,n-1]
				return false;
			}

			if (input_nodes_found[input_index]) {
				// Duplicate input index
				return false;
			}

			input_nodes_found[input_index] = true;
		} else if (get_node_type(index) == k_execution_graph_node_type_output) {
			uint32 output_index = get_output_node_output_index(index);

			if (output_index == k_remain_active_output_index) {
				if (remain_active_output_node_found) {
					// Duplicate remain-active result
					return false;
				}

				remain_active_output_node_found = true;
			} else {
				if (!VALID_INDEX(output_index, output_node_count - 1)) {
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
	}

	// We should have found exactly n unique input and outputs
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = 0; index < input_nodes_found.size(); index++) {
		wl_assert(input_nodes_found[index]);
	}

	for (size_t index = 0; index < output_nodes_found.size(); index++) {
		wl_assert(output_nodes_found[index]);
	}

	wl_assert(remain_active_output_node_found);
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Check for cycles
	std::vector<bool> nodes_visited(m_nodes.size());
	std::vector<bool> nodes_marked(m_nodes.size());
	while (true) {
		// Find an unvisited node
		uint32 index;
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

	return true;
}

bool c_execution_graph::does_node_use_indexed_inputs(const s_node &node) {
	switch (node.type) {
	case k_execution_graph_node_type_constant:
		return node.node_data.constant.type.is_array();

	case k_execution_graph_node_type_native_module_call:
		return true;

	case k_execution_graph_node_type_indexed_input:
	case k_execution_graph_node_type_indexed_output:
	case k_execution_graph_node_type_input:
	case k_execution_graph_node_type_output:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

bool c_execution_graph::does_node_use_indexed_outputs(const s_node &node) {
	switch (node.type) {
	case k_execution_graph_node_type_constant:
		return false;

	case k_execution_graph_node_type_native_module_call:
		return true;

	case k_execution_graph_node_type_indexed_input:
	case k_execution_graph_node_type_indexed_output:
	case k_execution_graph_node_type_input:
	case k_execution_graph_node_type_output:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

uint32 c_execution_graph::allocate_node() {
	uint32 index = cast_integer_verify<uint32>(m_nodes.size());
	m_nodes.push_back(s_node());
	ZERO_STRUCT(&m_nodes.back().node_data);
	return index;
}

uint32 c_execution_graph::add_constant_node(real32 constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = c_native_module_data_type(k_native_module_primitive_type_real);
	node.node_data.constant.real_value = constant_value;
	return index;
}

uint32 c_execution_graph::add_constant_node(bool constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = c_native_module_data_type(k_native_module_primitive_type_bool);
	node.node_data.constant.bool_value = constant_value;
	return index;
}

uint32 c_execution_graph::add_constant_node(const std::string &constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.node_data.constant.type = c_native_module_data_type(k_native_module_primitive_type_string);
	node.node_data.constant.string_index = cast_integer_verify<uint32>(
		m_string_table.add_string(constant_value.c_str()));
	return index;
}

uint32 c_execution_graph::add_constant_array_node(c_native_module_data_type element_type) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	// Will assert if there is not a valid array type (e.g. cannot make array of array)
	node.node_data.constant.type = element_type.get_array_type();
	return index;
}

void c_execution_graph::add_constant_array_value(uint32 constant_array_node_index, uint32 value_node_index) {
#if IS_TRUE(ASSERTS_ENABLED)
	const s_node &constant_array_node = m_nodes[constant_array_node_index];
	wl_assert(constant_array_node.type == k_execution_graph_node_type_constant);
	// Will assert if this is not an array type
	c_native_module_data_type element_type = constant_array_node.node_data.constant.type.get_element_type();

	c_native_module_data_type value_node_type;
	bool obtained_type = get_type_from_node(value_node_index, value_node_type);
	wl_assert(obtained_type);
	wl_assert(element_type == value_node_type);
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Create an indexed input node to represent this array index
	uint32 input_node_index = allocate_node();
	s_node &input_node = m_nodes[input_node_index];
	input_node.type = k_execution_graph_node_type_indexed_input;
	add_edge_internal(input_node_index, constant_array_node_index);
	add_edge_internal(value_node_index, input_node_index);
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
		if (native_module_qualifier_is_input(argument.type.get_qualifier())) {
			argument_node.type = k_execution_graph_node_type_indexed_input;
			add_edge_internal(argument_node_index, index);
		} else {
			wl_assert(argument.type.get_qualifier() == k_native_module_qualifier_out);
			argument_node.type = k_execution_graph_node_type_indexed_output;
			add_edge_internal(index, argument_node_index);
		}
	}

	return index;
}

uint32 c_execution_graph::add_input_node(uint32 input_index) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_input;
	node.node_data.input.input_index = input_index;
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

	if (does_node_use_indexed_inputs(node)) {
		// Remove input nodes - this will naturally break edges
		while (!node.incoming_edge_indices.empty()) {
			if (node.incoming_edge_indices.back() != k_invalid_index) {
				remove_node(node.incoming_edge_indices.back());
			}
		}
	} else {
		// Break edges
		while (!node.incoming_edge_indices.empty()) {
			if (node.incoming_edge_indices.back() != k_invalid_index) {
				remove_edge_internal(node.incoming_edge_indices.back(), node_index);
			}
		}
	}

	if (does_node_use_indexed_outputs(node)) {
		// Remove output nodes - this will naturally break edges
		while (!node.outgoing_edge_indices.empty()) {
			if (node.outgoing_edge_indices.back() != k_invalid_index) {
				remove_node(node.outgoing_edge_indices.back());
			}
		}
	} else {
		// Break edges
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
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = m_nodes[to_index];
	const s_node &from_node = m_nodes[from_index];
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

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

#if IS_TRUE(ASSERTS_ENABLED)
	bool validate_exists = false;
	for (size_t to_in_index = 0;
		 !validate_exists && to_in_index < to_node.incoming_edge_indices.size();
		 to_in_index++) {
		if (to_node.incoming_edge_indices[to_in_index] == from_index) {
			validate_exists = true;
		}
	}

	wl_assert(exists == validate_exists);
#endif // IS_TRUE(ASSERTS_ENABLED)

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

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t to_in_index = 0; to_in_index < to_node.incoming_edge_indices.size(); to_in_index++) {
		// Validate that it doesn't exist in both directions
		wl_assert(to_node.incoming_edge_indices[to_in_index] != from_index);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	from_node.outgoing_edge_indices.push_back(to_index);
	to_node.incoming_edge_indices.push_back(from_index);
	return true;
}

void c_execution_graph::remove_edge(uint32 from_index, uint32 to_index) {
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = m_nodes[to_index];
	const s_node &from_node = m_nodes[from_index];
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

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
	{
		// We don't need to check here that the type itself it valid because if it isn't the graph will fail to load

		if (node.node_data.constant.type.is_array()) {
			// Arrays can have inputs
			return true;
		} else {
			return node.incoming_edge_indices.empty();
		}
	}

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

	case k_execution_graph_node_type_indexed_input:
		return
			(node.incoming_edge_indices.size() == 1) &&
			(node.outgoing_edge_indices.size() == 1);

	case k_execution_graph_node_type_indexed_output:
		return
			(node.incoming_edge_indices.size() == 1);

	case k_execution_graph_node_type_input:
		return
			(node.incoming_edge_indices.empty());

	case k_execution_graph_node_type_output:
		return
			(node.incoming_edge_indices.size() == 1) &&
			node.outgoing_edge_indices.empty();

	default:
		// Unknown type
		return false;
	}
}

bool c_execution_graph::validate_edge(uint32 from_index, uint32 to_index) const {
	if (!VALID_INDEX(from_index, m_nodes.size()) || !VALID_INDEX(to_index, m_nodes.size())) {
		return false;
	}

	bool check_type = false;
	const s_node &from_node = m_nodes[from_index];
	const s_node &to_node = m_nodes[to_index];

	switch (from_node.type) {
	case k_execution_graph_node_type_constant:
		if ((to_node.type != k_execution_graph_node_type_indexed_input) &&
			(to_node.type != k_execution_graph_node_type_output)) {
			return false;
		}
		check_type = true;
		break;

	case k_execution_graph_node_type_native_module_call:
		if (to_node.type != k_execution_graph_node_type_indexed_output) {
			return false;
		}
		// Don't check type for native module calls
		break;

	case k_execution_graph_node_type_indexed_input:
		if (!does_node_use_indexed_inputs(to_node)) {
			return false;
		}
		// Don't check type for indexed inputs
		break;

	case k_execution_graph_node_type_indexed_output:
		if ((to_node.type != k_execution_graph_node_type_indexed_input) &&
			(to_node.type != k_execution_graph_node_type_output)) {
			return false;
		}
		check_type = true;
		break;

	case k_execution_graph_node_type_input:
		if ((to_node.type != k_execution_graph_node_type_indexed_input) &&
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
		c_native_module_data_type from_type;
		c_native_module_data_type to_type;
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
			if (argument.type.get_qualifier() == k_native_module_qualifier_in) {
				input++;
			} else if (argument.type.get_qualifier() == k_native_module_qualifier_constant) {
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
			node.node_data.constant.type != c_native_module_data_type(k_native_module_primitive_type_string)) {
			continue;
		}

		if (!VALID_INDEX(node.node_data.constant.string_index, m_string_table.get_table_size())) {
			return false;
		}
	}

	return true;
}

bool c_execution_graph::get_type_from_node(uint32 node_index, c_native_module_data_type &out_type) const {
	const s_node &node = m_nodes[node_index];

	switch (node.type) {
	case k_execution_graph_node_type_constant:
		out_type = node.node_data.constant.type;
		return true;

	case k_execution_graph_node_type_native_module_call:
		wl_vhalt("We shouldn't be checking types for this node, but instead for its argument nodes");
		return false;

	case k_execution_graph_node_type_indexed_input:
	{
		// Find the index of this input
		if (node.outgoing_edge_indices.size() != 1) {
			return false;
		}

		uint32 dest_node_index = node.outgoing_edge_indices[0];
		if (!VALID_INDEX(dest_node_index, m_nodes.size())) {
			return false;
		}

		if (!validate_node(dest_node_index)) {
			return false;
		}

		const s_node &dest_node = m_nodes[dest_node_index];

		if (dest_node.type == k_execution_graph_node_type_constant) {
			// Since we've validated the node, we know this must be an array type if it's a constant. Otherwise, it
			// should not have an indexed input.
			out_type = dest_node.node_data.constant.type.get_element_type();
			return true;
		} else if (dest_node.type == k_execution_graph_node_type_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.node_data.native_module_call.native_module_index);

			size_t input = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier())) {
					if (dest_node.incoming_edge_indices[input] == node_index) {
						// We've found the matching argument - return its type
						out_type = native_module.arguments[arg].type.get_data_type();
						return true;
					}
					input++;
				}
			}
		}

		return false;
	}

	case k_execution_graph_node_type_indexed_output:
	{
		// Find the index of this output
		if (node.incoming_edge_indices.size() != 1) {
			return false;
		}

		uint32 dest_node_index = node.incoming_edge_indices[0];
		if (!VALID_INDEX(dest_node_index, m_nodes.size())) {
			return false;
		}

		if (!validate_node(dest_node_index)) {
			return false;
		}

		const s_node &dest_node = m_nodes[dest_node_index];

		if (dest_node.type == k_execution_graph_node_type_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.node_data.native_module_call.native_module_index);

			size_t output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module.arguments[arg].type.get_qualifier() == k_native_module_qualifier_out) {
					if (dest_node.outgoing_edge_indices[output] == node_index) {
						// We've found the matching argument - return its type
						out_type = native_module.arguments[arg].type.get_data_type();
						return true;
					}
					output++;
				}
			}
		}

		return false;
	}

	case k_execution_graph_node_type_input:
		out_type = c_native_module_data_type(k_native_module_primitive_type_real);
		return true;

	case k_execution_graph_node_type_output:
		if (node.node_data.output.output_index == k_remain_active_output_index) {
			out_type = c_native_module_data_type(k_native_module_primitive_type_bool);
		} else {
			out_type = c_native_module_data_type(k_native_module_primitive_type_real);
		}
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
			node.node_data.constant.type != c_native_module_data_type(k_native_module_primitive_type_string)) {
			continue;
		}

		uint32 new_string_index = cast_integer_verify<uint32>(new_string_table.add_string(
			m_string_table.get_string(node.node_data.constant.string_index)));
		node.node_data.constant.string_index = new_string_index;
	}

	m_string_table.swap(new_string_table);
}

uint32 c_execution_graph::get_node_count() const {
	return cast_integer_verify<uint32>(m_nodes.size());
}

e_execution_graph_node_type c_execution_graph::get_node_type(uint32 node_index) const {
	return m_nodes[node_index].type;
}

c_native_module_data_type c_execution_graph::get_constant_node_data_type(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	return node.node_data.constant.type;
}

real32 c_execution_graph::get_constant_node_real_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == c_native_module_data_type(k_native_module_primitive_type_real));
	return node.node_data.constant.real_value;
}

bool c_execution_graph::get_constant_node_bool_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == c_native_module_data_type(k_native_module_primitive_type_bool));
	return node.node_data.constant.bool_value;
}

const char *c_execution_graph::get_constant_node_string_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	wl_assert(node.node_data.constant.type == c_native_module_data_type(k_native_module_primitive_type_string));
	return m_string_table.get_string(node.node_data.constant.string_index);
}

uint32 c_execution_graph::get_native_module_call_native_module_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_native_module_call);
	return node.node_data.native_module_call.native_module_index;
}

uint32 c_execution_graph::get_input_node_input_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_input);
	return node.node_data.input.input_index;
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

bool c_execution_graph::does_node_use_indexed_inputs(uint32 node_index) const {
	return does_node_use_indexed_inputs(m_nodes[node_index]);
}

bool c_execution_graph::does_node_use_indexed_outputs(uint32 node_index) const {
	return does_node_use_indexed_outputs(m_nodes[node_index]);
}

void c_execution_graph::remove_unused_nodes_and_reassign_node_indices() {
	std::vector<uint32> old_to_new_indices(m_nodes.size(), k_invalid_index);

	uint32 next_new_index = 0;
	for (uint32 old_index = 0; old_index < m_nodes.size(); old_index++) {
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
		for (uint32 in = 0; in < node.incoming_edge_indices.size(); in++) {
			node.incoming_edge_indices[in] = old_to_new_indices[node.incoming_edge_indices[in]];
			wl_assert(node.incoming_edge_indices[in] != k_invalid_index);
		}
		for (uint32 out = 0; out < node.outgoing_edge_indices.size(); out++) {
			node.outgoing_edge_indices[out] = old_to_new_indices[node.outgoing_edge_indices[out]];
			wl_assert(node.outgoing_edge_indices[out] != k_invalid_index);
		}
	}

	remove_unused_strings();
}

bool c_execution_graph::generate_graphviz_file(const char *fname, bool collapse_large_arrays) const {
	static const uint32 k_large_array_limit = 8;

	std::vector<bool> arrays_to_collapse;
	std::vector<bool> nodes_to_skip;

	if (collapse_large_arrays) {
		arrays_to_collapse.resize(m_nodes.size());
		nodes_to_skip.resize(m_nodes.size());

		// Find and mark all large constant arrays
		for (size_t index = 0; index < m_nodes.size(); index++) {
			const s_node &node = m_nodes[index];

			if (node.type == k_execution_graph_node_type_constant) {
				if (node.node_data.constant.type.is_array() &&
					node.incoming_edge_indices.size() >= k_large_array_limit) {
					// Check if all inputs are constants
					bool all_constant = true;
					for (size_t input = 0; all_constant && input < node.incoming_edge_indices.size(); input++) {
						uint32 input_node_index = node.incoming_edge_indices[input];
						uint32 source_node_index = m_nodes[input_node_index].incoming_edge_indices[0];
						all_constant = (m_nodes[source_node_index].type == k_execution_graph_node_type_constant);
					}

					if (all_constant) {
						arrays_to_collapse[index] = true;

						// Mark each input as an array to collapse so that checking the constants below is easier. Also
						// mark it as a node to skip, so we skip it during output.
						for (size_t input = 0; input < node.incoming_edge_indices.size(); input++) {
							uint32 input_node_index = node.incoming_edge_indices[input];
							arrays_to_collapse[input_node_index] = true;
							nodes_to_skip[input_node_index] = true;
						}
					}
				}
			}
		}

		// Find and mark all constants which are inputs to only the nodes we just marked
		for (size_t index = 0; index < m_nodes.size(); index++) {
			const s_node &node = m_nodes[index];

			if (node.type == k_execution_graph_node_type_constant) {
				bool all_outputs_marked = true;
				for (size_t output = 0; all_outputs_marked && output < node.outgoing_edge_indices.size(); output++) {
					uint32 output_node_index = node.outgoing_edge_indices[output];
					all_outputs_marked = arrays_to_collapse[output_node_index];
				}

				if (all_outputs_marked) {
					nodes_to_skip[index] = true;
				}
			}
		}
	}

	c_graphviz_generator graph;
	graph.set_graph_name("execution_graph");

	size_t collapsed_nodes = 0;

	// Declare the nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];
		c_graphviz_node graph_node;

		if (collapse_large_arrays && nodes_to_skip[index]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		switch (node.type) {
		case k_execution_graph_node_type_constant:
		{
			graph_node.set_shape("ellipse");
			std::string label;
			bool is_array = node.node_data.constant.type.is_array();
			switch (node.node_data.constant.type.get_primitive_type()) {
			case k_native_module_primitive_type_real:
				label = is_array ? "real[]" : format_real_for_graph_output(node.node_data.constant.real_value);
				break;

			case k_native_module_primitive_type_bool:
				label = is_array ? "bool[]" : node.node_data.constant.bool_value ? "true" : "false";
				break;

			case k_native_module_primitive_type_string:
				label = is_array ?
					"string[]" :
					"\\\"" + std::string(m_string_table.get_string(node.node_data.constant.string_index)) + "\\\"";
				break;

			default:
				wl_unreachable();
			}

			graph_node.set_label(label.c_str());
			break;
		}

		case k_execution_graph_node_type_native_module_call:
			graph_node.set_shape("box");
			graph_node.set_label(c_native_module_registry::get_native_module(
				node.node_data.native_module_call.native_module_index).name.get_string());
			break;

		case k_execution_graph_node_type_indexed_input:
		{
			wl_assert(node.outgoing_edge_indices.size() == 1);
			const s_node &root_node = m_nodes[node.outgoing_edge_indices[0]];
			uint32 input_index;
			for (input_index = 0; input_index < root_node.incoming_edge_indices.size(); input_index++) {
				if (root_node.incoming_edge_indices[input_index] == index) {
					break;
				}
			}

			wl_assert(VALID_INDEX(input_index, root_node.incoming_edge_indices.size()));
			graph_node.set_shape("house");
			graph_node.set_orientation(180.0f);
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(input_index)).c_str());
			break;
		}

		case k_execution_graph_node_type_indexed_output:
		{
			wl_assert(node.incoming_edge_indices.size() == 1);
			const s_node &root_node = m_nodes[node.incoming_edge_indices[0]];
			uint32 output_index;
			for (output_index = 0; output_index < root_node.outgoing_edge_indices.size(); output_index++) {
				if (root_node.outgoing_edge_indices[output_index] == index) {
					break;
				}
			}

			wl_assert(VALID_INDEX(output_index, root_node.outgoing_edge_indices.size()));
			graph_node.set_shape("house");
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(output_index)).c_str());
			break;
		}

		case k_execution_graph_node_type_input:
			graph_node.set_shape("box");
			graph_node.set_label(("input " + std::to_string(node.node_data.input.input_index)).c_str());
			graph_node.set_style("rounded");
			break;

		case k_execution_graph_node_type_output:
			graph_node.set_shape("box");
			{
				std::string output_index_string;
				if (node.node_data.output.output_index == k_remain_active_output_index) {
					output_index_string = "remain-active";
				} else {
					output_index_string = std::to_string(node.node_data.output.output_index);
				}
				graph_node.set_label(("output " + output_index_string).c_str());
			}
			graph_node.set_style("rounded");
			break;

		default:
			wl_unreachable();
		}

		std::string node_name = "node_" + std::to_string(index);
		graph_node.set_name(node_name.c_str());
		graph.add_node(graph_node);

		if (collapse_large_arrays && arrays_to_collapse[index]) {
			// Add an extra "collapsed" node for this array
			std::string collapsed_array_node_name = "collapsed_array_" + std::to_string(collapsed_nodes);
			c_graphviz_node collapsed_array_node;
			collapsed_array_node.set_name(collapsed_array_node_name.c_str());
			collapsed_array_node.set_shape("ellipse");
			collapsed_array_node.set_peripheries(2);
			collapsed_array_node.set_label(("[" + std::to_string(node.incoming_edge_indices.size()) + "]").c_str());
			graph.add_node(collapsed_array_node);

			collapsed_nodes++;

			// Add the edge
			graph.add_edge(collapsed_array_node_name.c_str(), node_name.c_str());
		}
	}

	// Declare the edges
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		if (collapse_large_arrays && nodes_to_skip[index]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		const s_node &node = m_nodes[index];
		for (size_t edge = 0; edge < node.outgoing_edge_indices.size(); edge++) {
			uint32 to_index = node.outgoing_edge_indices[edge];
			if (collapse_large_arrays && nodes_to_skip[to_index]) {
				// This is a node which has been collapsed - skip it
				continue;
			}

			graph.add_edge(
				("node_" + std::to_string(index)).c_str(),
				("node_" + std::to_string(to_index)).c_str());
		}
	}

	return graph.output_to_file(fname);
}
