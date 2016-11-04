#ifndef WAVELANG_ENGINE_TASK_GRAPH_H__
#define WAVELANG_ENGINE_TASK_GRAPH_H__

#include "common/common.h"
#include "common/utility/string_table.h"

#include "engine/task_function.h"

#include <vector>

class c_execution_graph;
class c_predecessor_resolver;

struct s_task_graph_globals {
	uint32 max_voices;
};

struct s_task_graph_data {
	// Do not access these directly
	struct {
		c_task_qualified_data_type type;
		bool is_constant;

		union u_value {
			u_value() {} // Allows for c_wrapped_array

			struct {
				// Used when building the graph
				uint32 execution_graph_index_a;
				uint32 execution_graph_index_b;
			};

			uint32 buffer; // Shared accessor for any buffer, no accessor function for this

			uint32 real_buffer_in;
			uint32 real_buffer_out;
			uint32 real_buffer_inout;
			real32 real_constant_in;
			c_real_array real_array_in;

			uint32 bool_buffer_in;
			uint32 bool_buffer_out;
			uint32 bool_buffer_inout;
			bool bool_constant_in;
			c_bool_array bool_array_in;

			const char *string_constant_in;
			c_string_array string_array_in;
		} value;
	} data;

	c_task_qualified_data_type get_type() const {
		return data.type;
	}

	bool is_constant() const {
		return data.is_constant;
	}

	uint32 get_real_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(!is_constant());
		return data.value.real_buffer_in;
	}

	uint32 get_real_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_out);
		wl_assert(!is_constant());
		return data.value.real_buffer_out;
	}

	uint32 get_real_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_inout);
		wl_assert(!is_constant());
		return data.value.real_buffer_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.real_constant_in;
	}

	c_real_array get_real_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.real_array_in;
	}

	uint32 get_bool_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(!is_constant());
		return data.value.bool_buffer_in;
	}

	uint32 get_bool_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_out);
		wl_assert(!is_constant());
		return data.value.bool_buffer_out;
	}

	uint32 get_bool_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_inout);
		wl_assert(!is_constant());
		return data.value.bool_buffer_inout;
	}

	bool get_bool_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.bool_constant_in;
	}

	c_bool_array get_bool_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.bool_array_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_string));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.string_constant_in;
	}

	c_string_array get_string_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_string, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.string_array_in;
	}
};

typedef c_wrapped_array_const<s_task_graph_data> c_task_graph_data_array;
typedef c_wrapped_array_const<uint32> c_task_graph_task_array;

// Iterates through all buffers associated with a task
class c_task_buffer_iterator {
public:
	c_task_buffer_iterator(
		c_task_graph_data_array data_array, c_task_data_type type_mask = c_task_data_type::invalid());
	bool is_valid() const;
	void next();
	uint32 get_buffer_index() const;
	uint32 get_node_index() const; // For internal use only - used when constructing the graph
	c_task_qualified_data_type get_buffer_type() const;
	const s_task_graph_data &get_task_graph_data() const;

private:
	c_task_graph_data_array m_data_array;
	c_task_data_type m_type_mask; // Only buffers of this type are returned
	size_t m_argument_index;
	size_t m_array_index;
};

class c_task_graph {
public:
	// Describes the usage of a type of buffer within the graph
	struct s_buffer_usage_info {
		c_task_data_type type;
		uint32 max_concurrency;
	};

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
	const s_task_graph_data &get_remain_active_output() const;

	uint32 get_buffer_count() const;
	c_wrapped_array_const<s_buffer_usage_info> get_buffer_usage_info() const;
	uint32 get_buffer_usages(uint32 buffer_index) const;

	const s_task_graph_globals &get_globals() const;

private:
	static const size_t k_invalid_list_index = static_cast<size_t>(-1);

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

	void resolve_arrays();
	void resolve_strings();
	bool add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index,
		std::vector<uint32> &nodes_to_tasks);
	void setup_task(const c_execution_graph &execution_graph, uint32 node_index,
		uint32 task_index, const s_task_function_mapping &task_function_mapping);
	uint32 *get_task_data_array_element_buffer(const s_task_graph_data &argument, size_t index);
	void build_task_successor_lists(const c_execution_graph &execution_graph,
		const std::vector<uint32> &nodes_to_tasks);
	void allocate_buffers(const c_execution_graph &execution_graph);
	void assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
		const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index);
	void calculate_max_concurrency();
	uint32 calculate_max_buffer_concurrency(
		const c_predecessor_resolver &task_predecessor_resolver, c_task_data_type type) const;
	uint32 estimate_max_concurrency(uint32 node_count, const std::vector<bool> &concurrency_matrix) const;
	void calculate_buffer_usages();

	std::vector<s_task> m_tasks;

	std::vector<s_task_graph_data> m_data_lists;
	std::vector<uint32> m_task_lists;
	c_string_table m_string_table;

	// Arrays
	std::vector<s_real_array_element> m_real_array_element_lists;
	std::vector<s_bool_array_element> m_bool_array_element_lists;
	std::vector<s_string_array_element> m_string_array_element_lists;

	// Total number of unique buffers required
	uint32 m_buffer_count;

	// Number of times each buffer is used
	std::vector<uint32> m_buffer_usages;

	// Max amount of concurrency that can exist at any given time during execution
	uint32 m_max_task_concurrency;

	// Information about the usage of buffers, including type and max concurrency
	std::vector<s_buffer_usage_info> m_buffer_usage_info;

	// List of initial tasks
	size_t m_initial_tasks_start;
	size_t m_initial_tasks_count;

	// List of final output buffers
	size_t m_outputs_start;
	size_t m_outputs_count;

	// Remain-active buffer
	size_t m_remain_active_output;

	s_task_graph_globals m_globals;
};

#endif // WAVELANG_ENGINE_TASK_GRAPH_H__
