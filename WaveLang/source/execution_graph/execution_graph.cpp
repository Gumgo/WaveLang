#include "execution_graph/execution_graph.h"
#include "execution_graph/native_modules.h"
#include <string>

#if PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)
#include <fstream>
#endif // PREDEFINED(EXECUTION_GRAPH_OUTPUT_ENABLED)

static const char k_format_identifier[] = { 'w', 'a', 'v', 'e', 'l', 'a', 'n', 'g' };

c_execution_graph::c_execution_graph() {
	ZERO_STRUCT(&m_globals);
}

// $TODO do we care about being endian-correct?

template<typename t_value> void write(std::ofstream &out, t_value value) {
	out.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

template<typename t_value> bool read(std::ifstream &in, t_value &value) {
	in.read(reinterpret_cast<char *>(&value), sizeof(value));
	return !in.fail();
}

e_execution_graph_result c_execution_graph::save(const char *fname) const {
	wl_assert(validate());

#if PREDEFINED(ASSERTS_ENABLED)
	// Make sure there are no invalid or intermediate value nodes
	for (size_t index = 0; index < m_nodes.size(); index++) {
		wl_assert(m_nodes[index].type != k_execution_graph_node_type_invalid &&
			m_nodes[index].type != k_execution_graph_node_type_intermediate_value);
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
			write(out, node.constant_value);
			break;

		case k_execution_graph_node_type_native_module_call:
			wl_assert(node.native_module_index != k_native_module_noop);
			write(out, node.native_module_index);
			break;

		case k_execution_graph_node_type_native_module_input:
		case k_execution_graph_node_type_native_module_output:
			break;

		case k_execution_graph_node_type_output:
			write(out, node.output_index);
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

	if (out.fail()) {
		return k_execution_graph_result_failed_to_write;
	}

	return k_execution_graph_result_success;
}

e_execution_graph_result c_execution_graph::load(const char *fname) {
	wl_assert(m_nodes.empty());

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
			if (!read(in, node.constant_value)) {
				return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
			}
			break;

		case k_execution_graph_node_type_native_module_call:
			if (!read(in, node.native_module_index)) {
				return in.eof() ? k_execution_graph_result_invalid_graph : k_execution_graph_result_failed_to_read;
			}

			// Don't allow no-ops
			if (node.native_module_index == k_native_module_noop) {
				return k_execution_graph_result_invalid_graph;
			}
			break;

		case k_execution_graph_node_type_native_module_input:
		case k_execution_graph_node_type_native_module_output:
			break;

		case k_execution_graph_node_type_output:
			if (!read(in, node.output_index)) {
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

	// Validate globals
	if (m_globals.max_voices < 1) {
		return false;
	}

	return true;
}

uint32 c_execution_graph::allocate_node() {
	uint32 index = static_cast<uint32>(m_nodes.size());
	m_nodes.push_back(s_node());
	return index;
}

uint32 c_execution_graph::add_constant_node(real32 constant_value) {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_constant;
	node.constant_value = constant_value;
	return index;
}

uint32 c_execution_graph::add_native_module_call_node(uint32 native_module_index) {
	wl_assert(VALID_INDEX(native_module_index, c_native_module_registry::get_native_module_count()));

	uint32 index = allocate_node();
	// Don't store a reference to the node, it gets invalidated if the vector is resized when adding inputs/outputs
	m_nodes[index].type = k_execution_graph_node_type_native_module_call;
	m_nodes[index].native_module_index = native_module_index;

	// Add nodes for each argument; the return value is output 0
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		uint32 argument_node_index = allocate_node();
		s_node &argument_node = m_nodes[argument_node_index];
		argument_node.data = 0;
		if (native_module.argument_types[arg] == k_native_module_argument_type_in) {
			argument_node.type = k_execution_graph_node_type_native_module_input;
			argument_node.outgoing_edge_indices.push_back(index);
			m_nodes[index].incoming_edge_indices.push_back(argument_node_index);
		} else {
			wl_assert(native_module.argument_types[arg] == k_native_module_argument_type_out);
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
	node.output_index = output_index;
	return index;
}

uint32 c_execution_graph::add_intermediate_value_node() {
	uint32 index = allocate_node();
	s_node &node = m_nodes[index];
	node.type = k_execution_graph_node_type_intermediate_value;
	node.data = 0;
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
	node.data = 0;
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
			if (!VALID_INDEX(node.native_module_index, c_native_module_registry::get_native_module_count())) {
				return false;
			}

			const s_native_module &native_module =
				c_native_module_registry::get_native_module(node.native_module_index);

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

	case k_execution_graph_node_type_intermediate_value:
		return
			(node.incoming_edge_indices.size() == 1);

	default:
		// Unknown type
		return false;
	}

	return true;
}

bool c_execution_graph::validate_edge(uint32 from_index, uint32 to_index) const {
	if (!VALID_INDEX(from_index, m_nodes.size()) ||
		!VALID_INDEX(to_index, m_nodes.size())) {
		return false;
	}

	const s_node &from_node = m_nodes[from_index];
	const s_node &to_node = m_nodes[to_index];

	switch (from_node.type) {
	case k_execution_graph_node_type_constant:
		return
			(to_node.type == k_execution_graph_node_type_native_module_input) ||
			(to_node.type == k_execution_graph_node_type_output) ||
			(to_node.type == k_execution_graph_node_type_intermediate_value);

	case k_execution_graph_node_type_native_module_call:
		return
			(to_node.type == k_execution_graph_node_type_native_module_output);

	case k_execution_graph_node_type_native_module_input:
		return
			(to_node.type == k_execution_graph_node_type_native_module_call);

	case k_execution_graph_node_type_native_module_output:
		return
			(to_node.type == k_execution_graph_node_type_native_module_input) ||
			(to_node.type == k_execution_graph_node_type_output) ||
			(to_node.type == k_execution_graph_node_type_intermediate_value);

	case k_execution_graph_node_type_output:
		return false;

	case k_execution_graph_node_type_intermediate_value:
		return
			(to_node.type == k_execution_graph_node_type_native_module_input) ||
			(to_node.type == k_execution_graph_node_type_output) ||
			(to_node.type == k_execution_graph_node_type_intermediate_value);

	default:
		// Unknown type
		return false;
	}

	return true;
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

uint32 c_execution_graph::get_node_count() const {
	return static_cast<uint32>(m_nodes.size());
}

e_execution_graph_node_type c_execution_graph::get_node_type(uint32 node_index) const {
	return m_nodes[node_index].type;
}

real32 c_execution_graph::get_constant_node_value(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_constant);
	return node.constant_value;
}

uint32 c_execution_graph::get_native_module_call_native_module_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_native_module_call);
	return node.native_module_index;
}

uint32 c_execution_graph::get_output_node_output_index(uint32 node_index) const {
	const s_node &node = m_nodes[node_index];
	wl_assert(node.type == k_execution_graph_node_type_output);
	return node.output_index;
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
			label = std::to_string(node.constant_value);
			break;

		case k_execution_graph_node_type_native_module_call:
			shape = "box";
			label = c_native_module_registry::get_native_module(node.native_module_index).name;
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

		case k_execution_graph_node_type_intermediate_value:
			shape = "circle";
			label = "val";
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