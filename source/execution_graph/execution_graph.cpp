#include "common/utility/file_utility.h"
#include "common/utility/graphviz_generator.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <string>

#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
#define SALT_ARG(salt) , (salt)
#else // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
#define SALT_ARG(salt)
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)

struct s_native_module_data_type_format {
	union {
		uint32 data;

		struct {
			uint16 primitive_type;
			uint16 is_array;
		};
	};
};

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
	return in.eof() ? e_instrument_result::k_invalid_graph : e_instrument_result::k_failed_to_read;
}

static uint32 write_data_type(const c_native_module_data_type &data_type) {
	wl_assert(data_type.is_valid()); // Invalid type is only used as a sentinel in a few spots, should never write it
	s_native_module_data_type_format format;
	format.primitive_type = static_cast<uint16>(data_type.get_primitive_type());
	format.is_array = static_cast<uint16>(data_type.is_array());
	return format.data;
}

static bool read_data_type(uint32 data, c_native_module_data_type &data_type_out) {
	s_native_module_data_type_format format;
	format.data = data;

	if (!valid_index(format.primitive_type, enum_count<e_native_module_primitive_type>())) {
		return false;
	}

	// This is checking for out-of-range flags
	if (format.is_array >= 2) {
		return false;
	}

	data_type_out = c_native_module_data_type(
		static_cast<e_native_module_primitive_type>(format.primitive_type),
		(format.is_array != 0));
	return true;
}

c_execution_graph::c_execution_graph() {}

e_instrument_result c_execution_graph::save(std::ofstream &out) const {
	wl_assert(validate());

#if IS_TRUE(ASSERTS_ENABLED)
	// Make sure there are no invalid or temporary reference nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		wl_assert(m_nodes[index].type != e_execution_graph_node_type::k_invalid);
		wl_assert(m_nodes[index].type != e_execution_graph_node_type::k_temporary_reference);
	}

	wl_assert(m_free_node_indices.empty());
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
		edge_count += cast_integer_verify<uint32>(node.outgoing_edge_references.size());
#if IS_TRUE(ASSERTS_ENABLED)
		edge_count_verify += cast_integer_verify<uint32>(node.incoming_edge_references.size());
#endif // IS_TRUE(ASSERTS_ENABLED)

		uint32 node_type = static_cast<uint32>(node.type);
		writer.write(node_type);

		switch (node.type) {
		case e_execution_graph_node_type::k_constant:
			wl_assert(!node.constant_node_data().type.is_array());
			writer.write(write_data_type(node.constant_node_data().type));
			switch (node.constant_node_data().type.get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				writer.write(node.constant_node_data().real_value);
				break;

			case e_native_module_primitive_type::k_bool:
				writer.write(node.constant_node_data().bool_value);
				break;

			case e_native_module_primitive_type::k_string:
				writer.write(node.constant_node_data().string_index);
				break;

			default:
				wl_unreachable();
			}
			break;

		case e_execution_graph_node_type::k_array:
			wl_assert(node.array_node_data().type.is_array());
			writer.write(write_data_type(node.array_node_data().type));
			break;

		case e_execution_graph_node_type::k_native_module_call:
		{
			// Save out the UID, not the index
			const s_native_module &native_module =
				c_native_module_registry::get_native_module(node.native_module_call_node_data().native_module_handle);
			writer.write(native_module.uid);
			break;
		}

		case e_execution_graph_node_type::k_indexed_input:
		case e_execution_graph_node_type::k_indexed_output:
			break;

		case e_execution_graph_node_type::k_input:
			writer.write(node.input_node_data().input_index);
			break;

		case e_execution_graph_node_type::k_output:
			writer.write(node.output_node_data().output_index);
			break;

		case e_execution_graph_node_type::k_temporary_reference:
			wl_unreachable();
			break;

		default:
			wl_unreachable();
		}
	}

	wl_assert(edge_count == edge_count_verify);

	// Write edges
	writer.write(edge_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &from_node = m_nodes[index];
		for (size_t from_edge = 0; from_edge < from_node.outgoing_edge_references.size(); from_edge++) {
			c_node_reference to_node_reference = from_node.outgoing_edge_references[from_edge];
			const s_node &to_node = get_node(to_node_reference);

			size_t to_edge;
			for (to_edge = 0; to_edge < to_node.incoming_edge_references.size(); to_edge++) {
				if (to_node.incoming_edge_references[to_edge].get_node_index() == index) {
					break;
				}
			}

			wl_assert(valid_index(to_edge, to_node.incoming_edge_references.size()));

			// Write (a, b, outgoing_index, incoming_index)
			writer.write(index);
			writer.write(to_node_reference.get_node_index());
			writer.write(cast_integer_verify<uint32>(from_edge));
			writer.write(cast_integer_verify<uint32>(to_edge));
		}
	}

	// Write string table
	uint32 string_table_size = cast_integer_verify<uint32>(m_string_table.get_table_size());
	writer.write(string_table_size);
	out.write(m_string_table.get_table_pointer(), m_string_table.get_table_size());

	if (out.fail()) {
		return e_instrument_result::k_failed_to_write;
	}

	return e_instrument_result::k_success;
}

e_instrument_result c_execution_graph::load(std::ifstream &in) {
	wl_assert(m_nodes.empty());
	wl_assert(m_free_node_indices.empty());
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
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		node.salt = 0;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)

		switch (node.type) {
		case e_execution_graph_node_type::k_constant:
		{
			node.node_data.emplace<s_node::s_constant_node_data>();

			uint32 type_data;
			if (!reader.read(type_data)) {
				return invalid_graph_or_read_failure(in);
			}

			if (!read_data_type(type_data, node.constant_node_data().type)) {
				return e_instrument_result::k_invalid_graph;
			}

			if (node.constant_node_data().type.is_array()) {
				return e_instrument_result::k_invalid_graph;
			} else {
				switch (node.constant_node_data().type.get_primitive_type()) {
				case e_native_module_primitive_type::k_real:
					if (!reader.read(node.constant_node_data().real_value)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				case e_native_module_primitive_type::k_bool:
					if (!reader.read(node.constant_node_data().bool_value)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				case e_native_module_primitive_type::k_string:
					if (!reader.read(node.constant_node_data().string_index)) {
						return invalid_graph_or_read_failure(in);
					}
					break;

				default:
					return e_instrument_result::k_invalid_graph;
				}
			}

			break;
		}

		case e_execution_graph_node_type::k_array:
		{
			node.node_data.emplace<s_node::s_array_node_data>();

			uint32 type_data;
			if (!reader.read(type_data)) {
				return invalid_graph_or_read_failure(in);
			}

			if (!read_data_type(type_data, node.array_node_data().type)) {
				return e_instrument_result::k_invalid_graph;
			}

			if (!node.array_node_data().type.is_array()) {
				return e_instrument_result::k_invalid_graph;
			}

			break;
		}

		case e_execution_graph_node_type::k_native_module_call:
		{
			node.node_data.emplace<s_node::s_native_module_call_node_data>();

			// Read the native module UID and convert it to an index
			s_native_module_uid uid;
			if (!reader.read(uid)) {
				return invalid_graph_or_read_failure(in);
			}

			h_native_module native_module_handle = c_native_module_registry::get_native_module_handle(uid);
			if (!native_module_handle.is_valid()) {
				return e_instrument_result::k_unregistered_native_module;
			}

			node.native_module_call_node_data().native_module_handle = native_module_handle;
			break;
		}

		case e_execution_graph_node_type::k_indexed_input:
		case e_execution_graph_node_type::k_indexed_output:
			break;

		case e_execution_graph_node_type::k_input:
			node.node_data.emplace<s_node::s_input_node_data>();

			if (!reader.read(node.input_node_data().input_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		case e_execution_graph_node_type::k_output:
			node.node_data.emplace<s_node::s_output_node_data>();

			if (!reader.read(node.output_node_data().output_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		case e_execution_graph_node_type::k_temporary_reference:
			return e_instrument_result::k_invalid_graph;

		default:
			return e_instrument_result::k_invalid_graph;
		}

		m_nodes.push_back(node);
	}

	// Read edges
	uint32 edge_count;
	if (!reader.read(edge_count)) {
		return invalid_graph_or_read_failure(in);
	}

	for (uint32 index = 0; index < edge_count; index++) {
		uint32 from_index, to_index, from_edge, to_edge;
		if (!reader.read(from_index)
			|| !reader.read(to_index)
			|| !reader.read(from_edge)
			|| !reader.read(to_edge)) {
			return invalid_graph_or_read_failure(in);
		}

		c_node_reference from_node_reference(from_index SALT_ARG(0));
		c_node_reference to_node_reference(to_index SALT_ARG(0));
		if (!add_edge_for_load(from_node_reference, to_node_reference, from_edge, to_edge)) {
			return e_instrument_result::k_invalid_graph;
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
		return e_instrument_result::k_invalid_graph;
	}

	return e_instrument_result::k_success;
}

bool c_execution_graph::validate() const {
	size_t input_node_count = 0;
	size_t output_node_count = 0;

	// Validate each node first
	for (uint32 index = 0; index < get_node_count(); index++) {
		c_node_reference node_reference = node_reference_from_index(index);
		input_node_count += (get_node_type(node_reference) == e_execution_graph_node_type::k_input);
		output_node_count += (get_node_type(node_reference) == e_execution_graph_node_type::k_output);

		if (!validate_node(node_reference)
			|| get_node_type(node_reference) == e_execution_graph_node_type::k_temporary_reference) {
			return false;
		}
	}

	if (output_node_count == 0) {
		// Need at least one output node for the remain-active result
		return false;
	}

	// Validate each edge (requires nodes to be validated), check for cycles
	for (uint32 index = 0; index < get_node_count(); index++) {
		c_node_reference node_reference = node_reference_from_index(index);

		// Only bother validating outgoing edges, since it would be redundant to check both directions
		for (size_t edge = 0; edge < m_nodes[index].outgoing_edge_references.size(); edge++) {
			if (!validate_edge(node_reference, m_nodes[index].outgoing_edge_references[edge])) {
				return false;
			}
		}
	}

	// Input indices for the n input nodes should be unique and map to the range [0,n-1]. This is also true for output
	// nodes, but with one additional node for the remain-active result
	std::vector<bool> input_nodes_found(input_node_count, false);
	std::vector<bool> output_nodes_found(output_node_count - 1, false);
	bool remain_active_output_node_found = false;
	for (uint32 index = 0; index < get_node_count(); index++) {
		c_node_reference node_reference = node_reference_from_index(index);

		if (get_node_type(node_reference) == e_execution_graph_node_type::k_input) {
			uint32 input_index = get_input_node_input_index(node_reference);

			if (!valid_index(input_index, input_node_count)) {
				// Not in the range [0,n-1]
				return false;
			}

			if (input_nodes_found[input_index]) {
				// Duplicate input index
				return false;
			}

			input_nodes_found[input_index] = true;
		} else if (get_node_type(node_reference) == e_execution_graph_node_type::k_output) {
			uint32 output_index = get_output_node_output_index(node_reference);

			if (output_index == k_remain_active_output_index) {
				if (remain_active_output_node_found) {
					// Duplicate remain-active result
					return false;
				}

				remain_active_output_node_found = true;
			} else {
				if (!valid_index(output_index, output_node_count - 1)) {
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
		c_node_reference node_reference;
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			if (!nodes_visited[index]) {
				node_reference = node_reference_from_index(index);
				break;
			}
		}

		if (node_reference.is_valid()) {
			if (!visit_node_for_cycle_detection(node_reference, nodes_visited, nodes_marked)) {
				return false;
			}
		} else {
			break;
		}
	}

	if (!validate_native_modules()) {
		return false;
	}

	if (!validate_string_table()) {
		return false;
	}

	return true;
}

bool c_execution_graph::does_node_use_indexed_inputs(const s_node &node) {
	switch (node.type) {
	case e_execution_graph_node_type::k_constant:
		return false;

	case e_execution_graph_node_type::k_array:
		return true;

	case e_execution_graph_node_type::k_native_module_call:
		return true;

	case e_execution_graph_node_type::k_indexed_input:
	case e_execution_graph_node_type::k_indexed_output:
	case e_execution_graph_node_type::k_input:
	case e_execution_graph_node_type::k_output:
	case e_execution_graph_node_type::k_temporary_reference:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

bool c_execution_graph::does_node_use_indexed_outputs(const s_node &node) {
	switch (node.type) {
	case e_execution_graph_node_type::k_constant:
	case e_execution_graph_node_type::k_array:
		return false;

	case e_execution_graph_node_type::k_native_module_call:
		return true;

	case e_execution_graph_node_type::k_indexed_input:
	case e_execution_graph_node_type::k_indexed_output:
	case e_execution_graph_node_type::k_input:
	case e_execution_graph_node_type::k_output:
	case e_execution_graph_node_type::k_temporary_reference:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

c_node_reference c_execution_graph::allocate_node() {
	uint32 index;
	s_node *node;
	if (m_free_node_indices.empty()) {
		index = cast_integer_verify<uint32>(m_nodes.size());
		m_nodes.push_back(s_node());
		node = &m_nodes.back();
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		node->salt = 0;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	} else {
		index = m_free_node_indices.back();
		m_free_node_indices.pop_back();
		node = &m_nodes[index];
		wl_assert(node->type == e_execution_graph_node_type::k_invalid);
	}

	zero_type(&node->node_data);
	return c_node_reference(index SALT_ARG(node->salt));
}

c_node_reference c_execution_graph::node_reference_from_index(uint32 node_index) const {
	wl_assert(valid_index(node_index, m_nodes.size()));
	wl_assert(m_nodes[node_index].type != e_execution_graph_node_type::k_invalid);

#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	return c_node_reference(node_index, m_nodes[node_index].salt);
#else // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	return c_node_reference(node_index);
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
}

c_execution_graph::s_node &c_execution_graph::get_node(c_node_reference node_reference) {
	s_node &result = m_nodes[node_reference.get_node_index()];
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	wl_vassert(result.salt == node_reference.get_salt(), "Salt mismatch");
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	return result;
}

const c_execution_graph::s_node &c_execution_graph::get_node(c_node_reference node_reference) const {
	const s_node &result = m_nodes[node_reference.get_node_index()];
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	wl_vassert(result.salt == node_reference.get_salt(), "Salt mismatch");
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	return result;
}

c_node_reference c_execution_graph::add_constant_node(real32 constant_value) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().type = c_native_module_data_type(e_native_module_primitive_type::k_real);
	node.constant_node_data().real_value = constant_value;
	return node_reference;
}

c_node_reference c_execution_graph::add_constant_node(bool constant_value) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().type = c_native_module_data_type(e_native_module_primitive_type::k_bool);
	node.constant_node_data().bool_value = constant_value;
	return node_reference;
}

c_node_reference c_execution_graph::add_constant_node(const std::string &constant_value) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().type = c_native_module_data_type(e_native_module_primitive_type::k_string);
	node.constant_node_data().string_index = cast_integer_verify<uint32>(
		m_string_table.add_string(constant_value.c_str()));
	return node_reference;
}

c_node_reference c_execution_graph::add_array_node(c_native_module_data_type element_type) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_array;
	node.node_data.emplace<s_node::s_array_node_data>();
	// Will assert if there is not a valid array type (e.g. cannot make array of array)
	node.array_node_data().type = element_type.get_array_type();
	return node_reference;
}

void c_execution_graph::add_array_value(
	c_node_reference array_node_reference,
	c_node_reference value_node_reference) {
#if IS_TRUE(ASSERTS_ENABLED)
	const s_node &array_node = get_node(array_node_reference);
	wl_assert(array_node.type == e_execution_graph_node_type::k_array);
	c_native_module_data_type element_type = array_node.array_node_data().type.get_element_type();
	c_native_module_data_type value_node_type = get_node_data_type(value_node_reference);
	wl_assert(value_node_type.is_valid());
	wl_assert(element_type == value_node_type);
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Create an indexed input node to represent this array index
	c_node_reference input_node_reference = allocate_node();
	s_node &input_node = get_node(input_node_reference);
	input_node.type = e_execution_graph_node_type::k_indexed_input;
	add_edge_internal(input_node_reference, array_node_reference);
	add_edge_internal(value_node_reference, input_node_reference);
}

c_node_reference c_execution_graph::set_array_value_at_index(
	c_node_reference array_node_reference,
	uint32 index,
	c_node_reference value_node_reference) {
	const s_node &array_node = get_node(array_node_reference);

#if IS_TRUE(ASSERTS_ENABLED)
	wl_assert(array_node.type == e_execution_graph_node_type::k_array);
	c_native_module_data_type element_type = array_node.array_node_data().type.get_element_type();
	c_native_module_data_type value_node_type = get_node_data_type(value_node_reference);
	wl_assert(value_node_type.is_valid());
	wl_assert(element_type == value_node_type);
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Replace the input to the indexed input node at the given index
	c_node_reference input_node_reference = array_node.incoming_edge_references[index];
	c_node_reference old_value_node_reference = get_node_incoming_edge_reference(input_node_reference, 0);
	remove_edge_internal(old_value_node_reference, input_node_reference);
	add_edge_internal(value_node_reference, input_node_reference);
	return old_value_node_reference;
}

c_node_reference c_execution_graph::add_native_module_call_node(h_native_module native_module_handle) {
	wl_assert(native_module_handle.is_valid());

	c_node_reference node_reference = allocate_node();
	// Don't store a reference to the node, it gets invalidated if the vector is resized when adding inputs/outputs
	get_node(node_reference).type = e_execution_graph_node_type::k_native_module_call;
	get_node(node_reference).node_data.emplace<s_node::s_native_module_call_node_data>();
	get_node(node_reference).native_module_call_node_data().native_module_handle = native_module_handle;

	// Add nodes for each argument; the return value is output 0
	// Note that the incoming and outgoing edge lists are ordered by argument index so accessing the nth edge will give
	// us the nth indexed input/output node
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		c_node_reference argument_node_reference = allocate_node();
		s_node &argument_node = get_node(argument_node_reference);
		if (argument.argument_direction == e_native_module_argument_direction::k_in) {
			argument_node.type = e_execution_graph_node_type::k_indexed_input;
			add_edge_internal(argument_node_reference, node_reference);
		} else {
			wl_assert(argument.argument_direction == e_native_module_argument_direction::k_out);
			argument_node.type = e_execution_graph_node_type::k_indexed_output;
			add_edge_internal(node_reference, argument_node_reference);
		}
	}

	return node_reference;
}

c_node_reference c_execution_graph::add_input_node(uint32 input_index) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_input;
	node.node_data.emplace<s_node::s_input_node_data>();
	node.input_node_data().input_index = input_index;
	return node_reference;
}

c_node_reference c_execution_graph::add_output_node(uint32 output_index) {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_output;
	node.node_data.emplace<s_node::s_output_node_data>();
	node.output_node_data().output_index = output_index;
	return node_reference;
}

c_node_reference c_execution_graph::add_temporary_reference_node() {
	c_node_reference node_reference = allocate_node();
	s_node &node = get_node(node_reference);
	node.type = e_execution_graph_node_type::k_temporary_reference;
	return node_reference;
}

void c_execution_graph::remove_node(
	c_node_reference node_reference,
	f_on_node_removed on_node_removed,
	void *on_node_removed_context) {
	s_node &node = get_node(node_reference);

	if (does_node_use_indexed_inputs(node)) {
		// Remove input nodes - this will naturally break edges
		while (!node.incoming_edge_references.empty()) {
			remove_node(node.incoming_edge_references.back(), on_node_removed, on_node_removed_context);
		}
	} else {
		// Break edges
		while (!node.incoming_edge_references.empty()) {
			remove_edge_internal(node.incoming_edge_references.back(), node_reference);
		}
	}

	if (does_node_use_indexed_outputs(node)) {
		// Remove output nodes - this will naturally break edges
		while (!node.outgoing_edge_references.empty()) {
			remove_node(node.outgoing_edge_references.back(), on_node_removed, on_node_removed_context);
		}
	} else {
		// Break edges
		while (!node.outgoing_edge_references.empty()) {
			remove_edge_internal(node_reference, node.outgoing_edge_references.back());
		}
	}

	if (on_node_removed) {
		on_node_removed(on_node_removed_context, node_reference);
	}

	// Set to invalid and clear
	node.type = e_execution_graph_node_type::k_invalid;
	node.node_data.emplace<s_node::s_no_node_data>();
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	node.salt++;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	zero_type(&node.node_data);

	m_free_node_indices.push_back(node_reference.get_node_index());
}

void c_execution_graph::add_edge(c_node_reference from_reference, c_node_reference to_reference) {
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = get_node(to_reference);
	const s_node &from_node = get_node(from_reference);
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

	add_edge_internal(from_reference, to_reference);
}

void c_execution_graph::add_edge_internal(c_node_reference from_reference, c_node_reference to_reference) {
	s_node &from_node = get_node(from_reference);
	s_node &to_node = get_node(to_reference);

	// Add the edge only if it does not already exist
	bool exists = false;
	for (size_t from_out_index = 0;
		!exists && from_out_index < from_node.outgoing_edge_references.size();
		from_out_index++) {
		if (from_node.outgoing_edge_references[from_out_index] == to_reference) {
			exists = true;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	bool validate_exists = false;
	for (size_t to_in_index = 0;
		!validate_exists && to_in_index < to_node.incoming_edge_references.size();
		to_in_index++) {
		if (to_node.incoming_edge_references[to_in_index] == from_reference) {
			validate_exists = true;
		}
	}

	wl_assert(exists == validate_exists);
#endif // IS_TRUE(ASSERTS_ENABLED)

	if (!exists) {
		wl_assert(validate_edge(from_reference, to_reference));
		from_node.outgoing_edge_references.push_back(to_reference);
		to_node.incoming_edge_references.push_back(from_reference);
	}
}

bool c_execution_graph::add_edge_for_load(
	c_node_reference from_reference,
	c_node_reference to_reference,
	size_t from_edge,
	size_t to_edge) {
	// Don't perform validation, and return false if the edge is invalid or already exists
	if (!valid_index(from_reference.get_node_index(), m_nodes.size())
		|| !valid_index(to_reference.get_node_index(), m_nodes.size())) {
		return false;
	}

	s_node &from_node = get_node(from_reference);
	s_node &to_node = get_node(to_reference);

	for (size_t from_out_index = 0; from_out_index < from_node.outgoing_edge_references.size(); from_out_index++) {
		if (from_node.outgoing_edge_references[from_out_index] == to_reference) {
			return false;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t to_in_index = 0; to_in_index < to_node.incoming_edge_references.size(); to_in_index++) {
		// Validate that it doesn't exist in both directions
		wl_assert(to_node.incoming_edge_references[to_in_index] != from_reference);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	from_node.outgoing_edge_references.resize(std::max(from_node.outgoing_edge_references.size(), from_edge + 1));
	from_node.outgoing_edge_references[from_edge] = to_reference;

	to_node.incoming_edge_references.resize(std::max(to_node.incoming_edge_references.size(), to_edge + 1));
	to_node.incoming_edge_references[to_edge] = from_reference;

	return true;
}

void c_execution_graph::remove_edge(c_node_reference from_reference, c_node_reference to_reference) {
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = get_node(to_reference);
	const s_node &from_node = get_node(from_reference);
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

	remove_edge_internal(from_reference, to_reference);
}

void c_execution_graph::remove_edge_internal(c_node_reference from_reference, c_node_reference to_reference) {
	s_node &from_node = get_node(from_reference);
	s_node &to_node = get_node(to_reference);

	// Remove the edge only if it exists
	bool found_out = false;
	for (size_t from_out_index = 0;
		!found_out && from_out_index < from_node.outgoing_edge_references.size();
		from_out_index++) {
		if (from_node.outgoing_edge_references[from_out_index] == to_reference) {
			from_node.outgoing_edge_references.erase(from_node.outgoing_edge_references.begin() + from_out_index);
			found_out = true;
		}
	}

	bool found_in = false;
	for (size_t to_in_index = 0;
		!found_in && to_in_index < to_node.incoming_edge_references.size();
		to_in_index++) {
		if (to_node.incoming_edge_references[to_in_index] == from_reference) {
			to_node.incoming_edge_references.erase(to_node.incoming_edge_references.begin() + to_in_index);
			found_in = true;
		}
	}

	// They should either both have been found, or both have not been found
	wl_assert(found_out == found_in);
}

bool c_execution_graph::validate_node(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);

	// Validate edge counts. To validate each edge individually use validate_edge.
	switch (node.type) {
	case e_execution_graph_node_type::k_constant:
		// We don't need to check here that the type itself it valid because if it isn't the graph will fail to load
		return node.incoming_edge_references.empty();

	case e_execution_graph_node_type::k_array:
		// We don't need to check here that the type itself it valid because if it isn't the graph will fail to load
		return true; // Arrays can have inputs

	case e_execution_graph_node_type::k_native_module_call:
	{
		if (!valid_index(node.native_module_call_node_data().native_module_handle.get_data(), 
			c_native_module_registry::get_native_module_count())) {
			return false;
		}

		const s_native_module &native_module =
			c_native_module_registry::get_native_module(node.native_module_call_node_data().native_module_handle);

		return (node.incoming_edge_references.size() == native_module.in_argument_count)
			&& (node.outgoing_edge_references.size() == native_module.out_argument_count);
	}

	case e_execution_graph_node_type::k_indexed_input:
		return (node.incoming_edge_references.size() == 1)
			&& (node.outgoing_edge_references.size() == 1);

	case e_execution_graph_node_type::k_indexed_output:
		return
			(node.incoming_edge_references.size() == 1);

	case e_execution_graph_node_type::k_input:
		return
			(node.incoming_edge_references.empty());

	case e_execution_graph_node_type::k_output:
		return (node.incoming_edge_references.size() == 1)
			&& node.outgoing_edge_references.empty();

	case e_execution_graph_node_type::k_temporary_reference:
		return true;

	default:
		// Unknown type
		return false;
	}
}

bool c_execution_graph::validate_edge(c_node_reference from_reference, c_node_reference to_reference) const {
	if (!valid_index(from_reference.get_node_index(), m_nodes.size())
		|| !valid_index(to_reference.get_node_index(), m_nodes.size())) {
		return false;
	}

	bool check_type = false;
	const s_node &from_node = get_node(from_reference);
	const s_node &to_node = get_node(to_reference);

	switch (from_node.type) {
	case e_execution_graph_node_type::k_constant:
	case e_execution_graph_node_type::k_array:
		if ((to_node.type != e_execution_graph_node_type::k_indexed_input)
			&& (to_node.type != e_execution_graph_node_type::k_output)
			&& (to_node.type != e_execution_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_execution_graph_node_type::k_temporary_reference;
		break;

	case e_execution_graph_node_type::k_native_module_call:
		if (to_node.type != e_execution_graph_node_type::k_indexed_output) {
			return false;
		}
		// Don't check type for native module calls
		break;

	case e_execution_graph_node_type::k_indexed_input:
		if (!does_node_use_indexed_inputs(to_node)) {
			return false;
		}
		// Don't check type for indexed inputs
		break;

	case e_execution_graph_node_type::k_indexed_output:
		if ((to_node.type != e_execution_graph_node_type::k_indexed_input)
			&& (to_node.type != e_execution_graph_node_type::k_output)
			&& (to_node.type != e_execution_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_execution_graph_node_type::k_temporary_reference;
		break;

	case e_execution_graph_node_type::k_input:
		if ((to_node.type != e_execution_graph_node_type::k_indexed_input)
			&& (to_node.type != e_execution_graph_node_type::k_output)
			&& (to_node.type != e_execution_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_execution_graph_node_type::k_temporary_reference;
		break;

	case e_execution_graph_node_type::k_output:
		return false;

	case e_execution_graph_node_type::k_temporary_reference:
		// This type effectively acts as a temporary output
		return false;

	default:
		// Unknown type
		return false;
	}

	if (check_type) {
		// Note: this assumes that all nodes have been validated
		c_native_module_data_type from_type = get_node_data_type(from_reference);
		c_native_module_data_type to_type = get_node_data_type(to_reference);
		if (!from_type.is_valid() || !to_type.is_valid() || from_type != to_type) {
			return false;
		}
	}

	return true;
}

bool c_execution_graph::validate_native_modules() const {
	// This should only be called after all nodes and edges have been validated
	for (uint32 node_index = 0; node_index < m_nodes.size(); node_index++) {
		c_node_reference node_reference = node_reference_from_index(node_index);
		if (get_node_type(node_reference) != e_execution_graph_node_type::k_native_module_call) {
			continue;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			get_native_module_call_native_module_handle(node_reference));

		bool always_runs_at_compile_time;
		bool always_runs_at_compile_time_if_dependent_constants_are_constant;
		get_native_module_compile_time_properties(
			native_module,
			always_runs_at_compile_time,
			always_runs_at_compile_time_if_dependent_constants_are_constant);

		if (always_runs_at_compile_time) {
			// This native module should have run when we compiled
			return false;
		}

		// Check to see if all dependent-constant inputs are constants
		bool all_dependent_constants_are_constant = true;

		wl_assert(native_module.in_argument_count == get_node_incoming_edge_count(node_reference));

		// For each constant input, verify that a constant node is linked up
		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			s_native_module_argument argument = native_module.arguments[arg];
			if (argument.argument_direction == e_native_module_argument_direction::k_in) {
				c_node_reference source_node_reference =
					get_node_indexed_input_incoming_edge_reference(node_reference, input, 0);

				e_native_module_data_mutability data_mutability = argument.type.get_data_mutability();
				if (data_mutability == e_native_module_data_mutability::k_constant) {
					// Validate that this input is constant
					if (!is_node_constant(source_node_reference)) {
						return false;
					}
				} else if (data_mutability == e_native_module_data_mutability::k_dependent_constant) {
					if (!is_node_constant(source_node_reference)) {
						all_dependent_constants_are_constant = false;
						break;
					}
				}

				input++;
			}
		}

		wl_assert(input == native_module.in_argument_count);

		if (always_runs_at_compile_time_if_dependent_constants_are_constant && all_dependent_constants_are_constant) {
			// Since all dependent-constants are constant, this native module should have run when we compiled
			return false;
		}
	}

	return true;
}

bool c_execution_graph::validate_string_table() const {
	// If we have any strings, they should be null-terminated
	if (m_string_table.get_table_size() > 0
		&& m_string_table.get_table_pointer()[m_string_table.get_table_size()-1] != '\0') {
		return false;
	}

	// Each constant string node should be pointing to a valid string
	for (size_t index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];

		if (node.type != e_execution_graph_node_type::k_constant
			|| node.constant_node_data().type != c_native_module_data_type(e_native_module_primitive_type::k_string)) {
			continue;
		}

		if (!valid_index(node.constant_node_data().string_index, m_string_table.get_table_size())) {
			return false;
		}
	}

	return true;
}

bool c_execution_graph::visit_node_for_cycle_detection(
	c_node_reference node_reference,
	std::vector<bool> &nodes_visited,
	std::vector<bool> &nodes_marked) const {
	if (nodes_marked[node_reference.get_node_index()]) {
		// Cycle
		return false;
	}

	if (!nodes_visited[node_reference.get_node_index()]) {
		nodes_marked[node_reference.get_node_index()] = true;

		const s_node &node = m_nodes[node_reference.get_node_index()];
		for (size_t edge = 0; edge < node.outgoing_edge_references.size(); edge++) {
			if (!visit_node_for_cycle_detection(node.outgoing_edge_references[edge], nodes_visited, nodes_marked)) {
				return false;
			}
		}

		nodes_marked[node_reference.get_node_index()] = false;
		nodes_visited[node_reference.get_node_index()] = true;
	}

	return true;
}

void c_execution_graph::remove_unused_strings() {
	// We could be more efficient about this, but it's easier to just build another table and swap
	c_string_table new_string_table;
	for (size_t index = 0; index < m_nodes.size(); index++) {
		s_node &node = m_nodes[index];

		if (node.type != e_execution_graph_node_type::k_constant
			|| node.constant_node_data().type != c_native_module_data_type(e_native_module_primitive_type::k_string)) {
			continue;
		}

		uint32 new_string_index = cast_integer_verify<uint32>(new_string_table.add_string(
			m_string_table.get_string(node.constant_node_data().string_index)));
		node.constant_node_data().string_index = new_string_index;
	}

	m_string_table.swap(new_string_table);
}

uint32 c_execution_graph::get_node_count() const {
	return cast_integer_verify<uint32>(m_nodes.size());
}

c_node_reference c_execution_graph::nodes_begin() const {
	uint32 index = 0;
	while (index < m_nodes.size()) {
		if (m_nodes[index].type != e_execution_graph_node_type::k_invalid) {
			return c_node_reference(index SALT_ARG(m_nodes[index].salt));
		}

		index++;
	}

	return c_node_reference();
}

c_node_reference c_execution_graph::nodes_next(c_node_reference node_reference) const {
	uint32 index = node_reference.get_node_index() + 1;
	while (index < m_nodes.size()) {
		if (m_nodes[index].type != e_execution_graph_node_type::k_invalid) {
			return c_node_reference(index SALT_ARG(m_nodes[index].salt));
		}

		index++;
	}

	return c_node_reference();
}

e_execution_graph_node_type c_execution_graph::get_node_type(c_node_reference node_reference) const {
	return get_node(node_reference).type;
}

c_native_module_data_type c_execution_graph::get_node_data_type(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);

	switch (node.type) {
	case e_execution_graph_node_type::k_constant:
		return node.constant_node_data().type;

	case e_execution_graph_node_type::k_array:
		return node.array_node_data().type;

	case e_execution_graph_node_type::k_native_module_call:
		wl_vhalt("We shouldn't be checking types for this node, but instead for its argument nodes");
		return c_native_module_data_type::invalid();

	case e_execution_graph_node_type::k_indexed_input:
	{
		// Find the index of this input
		if (node.outgoing_edge_references.size() != 1) {
			return c_native_module_data_type::invalid();
		}

		c_node_reference dest_node_reference = node.outgoing_edge_references[0];
		if (!valid_index(dest_node_reference.get_node_index(), m_nodes.size())) {
			return c_native_module_data_type::invalid();
		}

		const s_node &dest_node = get_node(dest_node_reference);

		if (dest_node.type == e_execution_graph_node_type::k_array) {
			return dest_node.array_node_data().type.get_element_type();
		} else if (dest_node.type == e_execution_graph_node_type::k_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.native_module_call_node_data().native_module_handle);

			size_t input = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module.arguments[arg].argument_direction == e_native_module_argument_direction::k_in) {
					if (dest_node.incoming_edge_references[input] == node_reference) {
						// We've found the matching argument - return its type
						return native_module.arguments[arg].type.get_data_type();
					}
					input++;
				}
			}
		}

		return c_native_module_data_type::invalid();
	}

	case e_execution_graph_node_type::k_indexed_output:
	{
		// Find the index of this output
		if (node.incoming_edge_references.size() != 1) {
			return c_native_module_data_type::invalid();
		}

		c_node_reference dest_node_reference = node.incoming_edge_references[0];
		if (!valid_index(dest_node_reference.get_node_index(), m_nodes.size())) {
			return c_native_module_data_type::invalid();
		}

		const s_node &dest_node = get_node(dest_node_reference);

		if (dest_node.type == e_execution_graph_node_type::k_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.native_module_call_node_data().native_module_handle);

			size_t output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module.arguments[arg].argument_direction == e_native_module_argument_direction::k_out) {
					if (dest_node.outgoing_edge_references[output] == node_reference) {
						// We've found the matching argument - return its type
						return native_module.arguments[arg].type.get_data_type();
					}
					output++;
				}
			}
		}

		return c_native_module_data_type::invalid();
	}

	case e_execution_graph_node_type::k_input:
		return c_native_module_data_type(e_native_module_primitive_type::k_real);

	case e_execution_graph_node_type::k_output:
		if (node.output_node_data().output_index == k_remain_active_output_index) {
			return c_native_module_data_type(e_native_module_primitive_type::k_bool);
		} else {
			return c_native_module_data_type(e_native_module_primitive_type::k_real);
		}

	case e_execution_graph_node_type::k_temporary_reference:
		wl_vhalt("Temporary reference nodes don't have a type, they just exist to prevent other nodes from deletion");
		return c_native_module_data_type::invalid();

	default:
		// Unknown type
		return c_native_module_data_type::invalid();
	}
}

bool c_execution_graph::is_node_constant(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	if (node.type == e_execution_graph_node_type::k_constant) {
		return true;
	}

	if (node.type == e_execution_graph_node_type::k_array) {
		for (size_t edge = 0; edge < node.incoming_edge_references.size(); edge++) {
			c_node_reference source_node_reference =
				get_node_indexed_input_incoming_edge_reference(node_reference, edge, 0);
			if (!is_node_constant(source_node_reference)) {
				return false;
			}
		}
	}

	return true;
}

real32 c_execution_graph::get_constant_node_real_value(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().type == c_native_module_data_type(e_native_module_primitive_type::k_real));
	return node.constant_node_data().real_value;
}

bool c_execution_graph::get_constant_node_bool_value(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().type == c_native_module_data_type(e_native_module_primitive_type::k_bool));
	return node.constant_node_data().bool_value;
}

const char *c_execution_graph::get_constant_node_string_value(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().type == c_native_module_data_type(e_native_module_primitive_type::k_string));
	return m_string_table.get_string(node.constant_node_data().string_index);
}

h_native_module c_execution_graph::get_native_module_call_native_module_handle(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_native_module_call);
	return node.native_module_call_node_data().native_module_handle;
}

uint32 c_execution_graph::get_input_node_input_index(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_input);
	return node.input_node_data().input_index;
}

uint32 c_execution_graph::get_output_node_output_index(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	wl_assert(node.type == e_execution_graph_node_type::k_output);
	return node.output_node_data().output_index;
}

size_t c_execution_graph::get_node_incoming_edge_count(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	return node.incoming_edge_references.size();
}

c_node_reference c_execution_graph::get_node_incoming_edge_reference(
	c_node_reference node_reference,
	size_t edge) const {
	const s_node &node = get_node(node_reference);
	return node.incoming_edge_references[edge];
}

size_t c_execution_graph::get_node_outgoing_edge_count(c_node_reference node_reference) const {
	const s_node &node = get_node(node_reference);
	return node.outgoing_edge_references.size();
}

c_node_reference c_execution_graph::get_node_outgoing_edge_reference(
	c_node_reference node_reference,
	size_t edge) const {
	const s_node &node = get_node(node_reference);
	return node.outgoing_edge_references[edge];
}

bool c_execution_graph::does_node_use_indexed_inputs(c_node_reference node_reference) const {
	return does_node_use_indexed_inputs(get_node(node_reference));
}

bool c_execution_graph::does_node_use_indexed_outputs(c_node_reference node_reference) const {
	return does_node_use_indexed_outputs(get_node(node_reference));
}

size_t c_execution_graph::get_node_indexed_input_incoming_edge_count(
	c_node_reference node_reference,
	size_t input_index) const {
	wl_assert(does_node_use_indexed_inputs(node_reference));
	return get_node_incoming_edge_count(get_node_incoming_edge_reference(node_reference, input_index));
}

c_node_reference c_execution_graph::get_node_indexed_input_incoming_edge_reference(
	c_node_reference node_reference,
	size_t input_index,
	size_t edge) const {
	wl_assert(does_node_use_indexed_inputs(node_reference));
	return get_node_incoming_edge_reference(get_node_incoming_edge_reference(node_reference, input_index), edge);
}

size_t c_execution_graph::get_node_indexed_output_outgoing_edge_count(
	c_node_reference node_reference,
	size_t output_index) const {
	wl_assert(does_node_use_indexed_outputs(node_reference));
	return get_node_outgoing_edge_count(get_node_outgoing_edge_reference(node_reference, output_index));
}

c_node_reference c_execution_graph::get_node_indexed_output_outgoing_edge_reference(
	c_node_reference node_reference,
	size_t output_index,
	size_t edge) const {
	wl_assert(does_node_use_indexed_outputs(node_reference));
	return get_node_outgoing_edge_reference(get_node_outgoing_edge_reference(node_reference, output_index), edge);
}

void c_execution_graph::remove_unused_nodes_and_reassign_node_indices() {
	std::vector<c_node_reference> old_to_new_references(m_nodes.size());

	uint32 next_new_index = 0;
	for (uint32 old_index = 0; old_index < m_nodes.size(); old_index++) {
		if (m_nodes[old_index].type == e_execution_graph_node_type::k_invalid) {
			// This node was removed, don't assign it a new index
		} else {
			// Compress nodes down - use the move operator if possible to avoid copying edge list unnecessarily
			std::swap(m_nodes[old_index], m_nodes[next_new_index]);

			// Reset salt to 0 since we're shifting everything around anyway
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
			m_nodes[next_new_index].salt = 0;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)

			old_to_new_references[old_index] = c_node_reference(next_new_index SALT_ARG(0));
			next_new_index++;
		}
	}

	m_nodes.resize(next_new_index);
	m_free_node_indices.clear();

	// Remap edge indices
	for (size_t old_index = 0; old_index < m_nodes.size(); old_index++) {
		s_node &node = m_nodes[old_index];
		for (uint32 in = 0; in < node.incoming_edge_references.size(); in++) {
			node.incoming_edge_references[in] =
				old_to_new_references[node.incoming_edge_references[in].get_node_index()];
			wl_assert(node.incoming_edge_references[in].is_valid());
		}
		for (uint32 out = 0; out < node.outgoing_edge_references.size(); out++) {
			node.outgoing_edge_references[out] =
				old_to_new_references[node.outgoing_edge_references[out].get_node_index()];
			wl_assert(node.outgoing_edge_references[out].is_valid());
		}
	}

	remove_unused_strings();
}

bool c_execution_graph::generate_graphviz_file(const char *fname, bool collapse_large_arrays) const {
	static constexpr uint32 k_large_array_limit = 8;

	std::vector<bool> arrays_to_collapse;
	std::vector<bool> nodes_to_skip;

	if (collapse_large_arrays) {
		arrays_to_collapse.resize(m_nodes.size());
		nodes_to_skip.resize(m_nodes.size());

		// Find and mark all large constant arrays
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			c_node_reference node_reference = node_reference_from_index(index);
			const s_node &node = get_node(node_reference);

			if (node.type == e_execution_graph_node_type::k_array
				&& node.incoming_edge_references.size() >= k_large_array_limit
				&& is_node_constant(node_reference)) {
				arrays_to_collapse[node_reference.get_node_index()] = true;

				// Mark each input as an array to collapse so that checking the constants below is easier. Also
				// mark it as a node to skip, so we skip it during output.
				for (size_t input = 0; input < node.incoming_edge_references.size(); input++) {
					c_node_reference input_node_reference = node.incoming_edge_references[input];
					arrays_to_collapse[input_node_reference.get_node_index()] = true;
					nodes_to_skip[input_node_reference.get_node_index()] = true;
				}
			}
		}

		// Find and mark all constants which are inputs to only the nodes we just marked
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			c_node_reference node_reference = node_reference_from_index(index);
			const s_node &node = get_node(node_reference);

			if (node.type == e_execution_graph_node_type::k_constant) {
				bool all_outputs_marked = true;
				for (size_t output = 0; all_outputs_marked && output < node.outgoing_edge_references.size(); output++) {
					c_node_reference output_node_reference = node.outgoing_edge_references[output];
					all_outputs_marked = arrays_to_collapse[output_node_reference.get_node_index()];
				}

				if (all_outputs_marked) {
					nodes_to_skip[node_reference.get_node_index()] = true;
				}
			}
		}
	}

	c_graphviz_generator graph;
	graph.set_graph_name("execution_graph");

	size_t collapsed_nodes = 0;

	// Declare the nodes
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		c_node_reference node_reference = node_reference_from_index(index);
		const s_node &node = get_node(node_reference);
		c_graphviz_node graph_node;

		if (collapse_large_arrays && nodes_to_skip[node_reference.get_node_index()]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		switch (node.type) {
		case e_execution_graph_node_type::k_constant:
		{
			graph_node.set_shape("ellipse");
			std::string label;
			switch (node.constant_node_data().type.get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				label = format_real_for_graph_output(node.constant_node_data().real_value);
				break;

			case e_native_module_primitive_type::k_bool:
				label = node.constant_node_data().bool_value ? "true" : "false";
				break;

			case e_native_module_primitive_type::k_string:
				label =
					"\\\"" + std::string(m_string_table.get_string(node.constant_node_data().string_index)) + "\\\"";
				break;

			default:
				wl_unreachable();
			}

			graph_node.set_label(label.c_str());
			break;
		}

		case e_execution_graph_node_type::k_array:
		{
			graph_node.set_shape("ellipse");
			std::string label = node.array_node_data().type.to_string();
			graph_node.set_label(label.c_str());
			break;
		}

		case e_execution_graph_node_type::k_native_module_call:
			graph_node.set_shape("box");
			graph_node.set_label(c_native_module_registry::get_native_module(
				node.native_module_call_node_data().native_module_handle).name.get_string());
			break;

		case e_execution_graph_node_type::k_indexed_input:
		{
			wl_assert(node.outgoing_edge_references.size() == 1);
			const s_node &root_node = get_node(node.outgoing_edge_references[0]);
			uint32 input_index;
			for (input_index = 0; input_index < root_node.incoming_edge_references.size(); input_index++) {
				if (root_node.incoming_edge_references[input_index] == node_reference) {
					break;
				}
			}

			wl_assert(valid_index(input_index, root_node.incoming_edge_references.size()));
			graph_node.set_shape("house");
			graph_node.set_orientation(180.0f);
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(input_index)).c_str());
			break;
		}

		case e_execution_graph_node_type::k_indexed_output:
		{
			wl_assert(node.incoming_edge_references.size() == 1);
			const s_node &root_node = get_node(node.incoming_edge_references[0]);
			uint32 output_index;
			for (output_index = 0; output_index < root_node.outgoing_edge_references.size(); output_index++) {
				if (root_node.outgoing_edge_references[output_index] == node_reference) {
					break;
				}
			}

			wl_assert(valid_index(output_index, root_node.outgoing_edge_references.size()));
			graph_node.set_shape("house");
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(output_index)).c_str());
			break;
		}

		case e_execution_graph_node_type::k_input:
			graph_node.set_shape("box");
			graph_node.set_label(("input " + std::to_string(node.input_node_data().input_index)).c_str());
			graph_node.set_style("rounded");
			break;

		case e_execution_graph_node_type::k_output:
			graph_node.set_shape("box");
			{
				std::string output_index_string;
				if (node.output_node_data().output_index == k_remain_active_output_index) {
					output_index_string = "remain-active";
				} else {
					output_index_string = std::to_string(node.output_node_data().output_index);
				}
				graph_node.set_label(("output " + output_index_string).c_str());
			}
			graph_node.set_style("rounded");
			break;

		case e_execution_graph_node_type::k_temporary_reference:
			wl_unreachable();
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
			collapsed_array_node.set_label(("[" + std::to_string(node.incoming_edge_references.size()) + "]").c_str());
			graph.add_node(collapsed_array_node);

			collapsed_nodes++;

			// Add the edge
			graph.add_edge(collapsed_array_node_name.c_str(), node_name.c_str());
		}
	}

	// Declare the edges
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		c_node_reference node_reference = node_reference_from_index(index);

		if (collapse_large_arrays && nodes_to_skip[node_reference.get_node_index()]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		const s_node &node = get_node(node_reference);
		for (size_t edge = 0; edge < node.outgoing_edge_references.size(); edge++) {
			c_node_reference to_reference = node.outgoing_edge_references[edge];
			if (collapse_large_arrays && nodes_to_skip[to_reference.get_node_index()]) {
				// This is a node which has been collapsed - skip it
				continue;
			}

			graph.add_edge(
				("node_" + std::to_string(node_reference.get_node_index())).c_str(),
				("node_" + std::to_string(to_reference.get_node_index())).c_str());
		}
	}

	return graph.output_to_file(fname);
}
