#ifndef WAVELANG_TASK_GRAPH_H__
#define WAVELANG_TASK_GRAPH_H__

#include "common/common.h"
#include "engine/task_functions.h"
#include "engine/native_module_task_mapping.h"
#include <vector>

class c_execution_graph;

struct s_task_graph_globals {
	uint32 max_voices;
};

class c_task_graph {
public:
	static const uint32 k_invalid_buffer = static_cast<uint32>(-1);

	typedef c_wrapped_array_const<real32> c_constant_array;
	typedef c_wrapped_array_const<uint32> c_buffer_array;
	typedef c_wrapped_array_const<uint32> c_task_array;

	c_task_graph();
	~c_task_graph();

	bool build(const c_execution_graph &execution_graph);

	uint32 get_task_count() const;
	uint32 get_max_task_concurrency() const;

	e_task_function get_task_function(uint32 task_index) const;

	c_constant_array get_task_constants(uint32 task_index) const;
	c_buffer_array get_task_in_buffers(uint32 task_index) const;
	c_buffer_array get_task_out_buffers(uint32 task_index) const;
	c_buffer_array get_task_inout_buffers(uint32 task_index) const;

	size_t get_task_predecessor_count(uint32 task_index) const;
	c_task_array get_task_successors(uint32 task_index) const;

	c_task_array get_initial_tasks() const;

	uint32 get_buffer_count() const;
	uint32 get_max_buffer_concurrency() const;
	uint32 get_buffer_usages(uint32 buffer_index) const;

	// Since this can switch between constant and buffer, don't return an array, require querying
	uint32 get_output_count() const;
	bool is_output_buffer(uint32 output_index) const;
	uint32 get_output_buffer(uint32 output_index) const;
	real32 get_output_constant(uint32 output_index) const;

	const s_task_graph_globals &get_globals() const;

private:
	struct s_task {
		// The function to execute during this task
		e_task_function task_function;

		// Start index in m_constant_lists or m_buffer_lists for each list of constants or buffers for the task function
		size_t in_constants_start;
		size_t in_buffers_start;
		size_t out_buffers_start;
		size_t inout_buffers_start;

		// Number of tasks which must complete before this task is executed
		size_t predecessor_count;

		// List of tasks which can execute only after this task has been completed
		size_t successors_start;
		size_t successors_count;
	};

	bool add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index,
		std::vector<uint32> &nodes_to_tasks);
	void setup_task(const c_execution_graph &execution_graph, uint32 node_index,
		uint32 task_index, e_task_function task_function, const c_task_mapping_array &task_mapping_array);
	void build_task_successor_lists(const c_execution_graph &execution_graph,
		const std::vector<uint32> &nodes_to_tasks);
	void allocate_buffers(const c_execution_graph &execution_graph);
	void assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
		const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index);
	void calculate_max_concurrency();
	uint32 estimate_max_concurrency(uint32 node_count, const std::vector<bool> &concurrency_matrix) const;
	void calculate_buffer_usages();

	std::vector<s_task> m_tasks;

	std::vector<real32> m_constant_lists;
	std::vector<uint32> m_buffer_lists;
	std::vector<uint32> m_task_lists;

	// Total number of unique buffers required
	uint32 m_buffer_count;

	// Number of times each buffer is used
	std::vector<uint32> m_buffer_usages;

	// Max amount of concurrency that can exist at any given time during execution
	uint32 m_max_task_concurrency;
	uint32 m_max_buffer_concurrency;

	// List of initial tasks
	size_t m_initial_tasks_start;
	size_t m_initial_tasks_count;

	// List of final output buffers
	size_t m_output_buffers_start;
	size_t m_output_buffers_count;

	// It is possible that an output is linked up to a constant instead of a buffer. In this case, a constant will be
	// stored at the appropriate index in this list, and in the output buffer list, k_invalid_buffer will be stored.
	size_t m_output_constants_start;
	size_t m_output_constants_count;

	s_task_graph_globals m_globals;
};

#endif // WAVELANG_TASK_GRAPH_H__