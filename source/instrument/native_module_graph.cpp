#include "common/utility/file_utility.h"
#include "common/utility/graphviz_generator.h"

#include "instrument/native_module_graph.h"
#include "instrument/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <string>

#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
#define NODE_SALT(salt) , (salt)
#else // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
#define NODE_SALT(salt)
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)

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

c_native_module_graph::c_node_iterator::c_iterand::c_iterand(
	const c_native_module_graph &graph,
	h_graph_node node_handle)
	: m_graph(graph)
	, m_node_handle(node_handle) {}

bool c_native_module_graph::c_node_iterator::c_iterand::operator!=(const c_iterand &other) const {
	return m_node_handle != other.m_node_handle;
}

c_native_module_graph::c_node_iterator::c_iterand &c_native_module_graph::c_node_iterator::c_iterand::operator++() {
	m_node_handle = m_graph.get_next_node(m_node_handle);
	return *this;
}

h_graph_node c_native_module_graph::c_node_iterator::c_iterand::operator*() const {
	return m_node_handle;
}

c_native_module_graph::c_node_iterator::c_iterand c_native_module_graph::c_node_iterator::begin() const {
	return c_iterand(m_graph, m_graph.get_next_node(h_graph_node::invalid()));
}

c_native_module_graph::c_node_iterator::c_iterand c_native_module_graph::c_node_iterator::end() const {
	return c_iterand(m_graph, h_graph_node::invalid());
}

c_native_module_graph::c_node_iterator::c_node_iterator(const c_native_module_graph &graph)
	: m_graph(graph) {}

c_native_module_graph::c_native_module_graph() {}

e_instrument_result c_native_module_graph::save(std::ofstream &out) const {
	wl_assert(validate());

#if IS_TRUE(ASSERTS_ENABLED)
	// Make sure there are no invalid or temporary reference nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		wl_assert(m_nodes[index].type != e_native_module_graph_node_type::k_invalid);
		wl_assert(m_nodes[index].type != e_native_module_graph_node_type::k_temporary_reference);
	}

	wl_assert(m_free_node_indices.empty());
#endif // IS_TRUE(ASSERTS_ENABLED)

	c_binary_file_writer writer(out);

	// Write the output latency
	writer.write(m_output_latency);

	// Write the node count, and also count up all edges
	uint32 node_count = cast_integer_verify<uint32>(m_nodes.size());
	uint32 edge_count = 0;
#if IS_TRUE(ASSERTS_ENABLED)
	uint32 edge_count_verify = 0;
#endif // IS_TRUE(ASSERTS_ENABLED)
	writer.write(node_count);
	for (uint32 index = 0; index < node_count; index++) {
		const s_node &node = m_nodes[index];
		edge_count += cast_integer_verify<uint32>(node.outgoing_edge_handles.size());
#if IS_TRUE(ASSERTS_ENABLED)
		edge_count_verify += cast_integer_verify<uint32>(node.incoming_edge_handles.size());
#endif // IS_TRUE(ASSERTS_ENABLED)

		uint32 node_type = static_cast<uint32>(node.type);
		writer.write(node_type);

		switch (node.type) {
		case e_native_module_graph_node_type::k_constant:
			writer.write(static_cast<uint8>(node.constant_node_data().primitive_type));
			switch (node.constant_node_data().primitive_type) {
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

		case e_native_module_graph_node_type::k_array:
			break;

		case e_native_module_graph_node_type::k_native_module_call:
		{
			// Save out the UID, not the index
			const s_native_module &native_module =
				c_native_module_registry::get_native_module(node.native_module_call_node_data().native_module_handle);
			writer.write(native_module.uid);
			writer.write(node.native_module_call_node_data().upsample_factor);
			break;
		}

		case e_native_module_graph_node_type::k_indexed_input:
		case e_native_module_graph_node_type::k_indexed_output:
			break;

		case e_native_module_graph_node_type::k_input:
			writer.write(node.input_node_data().input_index);
			break;

		case e_native_module_graph_node_type::k_output:
			writer.write(node.output_node_data().output_index);
			break;

		case e_native_module_graph_node_type::k_temporary_reference:
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
		for (size_t from_edge = 0; from_edge < from_node.outgoing_edge_handles.size(); from_edge++) {
			h_graph_node to_node_handle = from_node.outgoing_edge_handles[from_edge];
			const s_node &to_node = get_node(to_node_handle);

			size_t to_edge;
			for (to_edge = 0; to_edge < to_node.incoming_edge_handles.size(); to_edge++) {
				if (to_node.incoming_edge_handles[to_edge].get_data().index == index) {
					break;
				}
			}

			wl_assert(valid_index(to_edge, to_node.incoming_edge_handles.size()));

			// Write (a, b, outgoing_index, incoming_index)
			writer.write(index);
			writer.write(to_node_handle.get_data().index);
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

e_instrument_result c_native_module_graph::load(std::ifstream &in) {
	wl_assert(m_nodes.empty());
	wl_assert(m_free_node_indices.empty());
	wl_assert(m_string_table.get_table_size() == 0);

	c_binary_file_reader reader(in);

	if (!reader.read(m_output_latency)
		|| m_output_latency < 0) {
		return invalid_graph_or_read_failure(in);
	}

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

		node.type = static_cast<e_native_module_graph_node_type>(node_type);
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		node.salt = 0;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)

		switch (node.type) {
		case e_native_module_graph_node_type::k_constant:
		{
			node.node_data.emplace<s_node::s_constant_node_data>();

			uint8 primitive_type;
			if (!reader.read(primitive_type)
				|| !valid_enum_index(static_cast<e_native_module_primitive_type>(primitive_type))) {
				return invalid_graph_or_read_failure(in);
			}

			node.constant_node_data().primitive_type = static_cast<e_native_module_primitive_type>(primitive_type);
			switch (node.constant_node_data().primitive_type) {
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

			break;
		}

		case e_native_module_graph_node_type::k_array:
			break;

		case e_native_module_graph_node_type::k_native_module_call:
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

			uint32 upsample_factor;
			if (!reader.read(upsample_factor)) {
				return invalid_graph_or_read_failure(in);
			}

			node.native_module_call_node_data() = { native_module_handle, upsample_factor };
			break;
		}

		case e_native_module_graph_node_type::k_indexed_input:
		case e_native_module_graph_node_type::k_indexed_output:
			break;

		case e_native_module_graph_node_type::k_input:
			node.node_data.emplace<s_node::s_input_node_data>();

			if (!reader.read(node.input_node_data().input_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		case e_native_module_graph_node_type::k_output:
			node.node_data.emplace<s_node::s_output_node_data>();

			if (!reader.read(node.output_node_data().output_index)) {
				return invalid_graph_or_read_failure(in);
			}
			break;

		case e_native_module_graph_node_type::k_temporary_reference:
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

		h_graph_node from_node_handle = h_graph_node::construct({ from_index NODE_SALT(0) });
		h_graph_node to_node_handle = h_graph_node::construct({ to_index NODE_SALT(0) });
		if (!add_edge_for_load(from_node_handle, to_node_handle, from_edge, to_edge)) {
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

bool c_native_module_graph::validate() const {
	size_t input_node_count = 0;
	size_t output_node_count = 0;

	// Validate each node first
	for (uint32 index = 0; index < get_node_count(); index++) {
		h_graph_node node_handle = node_handle_from_index(index);
		input_node_count += (get_node_type(node_handle) == e_native_module_graph_node_type::k_input);
		output_node_count += (get_node_type(node_handle) == e_native_module_graph_node_type::k_output);

		if (!validate_node(node_handle)
			|| get_node_type(node_handle) == e_native_module_graph_node_type::k_temporary_reference) {
			return false;
		}
	}

	if (output_node_count == 0) {
		// Need at least one output node for the remain-active result
		return false;
	}

	// Validate each edge (requires nodes to be validated), check for cycles
	for (uint32 index = 0; index < get_node_count(); index++) {
		h_graph_node node_handle = node_handle_from_index(index);

		// Only bother validating outgoing edges, since it would be redundant to check both directions
		for (h_graph_node edge_node_handle : m_nodes[index].outgoing_edge_handles) {
			if (!validate_edge(node_handle, edge_node_handle)) {
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
		h_graph_node node_handle = node_handle_from_index(index);

		if (get_node_type(node_handle) == e_native_module_graph_node_type::k_input) {
			uint32 input_index = get_input_node_input_index(node_handle);

			if (!valid_index(input_index, input_node_count)) {
				// Not in the range [0,n-1]
				return false;
			}

			if (input_nodes_found[input_index]) {
				// Duplicate input index
				return false;
			}

			input_nodes_found[input_index] = true;
		} else if (get_node_type(node_handle) == e_native_module_graph_node_type::k_output) {
			uint32 output_index = get_output_node_output_index(node_handle);

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
		h_graph_node node_handle = h_graph_node::invalid();
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			if (!nodes_visited[index]) {
				node_handle = node_handle_from_index(index);
				break;
			}
		}

		if (node_handle.is_valid()) {
			if (!visit_node_for_cycle_detection(node_handle, nodes_visited, nodes_marked)) {
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

bool c_native_module_graph::does_node_use_indexed_inputs(const s_node &node) {
	switch (node.type) {
	case e_native_module_graph_node_type::k_constant:
		return false;

	case e_native_module_graph_node_type::k_array:
		return true;

	case e_native_module_graph_node_type::k_native_module_call:
		return true;

	case e_native_module_graph_node_type::k_indexed_input:
	case e_native_module_graph_node_type::k_indexed_output:
	case e_native_module_graph_node_type::k_input:
	case e_native_module_graph_node_type::k_output:
	case e_native_module_graph_node_type::k_temporary_reference:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

bool c_native_module_graph::does_node_use_indexed_outputs(const s_node &node) {
	switch (node.type) {
	case e_native_module_graph_node_type::k_constant:
	case e_native_module_graph_node_type::k_array:
		return false;

	case e_native_module_graph_node_type::k_native_module_call:
		return true;

	case e_native_module_graph_node_type::k_indexed_input:
	case e_native_module_graph_node_type::k_indexed_output:
	case e_native_module_graph_node_type::k_input:
	case e_native_module_graph_node_type::k_output:
	case e_native_module_graph_node_type::k_temporary_reference:
		return false;

	default:
		wl_unreachable();
		return false;
	}
}

h_graph_node c_native_module_graph::allocate_node() {
	uint32 index;
	s_node *node;
	if (m_free_node_indices.empty()) {
		index = cast_integer_verify<uint32>(m_nodes.size());
		m_nodes.push_back(s_node());
		node = &m_nodes.back();
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		node->salt = 0;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	} else {
		index = m_free_node_indices.back();
		m_free_node_indices.pop_back();
		node = &m_nodes[index];
		wl_assert(node->type == e_native_module_graph_node_type::k_invalid);
	}

	zero_type(&node->node_data);
	return h_graph_node::construct({ index NODE_SALT(node->salt) });
}

h_graph_node c_native_module_graph::node_handle_from_index(uint32 node_index) const {
	wl_assert(valid_index(node_index, m_nodes.size()));
	wl_assert(m_nodes[node_index].type != e_native_module_graph_node_type::k_invalid);
	return h_graph_node::construct({ node_index NODE_SALT(m_nodes[node_index].salt) });
}

c_native_module_graph::s_node &c_native_module_graph::get_node(h_graph_node node_handle) {
	s_node &result = m_nodes[node_handle.get_data().index];
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	wl_assertf(result.salt == node_handle.get_data().salt, "Salt mismatch");
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return result;
}

const c_native_module_graph::s_node &c_native_module_graph::get_node(h_graph_node node_handle) const {
	const s_node &result = m_nodes[node_handle.get_data().index];
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	wl_assertf(result.salt == node_handle.get_data().salt, "Salt mismatch");
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return result;
}

h_graph_node c_native_module_graph::add_constant_node(real32 constant_value) {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().primitive_type = e_native_module_primitive_type::k_real;
	node.constant_node_data().real_value = constant_value;
	return node_handle;
}

h_graph_node c_native_module_graph::add_constant_node(bool constant_value) {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().primitive_type = e_native_module_primitive_type::k_bool;
	node.constant_node_data().bool_value = constant_value;
	return node_handle;
}

h_graph_node c_native_module_graph::add_constant_node(const char *constant_value) {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_constant;
	node.node_data.emplace<s_node::s_constant_node_data>();
	node.constant_node_data().primitive_type = e_native_module_primitive_type::k_string;
	node.constant_node_data().string_index = cast_integer_verify<uint32>(m_string_table.add_string(constant_value));
	return node_handle;
}

h_graph_node c_native_module_graph::add_array_node() {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_array;
	return node_handle;
}

void c_native_module_graph::add_array_value(
	h_graph_node array_node_handle,
	h_graph_node value_node_handle) {
#if IS_TRUE(ASSERTS_ENABLED)
	const s_node &array_node = get_node(array_node_handle);
	wl_assert(array_node.type == e_native_module_graph_node_type::k_array);

	// Make sure the element is compatible. This must consider constness and upsample factor. When we add a new element
	// to an array, the array type gets "downgraded" - e.g. if the array was previously constant and the new element
	// is not constant, the array becomes non-constant. Therefore, when checking for type compatibility, we need to
	// check whether the new value type is assignable to the most downgraded possible array type.
	c_native_module_qualified_data_type array_type = get_node_data_type(array_node_handle);
	if (array_type.is_empty_array()) {
		// Nothing further to check - we can assign any element type to an empty array
	} else {
		c_native_module_qualified_data_type element_type = array_type.get_element_type();
		c_native_module_qualified_data_type downgraded_element_type =
			element_type.change_data_mutability(e_native_module_data_mutability::k_variable);
		c_native_module_qualified_data_type value_node_type = get_node_data_type(value_node_handle);
		wl_assert(value_node_type.is_valid());
		wl_assert(is_native_module_data_type_assignable(value_node_type, downgraded_element_type));
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Create an indexed input node to represent this array index
	h_graph_node input_node_handle = allocate_node();
	s_node &input_node = get_node(input_node_handle);
	input_node.type = e_native_module_graph_node_type::k_indexed_input;

	add_edge_internal(input_node_handle, array_node_handle);
	add_edge_internal(value_node_handle, input_node_handle);
}

h_graph_node c_native_module_graph::set_array_value_at_index(
	h_graph_node array_node_handle,
	uint32 index,
	h_graph_node value_node_handle) {
	const s_node &array_node = get_node(array_node_handle);

#if IS_TRUE(ASSERTS_ENABLED)
	wl_assert(array_node.type == e_native_module_graph_node_type::k_array);

	// See comment in add_array_value() for type compatibility validation details
	c_native_module_qualified_data_type element_type = get_node_data_type(array_node_handle).get_element_type();
	c_native_module_qualified_data_type value_node_type = get_node_data_type(value_node_handle);
	wl_assert(value_node_type.is_valid());
	wl_assert(is_native_module_data_type_assignable(element_type, value_node_type));
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Replace the input to the indexed input node at the given index
	h_graph_node input_node_handle = array_node.incoming_edge_handles[index];
	h_graph_node old_value_node_handle = get_node_incoming_edge_handle(input_node_handle, 0);
	remove_edge_internal(old_value_node_handle, input_node_handle);
	add_edge_internal(value_node_handle, input_node_handle);

	return old_value_node_handle;
}

h_graph_node c_native_module_graph::add_native_module_call_node(
	h_native_module native_module_handle,
	uint32 upsample_factor) {
	wl_assert(native_module_handle.is_valid());
	wl_assert(upsample_factor > 0);

	h_graph_node node_handle = allocate_node();
	// Don't store a handle to the node, it gets invalidated if the vector is resized when adding inputs/outputs
	get_node(node_handle).type = e_native_module_graph_node_type::k_native_module_call;
	get_node(node_handle).node_data.emplace<s_node::s_native_module_call_node_data>();
	get_node(node_handle).native_module_call_node_data() = { native_module_handle, upsample_factor };

	// Add nodes for each argument; the return value is output 0
	// Note that the incoming and outgoing edge lists are ordered by argument index so accessing the nth edge will give
	// us the nth indexed input/output node
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		s_native_module_argument argument = native_module.arguments[arg];
		h_graph_node argument_node_handle = allocate_node();
		s_node &argument_node = get_node(argument_node_handle);
		if (argument.argument_direction == e_native_module_argument_direction::k_in) {
			argument_node.type = e_native_module_graph_node_type::k_indexed_input;
			add_edge_internal(argument_node_handle, node_handle);
		} else {
			wl_assert(argument.argument_direction == e_native_module_argument_direction::k_out);
			argument_node.type = e_native_module_graph_node_type::k_indexed_output;
			add_edge_internal(node_handle, argument_node_handle);
		}
	}

	return node_handle;
}

h_graph_node c_native_module_graph::add_input_node(uint32 input_index) {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_input;
	node.node_data.emplace<s_node::s_input_node_data>();
	node.input_node_data().input_index = input_index;
	return node_handle;
}

h_graph_node c_native_module_graph::add_output_node(uint32 output_index) {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_output;
	node.node_data.emplace<s_node::s_output_node_data>();
	node.output_node_data().output_index = output_index;
	return node_handle;
}

h_graph_node c_native_module_graph::add_temporary_reference_node() {
	h_graph_node node_handle = allocate_node();
	s_node &node = get_node(node_handle);
	node.type = e_native_module_graph_node_type::k_temporary_reference;
	return node_handle;
}

void c_native_module_graph::remove_node(h_graph_node node_handle) {
	s_node &node = get_node(node_handle);

	if (does_node_use_indexed_inputs(node)) {
		// Remove input nodes - this will naturally break edges
		while (!node.incoming_edge_handles.empty()) {
			remove_node(node.incoming_edge_handles.back());
		}
	} else {
		// Break edges
		while (!node.incoming_edge_handles.empty()) {
			remove_edge_internal(node.incoming_edge_handles.back(), node_handle);
		}
	}

	if (does_node_use_indexed_outputs(node)) {
		// Remove output nodes - this will naturally break edges
		while (!node.outgoing_edge_handles.empty()) {
			remove_node(node.outgoing_edge_handles.back());
		}
	} else {
		// Break edges
		while (!node.outgoing_edge_handles.empty()) {
			remove_edge_internal(node_handle, node.outgoing_edge_handles.back());
		}
	}

	// Set to invalid and clear
	node.type = e_native_module_graph_node_type::k_invalid;
	node.node_data.emplace<s_node::s_no_node_data>();
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	node.salt++;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	zero_type(&node.node_data);

	m_free_node_indices.push_back(node_handle.get_data().index);
}

void c_native_module_graph::add_edge(h_graph_node from_handle, h_graph_node to_handle) {
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = get_node(to_handle);
	const s_node &from_node = get_node(from_handle);
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

	add_edge_internal(from_handle, to_handle);
}

void c_native_module_graph::add_edge_internal(h_graph_node from_handle, h_graph_node to_handle) {
	s_node &from_node = get_node(from_handle);
	s_node &to_node = get_node(to_handle);

	// Add the edge only if it does not already exist
	bool exists = false;
	for (h_graph_node edge_node_handle : from_node.outgoing_edge_handles) {
		if (edge_node_handle == to_handle) {
			exists = true;
			break;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	bool validate_exists = false;
	for (h_graph_node edge_node_handle : to_node.incoming_edge_handles) {
		if (edge_node_handle == from_handle) {
			validate_exists = true;
			break;
		}
	}

	wl_assert(exists == validate_exists);
#endif // IS_TRUE(ASSERTS_ENABLED)

	if (!exists) {
		from_node.outgoing_edge_handles.push_back(to_handle);
		to_node.incoming_edge_handles.push_back(from_handle);

		// Validate after adding the edge because some nodes derive their type from the connected edges
		wl_assert(validate_edge(from_handle, to_handle));
	}
}

bool c_native_module_graph::add_edge_for_load(
	h_graph_node from_handle,
	h_graph_node to_handle,
	size_t from_edge,
	size_t to_edge) {
	// Don't perform validation, and return false if the edge is invalid or already exists
	if (!valid_index(from_handle.get_data().index, m_nodes.size())
		|| !valid_index(to_handle.get_data().index, m_nodes.size())) {
		return false;
	}

	s_node &from_node = get_node(from_handle);
	s_node &to_node = get_node(to_handle);

	for (h_graph_node edge_node_handle : from_node.outgoing_edge_handles) {
		if (edge_node_handle == to_handle) {
			return false;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (h_graph_node edge_node_handle : to_node.incoming_edge_handles) {
		// Validate that it doesn't exist in both directions
		wl_assert(edge_node_handle != from_handle);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	from_node.outgoing_edge_handles.resize(std::max(from_node.outgoing_edge_handles.size(), from_edge + 1));
	from_node.outgoing_edge_handles[from_edge] = to_handle;

	to_node.incoming_edge_handles.resize(std::max(to_node.incoming_edge_handles.size(), to_edge + 1));
	to_node.incoming_edge_handles[to_edge] = from_handle;

	return true;
}

void c_native_module_graph::remove_edge(h_graph_node from_handle, h_graph_node to_handle) {
#if IS_TRUE(ASSERTS_ENABLED)
	// The user-facing function should never touch nodes which use indexed inputs/outputs directly. Instead, the user
	// should always be creating edges to/from indexed inputs/outputs.
	const s_node &to_node = get_node(to_handle);
	const s_node &from_node = get_node(from_handle);
	wl_assert(!does_node_use_indexed_inputs(to_node));
	wl_assert(!does_node_use_indexed_outputs(from_node));
#endif // IS_TRUE(ASSERTS_ENABLED)

	remove_edge_internal(from_handle, to_handle);
}

void c_native_module_graph::remove_edge_internal(h_graph_node from_handle, h_graph_node to_handle) {
	s_node &from_node = get_node(from_handle);
	s_node &to_node = get_node(to_handle);

	// Remove the edge only if it exists
	bool found_out = false;
	for (size_t from_out_index = 0; from_out_index < from_node.outgoing_edge_handles.size(); from_out_index++) {
		if (from_node.outgoing_edge_handles[from_out_index] == to_handle) {
			from_node.outgoing_edge_handles.erase(from_node.outgoing_edge_handles.begin() + from_out_index);
			found_out = true;
			break;
		}
	}

	bool found_in = false;
	for (size_t to_in_index = 0; to_in_index < to_node.incoming_edge_handles.size(); to_in_index++) {
		if (to_node.incoming_edge_handles[to_in_index] == from_handle) {
			to_node.incoming_edge_handles.erase(to_node.incoming_edge_handles.begin() + to_in_index);
			found_in = true;
			break;
		}
	}

	// They should either both have been found, or both have not been found
	wl_assert(found_out == found_in);
}

bool c_native_module_graph::validate_node(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);

	// Make sure each edge at least points to a valid node index
	for (h_graph_node edge_node : node.incoming_edge_handles) {
		if (edge_node.get_data().index >= m_nodes.size()) {
			return false;
		}
	}

	for (h_graph_node edge_node : node.outgoing_edge_handles) {
		if (edge_node.get_data().index >= m_nodes.size()) {
			return false;
		}
	}

	// Validate edge counts. To validate each edge individually use validate_edge.
	switch (node.type) {
	case e_native_module_graph_node_type::k_constant:
		// We don't need to check here that the type itself it valid because if it isn't the graph will fail to load
		return node.incoming_edge_handles.empty();

	case e_native_module_graph_node_type::k_array:
		// We don't need to check here that the type itself it valid because if it isn't the graph will fail to load
		return true; // Arrays can have inputs

	case e_native_module_graph_node_type::k_native_module_call:
	{
		if (!valid_index(node.native_module_call_node_data().native_module_handle.get_data(), 
			c_native_module_registry::get_native_module_count())) {
			return false;
		}

		const s_native_module &native_module =
			c_native_module_registry::get_native_module(node.native_module_call_node_data().native_module_handle);

		size_t in_argument_count = 0;
		size_t out_argument_count = 0;
		for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
			e_native_module_argument_direction argument_direction =
				native_module.arguments[argument_index].argument_direction;
			if (argument_direction == e_native_module_argument_direction::k_in) {
				in_argument_count++;
			} else {
				wl_assert(argument_direction == e_native_module_argument_direction::k_out);
				out_argument_count++;
			}
		}

		return (node.incoming_edge_handles.size() == in_argument_count)
			&& (node.outgoing_edge_handles.size() == out_argument_count);
	}

	case e_native_module_graph_node_type::k_indexed_input:
		return (node.incoming_edge_handles.size() == 1)
			&& (node.outgoing_edge_handles.size() == 1);

	case e_native_module_graph_node_type::k_indexed_output:
		return
			(node.incoming_edge_handles.size() == 1);

	case e_native_module_graph_node_type::k_input:
		return
			(node.incoming_edge_handles.empty());

	case e_native_module_graph_node_type::k_output:
		return (node.incoming_edge_handles.size() == 1)
			&& node.outgoing_edge_handles.empty();

	case e_native_module_graph_node_type::k_temporary_reference:
		return true;

	default:
		// Unknown type
		return false;
	}
}

bool c_native_module_graph::validate_edge(h_graph_node from_handle, h_graph_node to_handle) const {
	if (!valid_index(from_handle.get_data().index, m_nodes.size())
		|| !valid_index(to_handle.get_data().index, m_nodes.size())) {
		return false;
	}

	bool check_type = false;
	const s_node &from_node = get_node(from_handle);
	const s_node &to_node = get_node(to_handle);

	switch (from_node.type) {
	case e_native_module_graph_node_type::k_constant:
	case e_native_module_graph_node_type::k_array:
		if ((to_node.type != e_native_module_graph_node_type::k_indexed_input)
			&& (to_node.type != e_native_module_graph_node_type::k_output)
			&& (to_node.type != e_native_module_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_native_module_graph_node_type::k_temporary_reference;
		break;

	case e_native_module_graph_node_type::k_native_module_call:
		if (to_node.type != e_native_module_graph_node_type::k_indexed_output) {
			return false;
		}
		// Don't check type for native module calls
		break;

	case e_native_module_graph_node_type::k_indexed_input:
		if (!does_node_use_indexed_inputs(to_node)) {
			return false;
		}
		// Don't check type for indexed inputs
		break;

	case e_native_module_graph_node_type::k_indexed_output:
		if ((to_node.type != e_native_module_graph_node_type::k_indexed_input)
			&& (to_node.type != e_native_module_graph_node_type::k_output)
			&& (to_node.type != e_native_module_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_native_module_graph_node_type::k_temporary_reference;
		break;

	case e_native_module_graph_node_type::k_input:
		if ((to_node.type != e_native_module_graph_node_type::k_indexed_input)
			&& (to_node.type != e_native_module_graph_node_type::k_output)
			&& (to_node.type != e_native_module_graph_node_type::k_temporary_reference)) {
			return false;
		}
		check_type = to_node.type != e_native_module_graph_node_type::k_temporary_reference;
		break;

	case e_native_module_graph_node_type::k_output:
		return false;

	case e_native_module_graph_node_type::k_temporary_reference:
		// This type effectively acts as a temporary output
		return false;

	default:
		// Unknown type
		return false;
	}

	if (check_type) {
		// Note: this assumes that all nodes have been validated
		c_native_module_qualified_data_type from_type = get_node_data_type(from_handle);
		c_native_module_qualified_data_type to_type = get_node_data_type(to_handle);

		if (!from_type.is_valid() || !to_type.is_valid()) {
			return false;
		}

		if (!is_native_module_data_type_assignable(from_type, to_type)) {
			return false;
		}
	}

	return true;
}

bool c_native_module_graph::validate_native_modules() const {
	// This should only be called after all nodes and edges have been validated
	for (uint32 node_index = 0; node_index < m_nodes.size(); node_index++) {
		h_graph_node node_handle = node_handle_from_index(node_index);
		if (get_node_type(node_handle) != e_native_module_graph_node_type::k_native_module_call) {
			continue;
		}

		const s_native_module &native_module = c_native_module_registry::get_native_module(
			get_native_module_call_node_native_module_handle(node_handle));

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

#if IS_TRUE(ASSERTS_ENABLED)
		size_t in_argument_count = 0;
		for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
			e_native_module_argument_direction argument_direction =
				native_module.arguments[argument_index].argument_direction;
			if (argument_direction == e_native_module_argument_direction::k_in) {
				in_argument_count++;
			} else {
				wl_assert(argument_direction == e_native_module_argument_direction::k_out);
			}
		}
#endif // IS_TRUE(ASSERTS_ENABLED)

		wl_assert(in_argument_count == get_node_incoming_edge_count(node_handle));

		// For each constant input, verify that a constant node is linked up
		size_t input = 0;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			s_native_module_argument argument = native_module.arguments[arg];
			if (argument.argument_direction == e_native_module_argument_direction::k_in) {
				h_graph_node source_node_handle =
					get_node_indexed_input_incoming_edge_handle(node_handle, input, 0);

				e_native_module_data_mutability data_mutability = argument.type.get_data_mutability();
				c_native_module_qualified_data_type node_data_type = get_node_data_type(source_node_handle);
				if (data_mutability == e_native_module_data_mutability::k_constant) {
					// Validate that this input is constant
					if (node_data_type.get_data_mutability() != e_native_module_data_mutability::k_constant) {
						return false;
					}
				} else if (data_mutability == e_native_module_data_mutability::k_dependent_constant) {
					if (node_data_type.get_data_mutability() != e_native_module_data_mutability::k_constant) {
						all_dependent_constants_are_constant = false;
					}
				}

				input++;
			}
		}

		wl_assert(input == in_argument_count);

		if (always_runs_at_compile_time_if_dependent_constants_are_constant && all_dependent_constants_are_constant) {
			// Since all dependent-constants are constant, this native module should have run when we compiled
			return false;
		}
	}

	return true;
}

bool c_native_module_graph::validate_string_table() const {
	// If we have any strings, they should be null-terminated
	if (m_string_table.get_table_size() > 0
		&& m_string_table.get_table_pointer()[m_string_table.get_table_size()-1] != '\0') {
		return false;
	}

	// Each constant string node should be pointing to a valid string
	for (size_t index = 0; index < m_nodes.size(); index++) {
		const s_node &node = m_nodes[index];

		if (node.type != e_native_module_graph_node_type::k_constant
			|| node.constant_node_data().primitive_type != e_native_module_primitive_type::k_string) {
			continue;
		}

		if (!valid_index(node.constant_node_data().string_index, m_string_table.get_table_size())) {
			return false;
		}
	}

	return true;
}

bool c_native_module_graph::visit_node_for_cycle_detection(
	h_graph_node node_handle,
	std::vector<bool> &nodes_visited,
	std::vector<bool> &nodes_marked) const {
	if (nodes_marked[node_handle.get_data().index]) {
		// Cycle
		return false;
	}

	if (!nodes_visited[node_handle.get_data().index]) {
		nodes_marked[node_handle.get_data().index] = true;

		const s_node &node = m_nodes[node_handle.get_data().index];
		for (h_graph_node edge_node_handle : node.outgoing_edge_handles) {
			if (!visit_node_for_cycle_detection(edge_node_handle, nodes_visited, nodes_marked)) {
				return false;
			}
		}

		nodes_marked[node_handle.get_data().index] = false;
		nodes_visited[node_handle.get_data().index] = true;
	}

	return true;
}

void c_native_module_graph::remove_unused_strings() {
	// We could be more efficient about this, but it's easier to just build another table and swap
	c_string_table new_string_table;
	for (size_t index = 0; index < m_nodes.size(); index++) {
		s_node &node = m_nodes[index];

		if (node.type != e_native_module_graph_node_type::k_constant
			|| node.constant_node_data().primitive_type != e_native_module_primitive_type::k_string) {
			continue;
		}

		uint32 new_string_index = cast_integer_verify<uint32>(new_string_table.add_string(
			m_string_table.get_string(node.constant_node_data().string_index)));
		node.constant_node_data().string_index = new_string_index;
	}

	m_string_table.swap(new_string_table);
}

uint32 c_native_module_graph::get_node_count() const {
	return cast_integer_verify<uint32>(m_nodes.size());
}

c_native_module_graph::c_node_iterator c_native_module_graph::iterate_nodes() const {
	return c_node_iterator(*this);
}

h_graph_node c_native_module_graph::get_next_node(h_graph_node node_handle) const {
	uint32 index = node_handle.is_valid() ? node_handle.get_data().index + 1 : 0;
	while (index < m_nodes.size()) {
		if (m_nodes[index].type != e_native_module_graph_node_type::k_invalid) {
			return h_graph_node::construct({ index NODE_SALT(m_nodes[index].salt) });
		}

		index++;
	}

	return h_graph_node::invalid();
}

e_native_module_graph_node_type c_native_module_graph::get_node_type(h_graph_node node_handle) const {
	return get_node(node_handle).type;
}

c_native_module_qualified_data_type c_native_module_graph::get_node_data_type(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);

	switch (node.type) {
	case e_native_module_graph_node_type::k_constant:
		return c_native_module_qualified_data_type(
			c_native_module_data_type(node.constant_node_data().primitive_type, false, 1),
			e_native_module_data_mutability::k_constant);

	case e_native_module_graph_node_type::k_array:
	{
		// $TODO restructure validation. It should be something like: validate nodes, validate edge indices, validate
		// edge node type correctness, validate edge data type correctness. Right now, because arrays derive their type
		// from a recursive call to get_node_data_type(), an invalid graph could potentially cause a loop.
		if (node.incoming_edge_handles.empty()) {
			return c_native_module_qualified_data_type::empty_array();
		}

		e_native_module_primitive_type primitive_type = e_native_module_primitive_type::k_invalid;
		uint32 upsample_factor = 1;
		e_native_module_data_mutability data_mutability = e_native_module_data_mutability::k_constant;
		for (size_t edge = 0; edge < node.incoming_edge_handles.size(); edge++) {
			// Because this is called at validation time, we can't assume that the input edges are of the correct node
			// type, so get_node_indexed_input_incoming_edge_handle() is unsafe
			const s_node &input_node = get_node(node.incoming_edge_handles[edge]);
			if (input_node.type != e_native_module_graph_node_type::k_indexed_input
				|| input_node.incoming_edge_handles.size() != 1) {
				return c_native_module_qualified_data_type::invalid();
			}

			h_graph_node source_node_handle = input_node.incoming_edge_handles[0];
			c_native_module_qualified_data_type element_data_type = get_node_data_type(source_node_handle);

			if (primitive_type == e_native_module_primitive_type::k_invalid) {
				primitive_type = element_data_type.get_primitive_type();
			} else if (element_data_type.get_primitive_type() != primitive_type) {
				return c_native_module_qualified_data_type::invalid();
			}

			if (element_data_type.get_data_mutability() != e_native_module_data_mutability::k_constant) {
				if (data_mutability == e_native_module_data_mutability::k_constant) {
					// Downgrade the type if we run into a non-constant
					upsample_factor = element_data_type.get_upsample_factor();
					data_mutability = element_data_type.get_data_mutability();
				} else if (element_data_type.get_upsample_factor() != upsample_factor) {
					// If we've downgraded, make sure the upsample factor matches
					return c_native_module_qualified_data_type::invalid();
				}
			}
		}

		return c_native_module_qualified_data_type(
			c_native_module_data_type(primitive_type, true, upsample_factor),
			data_mutability);
	}

	case e_native_module_graph_node_type::k_native_module_call:
		wl_haltf("We shouldn't be checking types for this node, but instead for its argument nodes");
		return c_native_module_qualified_data_type::invalid();

	case e_native_module_graph_node_type::k_indexed_input:
	{
		// Find the index of this input
		if (node.outgoing_edge_handles.size() != 1) {
			return c_native_module_qualified_data_type::invalid();
		}

		h_graph_node dest_node_handle = node.outgoing_edge_handles[0];
		if (!valid_index(dest_node_handle.get_data().index, m_nodes.size())) {
			return c_native_module_qualified_data_type::invalid();
		}

		const s_node &dest_node = get_node(dest_node_handle);

		if (dest_node.type == e_native_module_graph_node_type::k_array) {
			return get_node_data_type(dest_node_handle).get_element_type();
		} else if (dest_node.type == e_native_module_graph_node_type::k_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.native_module_call_node_data().native_module_handle);

			size_t input = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module.arguments[arg].argument_direction == e_native_module_argument_direction::k_in) {
					if (dest_node.incoming_edge_handles[input] == node_handle) {
						// We've found the matching argument - return its type.
						c_native_module_qualified_data_type data_type =
							native_module.arguments[arg].type.get_upsampled_type(
								dest_node.native_module_call_node_data().upsample_factor);

						if (data_type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant) {
							data_type = c_native_module_qualified_data_type(
								data_type.get_data_type(),
								e_native_module_data_mutability::k_variable);
						}

						return data_type;
					}
					input++;
				}
			}
		}

		return c_native_module_qualified_data_type::invalid();
	}

	case e_native_module_graph_node_type::k_indexed_output:
	{
		// Find the index of this output
		if (node.incoming_edge_handles.size() != 1) {
			return c_native_module_qualified_data_type::invalid();
		}

		h_graph_node dest_node_handle = node.incoming_edge_handles[0];
		if (!valid_index(dest_node_handle.get_data().index, m_nodes.size())) {
			return c_native_module_qualified_data_type::invalid();
		}

		const s_node &dest_node = get_node(dest_node_handle);

		if (dest_node.type == e_native_module_graph_node_type::k_native_module_call) {
			const s_native_module &native_module = c_native_module_registry::get_native_module(
				dest_node.native_module_call_node_data().native_module_handle);

			size_t output = 0;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (native_module.arguments[arg].argument_direction == e_native_module_argument_direction::k_out) {
					if (dest_node.outgoing_edge_handles[output] == node_handle) {
						// We've found the matching argument - return its type
						c_native_module_qualified_data_type data_type =
							native_module.arguments[arg].type.get_upsampled_type(
								dest_node.native_module_call_node_data().upsample_factor);

						if (data_type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant) {
							data_type = c_native_module_qualified_data_type(
								data_type.get_data_type(),
								e_native_module_data_mutability::k_variable);
						}

						return data_type;
					}
					output++;
				}
			}
		}

		return c_native_module_qualified_data_type::invalid();
	}

	case e_native_module_graph_node_type::k_input:
		return c_native_module_qualified_data_type(
			c_native_module_data_type(e_native_module_primitive_type::k_real, false, 1),
			e_native_module_data_mutability::k_variable);

	case e_native_module_graph_node_type::k_output:
		if (node.output_node_data().output_index == k_remain_active_output_index) {
			return c_native_module_qualified_data_type(
				c_native_module_data_type(e_native_module_primitive_type::k_bool, false, 1),
				e_native_module_data_mutability::k_variable);
		} else {
			return c_native_module_qualified_data_type(
				c_native_module_data_type(e_native_module_primitive_type::k_real, false, 1),
				e_native_module_data_mutability::k_variable);
		}

	case e_native_module_graph_node_type::k_temporary_reference:
		wl_haltf("Temporary reference nodes don't have a type, they just exist to prevent other nodes from deletion");
		return c_native_module_qualified_data_type::invalid();

	default:
		// Unknown type
		return c_native_module_qualified_data_type::invalid();
	}
}

real32 c_native_module_graph::get_constant_node_real_value(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().primitive_type == e_native_module_primitive_type::k_real);
	return node.constant_node_data().real_value;
}

bool c_native_module_graph::get_constant_node_bool_value(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().primitive_type == e_native_module_primitive_type::k_bool);
	return node.constant_node_data().bool_value;
}

const char *c_native_module_graph::get_constant_node_string_value(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_constant);
	wl_assert(node.constant_node_data().primitive_type == e_native_module_primitive_type::k_string);
	return m_string_table.get_string(node.constant_node_data().string_index);
}

h_native_module c_native_module_graph::get_native_module_call_node_native_module_handle(
	h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_native_module_call);
	return node.native_module_call_node_data().native_module_handle;
}

uint32 c_native_module_graph::get_native_module_call_node_upsample_factor(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_native_module_call);
	return node.native_module_call_node_data().upsample_factor;
}

uint32 c_native_module_graph::get_input_node_input_index(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_input);
	return node.input_node_data().input_index;
}

uint32 c_native_module_graph::get_output_node_output_index(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	wl_assert(node.type == e_native_module_graph_node_type::k_output);
	return node.output_node_data().output_index;
}

size_t c_native_module_graph::get_node_incoming_edge_count(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	return node.incoming_edge_handles.size();
}

h_graph_node c_native_module_graph::get_node_incoming_edge_handle(h_graph_node node_handle, size_t edge) const {
	const s_node &node = get_node(node_handle);
	return node.incoming_edge_handles[edge];
}

size_t c_native_module_graph::get_node_outgoing_edge_count(h_graph_node node_handle) const {
	const s_node &node = get_node(node_handle);
	return node.outgoing_edge_handles.size();
}

h_graph_node c_native_module_graph::get_node_outgoing_edge_handle(h_graph_node node_handle, size_t edge) const {
	const s_node &node = get_node(node_handle);
	return node.outgoing_edge_handles[edge];
}

bool c_native_module_graph::does_node_use_indexed_inputs(h_graph_node node_handle) const {
	return does_node_use_indexed_inputs(get_node(node_handle));
}

bool c_native_module_graph::does_node_use_indexed_outputs(h_graph_node node_handle) const {
	return does_node_use_indexed_outputs(get_node(node_handle));
}

size_t c_native_module_graph::get_node_indexed_input_incoming_edge_count(
	h_graph_node node_handle,
	size_t input_index) const {
	wl_assert(does_node_use_indexed_inputs(node_handle));
	return get_node_incoming_edge_count(get_node_incoming_edge_handle(node_handle, input_index));
}

h_graph_node c_native_module_graph::get_node_indexed_input_incoming_edge_handle(
	h_graph_node node_handle,
	size_t input_index,
	size_t edge) const {
	wl_assert(does_node_use_indexed_inputs(node_handle));
	return get_node_incoming_edge_handle(get_node_incoming_edge_handle(node_handle, input_index), edge);
}

size_t c_native_module_graph::get_node_indexed_output_outgoing_edge_count(
	h_graph_node node_handle,
	size_t output_index) const {
	wl_assert(does_node_use_indexed_outputs(node_handle));
	return get_node_outgoing_edge_count(get_node_outgoing_edge_handle(node_handle, output_index));
}

h_graph_node c_native_module_graph::get_node_indexed_output_outgoing_edge_handle(
	h_graph_node node_handle,
	size_t output_index,
	size_t edge) const {
	wl_assert(does_node_use_indexed_outputs(node_handle));
	return get_node_outgoing_edge_handle(get_node_outgoing_edge_handle(node_handle, output_index), edge);
}

void c_native_module_graph::set_output_latency(int32 output_latency) {
	wl_assert(output_latency >= 0);
	m_output_latency = output_latency;
}

int32 c_native_module_graph::get_output_latency() const {
	return m_output_latency;
}

void c_native_module_graph::remove_unused_nodes_and_reassign_node_indices() {
	std::vector<h_graph_node> old_to_new_handles(m_nodes.size());

	uint32 next_new_index = 0;
	for (uint32 old_index = 0; old_index < m_nodes.size(); old_index++) {
		if (m_nodes[old_index].type == e_native_module_graph_node_type::k_invalid) {
			// This node was removed, don't assign it a new index
		} else {
			// Compress nodes down - use the move operator if possible to avoid copying edge list unnecessarily
			std::swap(m_nodes[old_index], m_nodes[next_new_index]);

			// Reset salt to 0 since we're shifting everything around anyway
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
			m_nodes[next_new_index].salt = 0;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)

			old_to_new_handles[old_index] = h_graph_node::construct({ next_new_index NODE_SALT(0) });
			next_new_index++;
		}
	}

	m_nodes.resize(next_new_index);
	m_free_node_indices.clear();

	// Remap edge indices
	for (size_t old_index = 0; old_index < m_nodes.size(); old_index++) {
		s_node &node = m_nodes[old_index];
		for (h_graph_node &edge_node_handle : node.incoming_edge_handles) {
			edge_node_handle = old_to_new_handles[edge_node_handle.get_data().index];
			wl_assert(edge_node_handle.is_valid());
		}

		for (h_graph_node &edge_node_handle : node.outgoing_edge_handles) {
			edge_node_handle = old_to_new_handles[edge_node_handle.get_data().index];
			wl_assert(edge_node_handle.is_valid());
		}
	}

	remove_unused_strings();
}

bool c_native_module_graph::generate_graphviz_file(const char *fname, bool collapse_large_arrays) const {
	static constexpr uint32 k_large_array_limit = 8;

	std::vector<bool> arrays_to_collapse;
	std::vector<bool> nodes_to_skip;

	if (collapse_large_arrays) {
		arrays_to_collapse.resize(m_nodes.size());
		nodes_to_skip.resize(m_nodes.size());

		// Find and mark all large constant arrays
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			h_graph_node node_handle = node_handle_from_index(index);
			const s_node &node = get_node(node_handle);

			if (node.type == e_native_module_graph_node_type::k_array
				&& node.incoming_edge_handles.size() >= k_large_array_limit) {
				e_native_module_data_mutability data_mutability = get_node_data_type(node_handle).get_data_mutability();
				if (data_mutability == e_native_module_data_mutability::k_constant) {
					arrays_to_collapse[node_handle.get_data().index] = true;

					// Mark each input as an array to collapse so that checking the constants below is easier. Also
					// mark it as a node to skip, so we skip it during output.
					for (size_t input = 0; input < node.incoming_edge_handles.size(); input++) {
						h_graph_node input_node_handle = node.incoming_edge_handles[input];
						arrays_to_collapse[input_node_handle.get_data().index] = true;
						nodes_to_skip[input_node_handle.get_data().index] = true;
					}
				}
			}
		}

		// Find and mark all constants which are inputs to only the nodes we just marked
		for (uint32 index = 0; index < m_nodes.size(); index++) {
			h_graph_node node_handle = node_handle_from_index(index);
			const s_node &node = get_node(node_handle);

			if (node.type == e_native_module_graph_node_type::k_constant) {
				bool all_outputs_marked = true;
				for (h_graph_node edge_node_handle : node.outgoing_edge_handles) {
					all_outputs_marked = arrays_to_collapse[edge_node_handle.get_data().index];
					if (!all_outputs_marked) {
						break;
					}
				}

				if (all_outputs_marked) {
					nodes_to_skip[node_handle.get_data().index] = true;
				}
			}
		}
	}

	c_graphviz_generator graph;
	graph.set_graph_name("native_module_graph");

	size_t collapsed_nodes = 0;

	// Declare the nodes
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		h_graph_node node_handle = node_handle_from_index(index);
		const s_node &node = get_node(node_handle);
		c_graphviz_node graph_node;

		if (collapse_large_arrays && nodes_to_skip[node_handle.get_data().index]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		switch (node.type) {
		case e_native_module_graph_node_type::k_constant:
		{
			graph_node.set_shape("ellipse");
			std::string label;
			switch (node.constant_node_data().primitive_type) {
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

		case e_native_module_graph_node_type::k_array:
		{
			graph_node.set_shape("ellipse");
			std::string label = get_node_data_type(node_handle).to_string();
			graph_node.set_label(label.c_str());
			break;
		}

		case e_native_module_graph_node_type::k_native_module_call:
			graph_node.set_shape("box");
			graph_node.set_label(c_native_module_registry::get_native_module(
				node.native_module_call_node_data().native_module_handle).name.get_string());
			break;

		case e_native_module_graph_node_type::k_indexed_input:
		{
			wl_assert(node.outgoing_edge_handles.size() == 1);
			const s_node &root_node = get_node(node.outgoing_edge_handles[0]);
			uint32 input_index;
			for (input_index = 0; input_index < root_node.incoming_edge_handles.size(); input_index++) {
				if (root_node.incoming_edge_handles[input_index] == node_handle) {
					break;
				}
			}

			wl_assert(valid_index(input_index, root_node.incoming_edge_handles.size()));
			graph_node.set_shape("house");
			graph_node.set_orientation(180.0f);
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(input_index)).c_str());
			break;
		}

		case e_native_module_graph_node_type::k_indexed_output:
		{
			wl_assert(node.incoming_edge_handles.size() == 1);
			const s_node &root_node = get_node(node.incoming_edge_handles[0]);
			uint32 output_index;
			for (output_index = 0; output_index < root_node.outgoing_edge_handles.size(); output_index++) {
				if (root_node.outgoing_edge_handles[output_index] == node_handle) {
					break;
				}
			}

			wl_assert(valid_index(output_index, root_node.outgoing_edge_handles.size()));
			graph_node.set_shape("house");
			graph_node.set_margin(0.0f, 0.0f);
			graph_node.set_label((std::to_string(output_index)).c_str());
			break;
		}

		case e_native_module_graph_node_type::k_input:
			graph_node.set_shape("box");
			graph_node.set_label(("input " + std::to_string(node.input_node_data().input_index)).c_str());
			graph_node.set_style("rounded");
			break;

		case e_native_module_graph_node_type::k_output:
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

		case e_native_module_graph_node_type::k_temporary_reference:
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
			collapsed_array_node.set_label(("[" + std::to_string(node.incoming_edge_handles.size()) + "]").c_str());
			graph.add_node(collapsed_array_node);

			collapsed_nodes++;

			// Add the edge
			graph.add_edge(collapsed_array_node_name.c_str(), node_name.c_str());
		}
	}

	// Declare the edges
	for (uint32 index = 0; index < m_nodes.size(); index++) {
		h_graph_node node_handle = node_handle_from_index(index);

		if (collapse_large_arrays && nodes_to_skip[node_handle.get_data().index]) {
			// This is a node which has been collapsed - skip it
			continue;
		}

		const s_node &node = get_node(node_handle);
		for (h_graph_node edge_node_handle : node.outgoing_edge_handles) {
			if (collapse_large_arrays && nodes_to_skip[edge_node_handle.get_data().index]) {
				// This is a node which has been collapsed - skip it
				continue;
			}

			graph.add_edge(
				("node_" + std::to_string(node_handle.get_data().index)).c_str(),
				("node_" + std::to_string(edge_node_handle.get_data().index)).c_str());
		}
	}

	return graph.output_to_file(fname);
}
