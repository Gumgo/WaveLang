#ifndef WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_H__
#define WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_H__

#include "common/common.h"
#include "common/utility/string_table.h"

#include "execution_graph/instrument_constants.h"
#include "execution_graph/native_module.h"

#include <fstream>
#include <vector>

enum e_execution_graph_node_type {
	// An invalid node, or a node which has been removed
	k_execution_graph_node_type_invalid,

	// Represents a constant; can only be used as inputs
	k_execution_graph_node_type_constant,

	// Has 1 input per in argument pointing to a k_execution_graph_node_type_indexed_input node
	// Has 1 output per out argument pointing to a k_execution_graph_node_type_indexed_output node
	k_execution_graph_node_type_native_module_call,

	// An input to a native module
	k_execution_graph_node_type_indexed_input,

	// An output to a native module
	k_execution_graph_node_type_indexed_output,

	// Takes exactly 1 input
	k_execution_graph_node_type_output,

	k_execution_graph_node_type_count
};

struct s_execution_graph_globals {
	// The maximum number of voice instances that can be playing at once
	uint32 max_voices;

	// The stream sample rate. If 0, this graph is compatible with any stream. Note that while some modules require the
	// sample rate to be pre-specified (such as get_sample_rate()), most are sample rate agnostic.
	uint32 sample_rate; // nocheckin

	// The size of each processing chunk. If 0, this graph is compatible with any processing chunk size. Certain modules
	// may require a minimum chunk size, though most are chunk size agnostic.
	uint32 chunk_size;
};

class c_execution_graph {
public:
	static const uint32 k_invalid_index = static_cast<uint32>(-1);

	// Special output index representing whether processing should remain active - once a voice drops to 0 volume it can
	// be disabled for improved performance
	static const uint32 k_remain_active_output_index = static_cast<uint32>(-1);

	c_execution_graph();

	e_instrument_result save(std::ofstream &out) const;
	e_instrument_result load(std::ifstream &in);

	bool validate() const;

	uint32 add_constant_node(real32 constant_value);
	uint32 add_constant_node(bool constant_value);
	uint32 add_constant_node(const std::string &constant_value);
	uint32 add_constant_array_node(c_native_module_data_type element_data_type);
	void add_constant_array_value(uint32 constant_array_node_index, uint32 value_node_index);
	uint32 add_native_module_call_node(uint32 native_module_index);
	uint32 add_output_node(uint32 output_index);
	void remove_node(uint32 node_index);

	void add_edge(uint32 from_index, uint32 to_index);
	void remove_edge(uint32 from_index, uint32 to_index);

	uint32 get_node_count() const;
	e_execution_graph_node_type get_node_type(uint32 node_index) const;
	c_native_module_data_type get_constant_node_data_type(uint32 node_index) const;
	real32 get_constant_node_real_value(uint32 node_index) const;
	bool get_constant_node_bool_value(uint32 node_index) const;
	const char *get_constant_node_string_value(uint32 node_index) const;
	uint32 get_native_module_call_native_module_index(uint32 node_index) const;
	uint32 get_output_node_output_index(uint32 node_index) const;
	size_t get_node_incoming_edge_count(uint32 node_index) const;
	uint32 get_node_incoming_edge_index(uint32 node_index, size_t edge) const;
	size_t get_node_outgoing_edge_count(uint32 node_index) const;
	uint32 get_node_outgoing_edge_index(uint32 node_index, size_t edge) const;

	bool does_node_use_indexed_inputs(uint32 node_index) const;
	bool does_node_use_indexed_outputs(uint32 node_index) const;

	void set_globals(const s_execution_graph_globals &globals);
	const s_execution_graph_globals get_globals() const;

	void remove_unused_nodes_and_reassign_node_indices();

	// If collapse_large_arrays is true, constant arrays with at least k_large_array_limit elements will be collapsed
	// into a single node labeled "[n]", where n is the number of elements.
	bool generate_graphviz_file(const char *fname, bool collapse_large_arrays) const;

private:
	struct s_node {
		e_execution_graph_node_type type;
		union u_node_data {
			u_node_data() {}

			struct {
				uint32 native_module_index;
			} native_module_call;

			struct {
				c_native_module_data_type type;

				union {
					real32 real_value;
					bool bool_value;
					uint32 string_index;
				};
			} constant;

			struct {
				uint32 output_index;
			} output;
		};

		u_node_data node_data;
		std::vector<uint32> incoming_edge_indices;
		std::vector<uint32> outgoing_edge_indices;
	};

	static bool does_node_use_indexed_inputs(const s_node &node);
	static bool does_node_use_indexed_outputs(const s_node &node);
	uint32 allocate_node();
	void add_edge_internal(uint32 from_index, uint32 to_index);
	bool add_edge_for_load(uint32 from_index, uint32 to_index);
	void remove_edge_internal(uint32 from_index, uint32 to_index);

	bool validate_node(uint32 index) const;
	bool validate_edge(uint32 from_index, uint32 to_index) const;
	bool validate_constants() const;
	bool validate_string_table() const;
	bool get_type_from_node(uint32 node_index, c_native_module_data_type &out_type) const;

	bool visit_node_for_cycle_detection(uint32 node_index,
		std::vector<bool> &nodes_visited, std::vector<bool> &nodes_marked) const;

	void remove_unused_strings();

	std::vector<s_node> m_nodes;
	s_execution_graph_globals m_globals;

	c_string_table m_string_table;
};

#endif // WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_H__
