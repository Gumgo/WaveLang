#ifndef WAVELANG_TASK_GRAPH_H__
#define WAVELANG_TASK_GRAPH_H__

#include "common/common.h"
#include "engine/task_functions.h"

#include <vector>

class c_execution_graph;

class c_task_graph {
public:
	c_task_graph();
	~c_task_graph();

	bool build(const c_execution_graph &execution_graph);

private:
	struct s_task_node {
		// The function to execute during this task
		e_task_function task_function;

		// Start index in m_constant_lists or m_buffer_lists for each list of constants or buffers for the task function
		size_t in_constants_start;
		size_t in_buffers_start;
		size_t out_buffers_start;
		size_t inout_buffers_start;

		// List of tasks which are dependent on this task
		size_t dependencies_start;
		size_t dependencies_count;
	};

	enum e_task_mapping_location {
		k_task_mapping_location_constant_in,
		k_task_mapping_location_buffer_in,
		k_task_mapping_location_buffer_out,
		k_task_mapping_location_buffer_inout,

		k_task_mapping_location_count
	};

	struct s_task_mapping {
		// Where to map to on the task
		e_task_mapping_location location;

		// Index for the location
		size_t index;

		s_task_mapping(e_task_mapping_location loc, size_t idx)
			: location(loc)
			, index(idx) {
		}
	};

	// Maps execution graph native module inputs/outputs to task inputs/outputs/inouts
	// The array should first list all inputs for the native module, then list all outputs
	typedef c_wrapped_array_const<s_task_mapping> c_task_mapping_array;

	bool add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index);
	void setup_task(const c_execution_graph &execution_graph, uint32 node_index,
		uint32 task_index, e_task_function task_function, const c_task_mapping_array &task_mapping_array);
	void allocate_buffers(const c_execution_graph &execution_graph);
	void assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
		const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index);
	void calculate_max_buffer_concurrency();

	std::vector<s_task_node> m_task_nodes;

	std::vector<real32> m_constant_lists;
	std::vector<uint32> m_buffer_lists;
	std::vector<uint32> m_node_lists;

	uint32 m_buffer_count;
	uint32 m_max_buffer_concurrency;

	// List of final output buffers
	size_t m_output_buffers_start;
	size_t m_output_buffers_count;
};

#endif // WAVELANG_TASK_GRAPH_H__