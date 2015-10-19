#ifndef WAVELANG_TASK_GRAPH_H__
#define WAVELANG_TASK_GRAPH_H__

#include "common/common.h"
#include "engine/task_function.h"
#include "common/utility/string_table.h"
#include <vector>

class c_execution_graph;

struct s_task_graph_globals {
	uint32 max_voices;
};

struct s_task_graph_data {
	e_task_data_type type;

	// Do not access these directly
	union {
		struct {
			// Used when building the graph
			uint32 execution_graph_index_a;
			uint32 execution_graph_index_b;
		};

		uint32 real_buffer_in;
		uint32 real_buffer_out;
		uint32 real_buffer_inout;
		real32 real_constant_in;
		bool bool_constant_in;
		const char *string_constant_in;
	} data;

	uint32 get_real_buffer_in() const {
		wl_assert(type == k_task_data_type_real_buffer_in);
		return data.real_buffer_in;
	}

	uint32 get_real_buffer_out() const {
		wl_assert(type == k_task_data_type_real_buffer_out);
		return data.real_buffer_out;
	}

	uint32 get_real_buffer_inout() const {
		wl_assert(type == k_task_data_type_real_buffer_inout);
		return data.real_buffer_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(type == k_task_data_type_real_constant_in);
		return data.real_constant_in;
	}

	bool get_bool_constant_in() const {
		wl_assert(type == k_task_data_type_bool_constant_in);
		return data.bool_constant_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(type == k_task_data_type_string_constant_in);
		return data.string_constant_in;
	}
};

typedef c_wrapped_array_const<s_task_graph_data> c_task_graph_data_array;
typedef c_wrapped_array_const<uint32> c_task_graph_task_array;

class c_task_graph {
public:
	static const uint32 k_invalid_buffer = static_cast<uint32>(-1);

	c_task_graph();
	~c_task_graph();

	bool build(const c_execution_graph &execution_graph);

	uint32 get_task_count() const;
	uint32 get_max_task_concurrency() const;

	uint32 get_task_function_index(uint32 task_index) const;
	c_task_graph_data_array get_task_arguments(uint32 task_index) const;

	size_t get_task_predecessor_count(uint32 task_index) const;
	c_task_graph_task_array get_task_successors(uint32 task_index) const;

	c_task_graph_task_array get_initial_tasks() const;
	c_task_graph_data_array get_outputs() const;

	uint32 get_buffer_count() const;
	uint32 get_max_buffer_concurrency() const;
	uint32 get_buffer_usages(uint32 buffer_index) const;

	const s_task_graph_globals &get_globals() const;

private:
	struct s_task {
		// The function to execute during this task
		uint32 task_function_index;

		// Start index in m_data_lists
		size_t arguments_start;

		// Number of tasks which must complete before this task is executed
		size_t predecessor_count;

		// List of tasks which can execute only after this task has been completed
		size_t successors_start;
		size_t successors_count;
	};

	void resolve_strings();
	bool add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index,
		std::vector<uint32> &nodes_to_tasks);
	void setup_task(const c_execution_graph &execution_graph, uint32 node_index,
		uint32 task_index, const s_task_function_mapping &task_function_mapping);
	void build_task_successor_lists(const c_execution_graph &execution_graph,
		const std::vector<uint32> &nodes_to_tasks);
	void allocate_buffers(const c_execution_graph &execution_graph);
	void assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
		const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index);
	void calculate_max_concurrency();
	uint32 estimate_max_concurrency(uint32 node_count, const std::vector<bool> &concurrency_matrix) const;
	void calculate_buffer_usages();

	std::vector<s_task> m_tasks;

	std::vector<s_task_graph_data> m_data_lists;
	std::vector<uint32> m_task_lists;
	c_string_table m_string_table;

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
	size_t m_outputs_start;
	size_t m_outputs_count;

	s_task_graph_globals m_globals;
};

#endif // WAVELANG_TASK_GRAPH_H__