#pragma once

#include "common/common.h"
#include "common/utility/string_table.h"

#include "instrument/graph_node_handle.h"
#include "instrument/instrument_constants.h"
#include "instrument/native_module_registry.h"

#include "native_module/native_module.h"

#include <fstream>
#include <variant>
#include <vector>

enum class e_native_module_graph_node_type {
	// An invalid node, or a node which has been removed
	k_invalid,

	// Represents a constant; can only be used as inputs
	k_constant,

	// Represents an array; can only be used as inputs
	k_array,

	// Has 1 input per in argument pointing to a e_native_module_graph_node_type::k_indexed_input node
	// Has 1 output per out argument pointing to a e_native_module_graph_node_type::k_indexed_output node
	k_native_module_call,

	// An input with an index, used for native module arguments and array indices
	k_indexed_input,

	// An output with an index, used for native module arguments
	k_indexed_output,

	// Has exactly 1 output and no inputs
	k_input,

	// Has exactly 1 input and no outputs
	k_output,

	// Used to temporarily reference other nodes to ensure that they don't get removed when building the graph
	k_temporary_reference,

	k_count
};

class c_native_module_graph {
public:
	using f_on_node_removed = void (*)(void *context, h_graph_node node_handle);

	// Special output index representing whether processing should remain active - once a voice drops to 0 volume it can
	// be disabled for improved performance
	static constexpr uint32 k_remain_active_output_index = static_cast<uint32>(-1);

	c_native_module_graph();

	e_instrument_result save(std::ofstream &out) const;
	e_instrument_result load(std::ifstream &in);

	bool validate() const;

	h_graph_node add_constant_node(real32 constant_value);
	h_graph_node add_constant_node(bool constant_value);
	h_graph_node add_constant_node(const char *constant_value);
	h_graph_node add_array_node(c_native_module_data_type element_data_type);
	void add_array_value(h_graph_node array_node_handle, h_graph_node value_node_handle);
	// Returns the old value
	h_graph_node set_array_value_at_index(h_graph_node array_node_handle, uint32 index, h_graph_node value_node_handle);
	h_graph_node add_native_module_call_node(h_native_module native_module_handle);
	h_graph_node add_input_node(uint32 input_index);
	h_graph_node add_output_node(uint32 output_index);
	h_graph_node add_temporary_reference_node();
	void remove_node(
		h_graph_node node_handle,
		f_on_node_removed on_node_removed = nullptr,
		void *on_node_removed_context = nullptr);

	void add_edge(h_graph_node from_handle, h_graph_node to_handle);
	void remove_edge(h_graph_node from_handle, h_graph_node to_handle);

	uint32 get_node_count() const;
	h_graph_node nodes_begin() const; // $TODO $COMPILER make an iterator class instead
	h_graph_node nodes_next(h_graph_node node_handle) const;

	e_native_module_graph_node_type get_node_type(h_graph_node node_handle) const;
	c_native_module_data_type get_node_data_type(h_graph_node node_handle) const;
	bool is_node_constant(h_graph_node node_handle) const; // Returns true for arrays with only constant inputs
	real32 get_constant_node_real_value(h_graph_node node_handle) const;
	bool get_constant_node_bool_value(h_graph_node node_handle) const;
	const char *get_constant_node_string_value(h_graph_node node_handle) const;
	h_native_module get_native_module_call_native_module_handle(h_graph_node node_handle) const;
	uint32 get_input_node_input_index(h_graph_node node_handle) const;
	uint32 get_output_node_output_index(h_graph_node node_handle) const;
	size_t get_node_incoming_edge_count(h_graph_node node_handle) const;
	h_graph_node get_node_incoming_edge_handle(h_graph_node node_handle, size_t edge) const;
	size_t get_node_outgoing_edge_count(h_graph_node node_handle) const;
	h_graph_node get_node_outgoing_edge_handle(h_graph_node node_handle, size_t edge) const;

	bool does_node_use_indexed_inputs(h_graph_node node_handle) const;
	bool does_node_use_indexed_outputs(h_graph_node node_handle) const;
	size_t get_node_indexed_input_incoming_edge_count(h_graph_node node_handle, size_t input_index) const;
	h_graph_node get_node_indexed_input_incoming_edge_handle(
		h_graph_node node_handle,
		size_t input_index,
		size_t edge) const;
	size_t get_node_indexed_output_outgoing_edge_count(h_graph_node node_handle, size_t output_index) const;
	h_graph_node get_node_indexed_output_outgoing_edge_handle(
		h_graph_node node_handle,
		size_t output_index,
		size_t edge) const;

	void remove_unused_nodes_and_reassign_node_indices();

	// If collapse_large_arrays is true, constant arrays with at least k_large_array_limit elements will be collapsed
	// into a single node labeled "[n]", where n is the number of elements.
	bool generate_graphviz_file(const char *fname, bool collapse_large_arrays) const;

private:
	struct s_node {
		struct s_no_node_data {};

		struct s_constant_node_data {
			c_native_module_data_type type;

			union {
				real32 real_value;
				bool bool_value;
				uint32 string_index;
			};
		};

		struct s_array_node_data {
			c_native_module_data_type type;
		};

		struct s_native_module_call_node_data {
			h_native_module native_module_handle;
		};

		struct s_input_node_data {
			uint32 input_index;
		};

		struct s_output_node_data {
			uint32 output_index;
		};

		e_native_module_graph_node_type type;
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		uint32 salt;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		std::variant<
			s_no_node_data,
			s_constant_node_data,
			s_array_node_data,
			s_native_module_call_node_data,
			s_input_node_data,
			s_output_node_data> node_data;
		std::vector<h_graph_node> incoming_edge_handles;
		std::vector<h_graph_node> outgoing_edge_handles;

		s_constant_node_data &constant_node_data() {
			return std::get<s_constant_node_data>(node_data);
		}

		const s_constant_node_data &constant_node_data() const {
			return std::get<s_constant_node_data>(node_data);
		}

		s_array_node_data &array_node_data() {
			return std::get<s_array_node_data>(node_data);
		}

		const s_array_node_data &array_node_data() const {
			return std::get<s_array_node_data>(node_data);
		}

		s_native_module_call_node_data &native_module_call_node_data() {
			return std::get<s_native_module_call_node_data>(node_data);
		}

		const s_native_module_call_node_data &native_module_call_node_data() const {
			return std::get<s_native_module_call_node_data>(node_data);
		}

		s_input_node_data &input_node_data() {
			return std::get<s_input_node_data>(node_data);
		}

		const s_input_node_data &input_node_data() const {
			return std::get<s_input_node_data>(node_data);
		}

		s_output_node_data &output_node_data() {
			return std::get<s_output_node_data>(node_data);
		}

		const s_output_node_data &output_node_data() const {
			return std::get<s_output_node_data>(node_data);
		}
	};

	static bool does_node_use_indexed_inputs(const s_node &node);
	static bool does_node_use_indexed_outputs(const s_node &node);

	h_graph_node allocate_node();
	h_graph_node node_handle_from_index(uint32 node_index) const;
	s_node &get_node(h_graph_node node_handle);
	const s_node &get_node(h_graph_node node_handle) const;

	void add_edge_internal(h_graph_node from_handle, h_graph_node to_handle);
	bool add_edge_for_load(
		h_graph_node from_handle,
		h_graph_node to_handle,
		size_t from_edge,
		size_t to_edge);
	void remove_edge_internal(h_graph_node from_handle, h_graph_node to_handle);

	bool validate_node(h_graph_node node_handle) const;
	bool validate_edge(h_graph_node from_handle, h_graph_node to_handle) const;
	bool validate_native_modules() const;
	bool validate_string_table() const;

	bool visit_node_for_cycle_detection(
		h_graph_node node_handle,
		std::vector<bool> &nodes_visited,
		std::vector<bool> &nodes_marked) const;

	void remove_unused_strings();

	std::vector<s_node> m_nodes;
	std::vector<uint32> m_free_node_indices;

	c_string_table m_string_table;
};

