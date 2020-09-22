#pragma once

#include "common/common.h"
#include "common/utility/string_table.h"

#include "engine/task_function.h"

#include "execution_graph/instrument_stage.h"
#include "execution_graph/node_reference.h"

#include <map>
#include <vector>

class c_execution_graph;
class c_predecessor_resolver;

using c_task_graph_task_array = c_wrapped_array<const uint32>;

// Iterates through all non-constant buffers associated with a task
class c_task_buffer_iterator {
public:
	c_task_buffer_iterator(c_task_function_arguments arguments);
	bool is_valid() const;
	void next();
	c_buffer *get_buffer() const;
	c_task_qualified_data_type get_buffer_type() const;

protected:
	static constexpr size_t k_invalid_index = static_cast<size_t>(-1);

	c_task_function_arguments m_arguments;
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

	c_task_graph() = default;

	bool build(const c_execution_graph &execution_graph);

	uint32 get_task_count() const;
	uint32 get_max_task_concurrency() const;

	uint32 get_task_function_index(uint32 task_index) const;
	c_task_function_arguments get_task_arguments(uint32 task_index) const;

	size_t get_task_predecessor_count(uint32 task_index) const;
	c_task_graph_task_array get_task_successors(uint32 task_index) const;

	c_task_graph_task_array get_initial_tasks() const;
	c_buffer_array get_inputs() const;
	c_buffer_array get_outputs() const;
	c_buffer *get_remain_active_output() const;

	size_t get_buffer_count() const;
	c_buffer *get_buffer_by_index(size_t index) const;
	size_t get_buffer_index(const c_buffer *buffer) const;

	c_wrapped_array<const s_buffer_usage_info> get_buffer_usage_info() const;

private:
	static constexpr size_t k_invalid_list_index = static_cast<size_t>(-1);

	struct s_task {
		// The function to execute during this task
		uint32 task_function_index;

		// Start index in m_task_function_arguments
		size_t arguments_start;

		// Number of tasks which must complete before this task is executed
		size_t predecessor_count;

		// List of tasks which can execute only after this task has been completed
		size_t successors_start;
		size_t successors_count;
	};

	void clear();
	void create_buffer_for_input(const c_execution_graph &execution_graph, c_node_reference node_reference);
	void assign_buffer_to_output(const c_execution_graph &execution_graph, c_node_reference node_reference);
	bool add_task_for_node(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		std::map<c_node_reference, uint32> &nodes_to_tasks);
	bool setup_task(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		uint32 task_index);

	c_buffer *add_or_get_buffer(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		c_task_data_type data_type);
	c_buffer_array add_or_get_buffer_array(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		c_task_data_type data_type);

	// These functions build constant arrays. Note: the arrays they return are relative to null and should be rebased
	// by calling rebase_arrays(). This is because the underlying array can be reallocated.
	c_real_constant_array build_real_constant_array(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference);
	c_bool_constant_array build_bool_constant_array(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference);
	c_string_constant_array build_string_constant_array(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference);

	void rebase_arrays();
	void rebase_buffers();

	// Adds a string to the string table and returns the offset as a pointer
	const char *add_string(const char *string);

	// Returns the pre-rebased string by looking up the offset in the string table
	const char *get_pre_rebased_string(const char *string) const;

	// Rebases the string relative to the finalized string table
	void rebase_string(const char *&string);

	// Rebases all strings
	void rebase_strings();

	void build_task_successor_lists(
		const c_execution_graph &execution_graph,
		const std::map<c_node_reference, uint32> &nodes_to_tasks);
	void add_task_successor(uint32 predecessor_task_index, uint32 successor_task_index);
	void calculate_max_concurrency();
	void add_usage_info_for_buffer_type(
		const c_predecessor_resolver &predecessor_resolver,
		c_task_data_type data_type);
	uint32 calculate_max_buffer_concurrency(
		const c_predecessor_resolver &task_predecessor_resolver,
		c_task_data_type data_type) const;
	uint32 estimate_max_concurrency(uint32 node_count, const std::vector<bool> &concurrency_matrix) const;

	std::vector<s_task> m_tasks;
	std::vector<s_task_function_argument> m_task_function_arguments;

	// List of all buffers
	std::vector<c_buffer> m_buffers;

	// Used when building the graph - maps nodes to buffers
	std::map<c_node_reference, c_buffer *> m_nodes_to_buffers;

	// Used when building the graph - maps output nodes to input nodes they are being shared with
	std::map<c_node_reference, c_node_reference> m_output_nodes_to_shared_input_nodes;

	// Input and output buffers
	std::vector<c_buffer *> m_input_buffers;
	std::vector<c_buffer *> m_output_buffers;

	// Remain-active output buffer
	c_buffer *m_remain_active_output_buffer = nullptr;

	// Buffer arrays
	std::vector<c_buffer *> m_buffer_arrays;

	// Constant arrays
	std::vector<real32> m_real_constant_arrays;
	std::vector<char> m_bool_constant_arrays; // Workaround: std::vector<bool> is a specialized bitvector
	std::vector<const char *> m_string_constant_arrays;

	// Make sure we can use char in place of bool for our constant array backing storage
	static_assert(sizeof(bool) == sizeof(char), "Size mismatch between char and bool");
	static_assert(alignof(bool) == alignof(char), "Alignment mismatch between char and bool");

	// Stores string constants in an efficient way
	c_string_table m_string_table;

	std::vector<uint32> m_task_lists;

	// Max amount of concurrency that can exist at any given time during execution
	uint32 m_max_task_concurrency = 0;

	// Information about the usage of buffers, including type and max concurrency
	std::vector<s_buffer_usage_info> m_buffer_usage_info;

	// List of initial tasks
	size_t m_initial_tasks_start = k_invalid_list_index;
	size_t m_initial_tasks_count = 0;
};

