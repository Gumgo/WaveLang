#pragma once

#include "common/common.h"
#include "common/utility/string_table.h"

#include "engine/buffer_handle.h"
#include "engine/task_function.h"

#include "execution_graph/instrument_stage.h"
#include "execution_graph/node_reference.h"

#include <map>
#include <vector>

class c_execution_graph;
class c_predecessor_resolver;

struct s_task_graph_data {
	// Do not access these directly
	struct {
		c_task_qualified_data_type type;
		bool is_constant;

		union u_value {
			u_value() {} // Allows for c_wrapped_array

			struct {
				// Used when building the graph
				c_node_reference execution_graph_reference_a;
				c_node_reference execution_graph_reference_b;
			};

			h_buffer buffer_handle; // Shared accessor for any buffer, no accessor function for this

			h_buffer real_buffer_handle_in;
			h_buffer real_buffer_handle_out;
			h_buffer real_buffer_handle_inout;
			real32 real_constant_in;
			c_real_array real_array_in;

			h_buffer bool_buffer_handle_in;
			h_buffer bool_buffer_handle_out;
			h_buffer bool_buffer_handle_inout;
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

	h_buffer get_real_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		wl_assert(!is_constant());
		return data.value.real_buffer_handle_in;
	}

	h_buffer get_real_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_out);
		wl_assert(!is_constant());
		return data.value.real_buffer_handle_out;
	}

	h_buffer get_real_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_inout);
		wl_assert(!is_constant());
		return data.value.real_buffer_handle_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		wl_assert(is_constant());
		return data.value.real_constant_in;
	}

	c_real_array get_real_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real, true));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		return data.value.real_array_in;
	}

	h_buffer get_bool_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		wl_assert(!is_constant());
		return data.value.bool_buffer_handle_in;
	}

	h_buffer get_bool_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_out);
		wl_assert(!is_constant());
		return data.value.bool_buffer_handle_out;
	}

	h_buffer get_bool_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_inout);
		wl_assert(!is_constant());
		return data.value.bool_buffer_handle_inout;
	}

	bool get_bool_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		wl_assert(is_constant());
		return data.value.bool_constant_in;
	}

	c_bool_array get_bool_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool, true));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		return data.value.bool_array_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_string));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		wl_assert(is_constant());
		return data.value.string_constant_in;
	}

	c_string_array get_string_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(e_task_primitive_type::k_string, true));
		wl_assert(data.type.get_qualifier() == e_task_qualifier::k_in);
		return data.value.string_array_in;
	}
};

typedef c_wrapped_array<const s_task_graph_data> c_task_graph_data_array;
typedef c_wrapped_array<const uint32> c_task_graph_task_array;

// Iterates through all buffers associated with a task
class c_task_buffer_iterator {
public:
	c_task_buffer_iterator(
		c_task_graph_data_array data_array, c_task_data_type type_mask = c_task_data_type::invalid());
	bool is_valid() const;
	void next();
	h_buffer get_buffer_handle() const;
	c_task_qualified_data_type get_buffer_type() const;
	const s_task_graph_data &get_task_graph_data() const;

protected:
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
	c_task_graph_data_array get_inputs() const;
	c_task_graph_data_array get_outputs() const;
	const s_task_graph_data &get_remain_active_output() const;

	uint32 get_buffer_count() const;
	c_buffer_handle_iterator iterate_buffers() const;
	c_wrapped_array<const s_buffer_usage_info> get_buffer_usage_info() const;
	uint32 get_buffer_usages(h_buffer buffer_handle) const;

private:
	friend class c_task_buffer_iterator_internal;

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
	bool add_task_for_node(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		std::map<c_node_reference, uint32> &nodes_to_tasks);
	void setup_task(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		uint32 task_index,
		const s_task_function_mapping &task_function_mapping);
	h_buffer *get_task_data_array_element_buffer_and_node_reference(
		const s_task_graph_data &argument,
		size_t index,
		c_node_reference *out_node_reference = nullptr);
	void build_task_successor_lists(
		const c_execution_graph &execution_graph,
		const std::map<c_node_reference, uint32> &nodes_to_tasks);
	void allocate_buffers(const c_execution_graph &execution_graph);
	void convert_nodes_to_buffers(
		s_task_graph_data &task_graph_data,
		const std::map<c_node_reference, h_buffer> &nodes_to_buffers);
	void assign_buffer_to_related_nodes(
		const c_execution_graph &execution_graph,
		c_node_reference node_reference,
		const std::map<c_node_reference, c_node_reference> &inout_connections,
		std::map<c_node_reference, h_buffer> &nodes_to_buffers,
		h_buffer buffer_handle);
	void calculate_max_concurrency();
	void add_usage_info_for_buffer_type(const c_predecessor_resolver &predecessor_resolver, c_task_data_type type);
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

	// These are used only to temporarily hold node references while constructing arrays
	// Strings are always constant so they don't need a node reference list
	std::vector<c_node_reference> m_real_array_node_reference_lists;
	std::vector<c_node_reference> m_bool_array_node_reference_lists;

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

	// List of input buffers
	size_t m_inputs_start;
	size_t m_inputs_count;

	// List of final output buffers
	size_t m_outputs_start;
	size_t m_outputs_count;

	// Remain-active buffer
	size_t m_remain_active_output;
};

