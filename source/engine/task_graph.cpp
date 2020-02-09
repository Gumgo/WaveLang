#include "engine/predecessor_resolver.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"

#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/execution_graph.h"

#define OUTPUT_TASK_GRAPH_BUILD_RESULT 0

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
#include <fstream>
#include <iomanip>
#include <sstream>
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)

static const uint32 k_invalid_task = static_cast<uint32>(-1);

typedef s_static_array<e_task_function_mapping_native_module_input_type, k_max_native_module_arguments>
	s_task_function_mapping_native_module_input_type_array;

// Array helpers
static size_t get_task_data_array_count(const s_task_graph_data &data);
static bool is_task_data_array_element_constant(const s_task_graph_data &data, size_t index);
static h_buffer get_task_data_array_element_buffer_const(const s_task_graph_data &data, size_t index);

// Fills out the input types for the given native module call node
static void get_native_module_call_input_types(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	s_task_function_mapping_native_module_input_type_array &out_input_types);

// Returns true if the given input is used as more than once. This indicates that the input cannot be used as an inout
// because overwriting the buffer would invalidate its second usage as an input.
static bool does_native_module_call_input_branch(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	size_t in_arg_index);

static bool do_native_module_inputs_match(
	size_t argument_count,
	const s_task_function_mapping_native_module_input_type_array &native_module_inputs,
	const s_task_function_mapping_native_module_input_type_array &inputs_to_match);

static const s_task_function_mapping *get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	const s_task_function_mapping_native_module_input_type_array &native_module_inputs);

template<typename t_build_array_settings>
typename t_build_array_settings::t_array build_array(
	const c_execution_graph &execution_graph,
	t_build_array_settings &settings,
	c_node_reference node_reference,
	bool &out_is_constant);

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
bool output_task_graph_build_result(const c_task_graph &task_graph, const char *filename);
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)

// Provides the ability to access node references for array buffers when building the graph
class c_task_buffer_iterator_internal : public c_task_buffer_iterator {
public:
	c_task_buffer_iterator_internal(
		c_task_graph &task_graph,
		c_task_graph_data_array data_array,
		c_task_data_type type_mask = c_task_data_type::invalid());

	c_node_reference get_node_reference() const;

private:
	c_task_graph &m_task_graph;
};

struct s_build_real_array_settings {
	typedef s_real_array_element t_element;
	typedef c_real_array t_array;
	static const e_native_module_primitive_type k_native_module_primitive_type = e_native_module_primitive_type::k_real;
	std::vector<t_element> *element_lists;
	std::vector<c_node_reference> *node_reference_lists;

	bool is_element_constant(const t_element &element) { return element.is_constant; }

	void set_element_data(
		size_t element_index,
		bool is_constant,
		const c_execution_graph &execution_graph,
		c_node_reference node_reference) {
		(*element_lists)[element_index].is_constant = is_constant;
		if (is_constant) {
			(*element_lists)[element_index].constant_value =
				execution_graph.get_constant_node_real_value(node_reference);
		} else {
			(*node_reference_lists)[element_index] = node_reference;
		}
	}

	bool compare_element_to_constant_node(
		const t_element &element, const c_execution_graph &execution_graph, c_node_reference constant_node_reference) {
		wl_assert(element.is_constant);
		return element.constant_value == execution_graph.get_constant_node_real_value(constant_node_reference);
	}
};

struct s_build_bool_array_settings {
	typedef s_bool_array_element t_element;
	typedef c_bool_array t_array;
	static const e_native_module_primitive_type k_native_module_primitive_type = e_native_module_primitive_type::k_bool;
	std::vector<t_element> *element_lists;
	std::vector<c_node_reference> *node_reference_lists;

	bool is_element_constant(const t_element &element) { return element.is_constant; }

	void set_element_data(
		size_t element_index,
		bool is_constant,
		const c_execution_graph &execution_graph,
		c_node_reference node_reference) {
		(*element_lists)[element_index].is_constant = is_constant;
		if (is_constant) {
			(*element_lists)[element_index].constant_value =
				execution_graph.get_constant_node_bool_value(node_reference);
		} else {
			(*node_reference_lists)[element_index] = node_reference;
		}
	}

	bool compare_element_to_constant_node(
		const t_element &element, const c_execution_graph &execution_graph, c_node_reference constant_node_reference) {
		wl_assert(element.is_constant);
		return element.constant_value == execution_graph.get_constant_node_bool_value(constant_node_reference);
	}
};

struct s_build_string_array_settings {
	typedef s_string_array_element t_element;
	typedef c_string_array t_array;
	static const e_native_module_primitive_type k_native_module_primitive_type =
		e_native_module_primitive_type::k_string;
	std::vector<t_element> *element_lists;
	std::vector<c_node_reference> *node_reference_lists; // This is always null for strings
	c_string_table *string_table;

	bool is_element_constant(const t_element &element) { return true; }

	void set_element_data(
		size_t element_index,
		bool is_constant,
		const c_execution_graph &execution_graph,
		c_node_reference node_reference) {
		wl_assert(is_constant);
		// Store the offset for now - it will be resolved to a pointer when the string table is finalized
		(*element_lists)[element_index].constant_value = reinterpret_cast<const char *>(
			string_table->add_string(execution_graph.get_constant_node_string_value(node_reference)));
	}

	bool compare_element_to_constant_node(
		const t_element &element, const c_execution_graph &execution_graph, c_node_reference constant_node_reference) {
		return strcmp(
			element.constant_value,
			execution_graph.get_constant_node_string_value(constant_node_reference)) == 0;
	}
};

c_task_buffer_iterator::c_task_buffer_iterator(
	c_task_graph_data_array data_array, c_task_data_type type_mask) {
	m_data_array = data_array;
	m_type_mask = type_mask;
	m_argument_index = static_cast<size_t>(-1);
	m_array_index = static_cast<size_t>(-1);
	next();
}

bool c_task_buffer_iterator::is_valid() const {
	return m_argument_index < m_data_array.get_count();
}

void c_task_buffer_iterator::next() {
	if (m_argument_index == static_cast<size_t>(-1)) {
		wl_assert(m_array_index == static_cast<size_t>(-1));
	} else {
		wl_assert(is_valid());
	}

	bool advance;
	do {
		if (m_argument_index == static_cast<size_t>(-1)) {
			m_argument_index = 0;
			m_array_index = 0;
		} else {
			const s_task_graph_data &current_data = m_data_array[m_argument_index];
			size_t current_array_count = 1;
			if (current_data.data.type.get_data_type().is_array()) {
				current_array_count = get_task_data_array_count(current_data);
			}

			if (m_array_index + 1 < current_array_count) {
				m_array_index++;
			} else {
				m_argument_index++;
				m_array_index = 0;
			}
		}

		if (!is_valid()) {
			advance = false;
		} else {
			const s_task_graph_data &new_data = m_data_array[m_argument_index];

			// Keep skipping past constants
			advance = new_data.data.is_constant;

			if (advance) {
				if (new_data.data.type.get_data_type().is_array()) {
					size_t new_array_count = get_task_data_array_count(new_data);

					if (new_array_count == 0) {
						// Skip past 0-sized arrays
						advance = true;
					} else {
						advance = is_task_data_array_element_constant(new_data, m_array_index);

						if (!advance && m_type_mask.is_valid() && get_buffer_type().get_data_type() != m_type_mask) {
							// Skip buffers which are not of the mask type
							advance = true;
						}
					}
				}
			}
		}
	} while (advance);
}

h_buffer c_task_buffer_iterator::get_buffer_handle() const {
	h_buffer buffer_handle = h_buffer::invalid();

	const s_task_graph_data &data = m_data_array[m_argument_index];
	wl_assert(!data.data.is_constant);

	if (data.data.type.get_data_type().is_array()) {
		buffer_handle = get_task_data_array_element_buffer_const(data, m_array_index);
	} else {
		buffer_handle = data.data.value.buffer_handle;
	}

	return buffer_handle;
}

c_task_qualified_data_type c_task_buffer_iterator::get_buffer_type() const {
	const s_task_graph_data &data = m_data_array[m_argument_index];
	wl_assert(!data.data.is_constant);

	c_task_qualified_data_type result = data.data.type;
	if (result.get_data_type().is_array()) {
		result = c_task_qualified_data_type(result.get_data_type().get_element_type(), result.get_qualifier());
	}

	return result;
}

const s_task_graph_data &c_task_buffer_iterator::get_task_graph_data() const {
	const s_task_graph_data &data = m_data_array[m_argument_index];
	wl_assert(!data.data.is_constant);
	return data;
}

c_task_buffer_iterator_internal::c_task_buffer_iterator_internal(
	c_task_graph &task_graph,
	c_task_graph_data_array data_array,
	c_task_data_type type_mask)
	: c_task_buffer_iterator(data_array, type_mask)
	, m_task_graph(task_graph) {
}

c_node_reference c_task_buffer_iterator_internal::get_node_reference() const {
	c_node_reference node_reference;

	const s_task_graph_data &data = m_data_array[m_argument_index];
	wl_assert(!data.data.is_constant);

	if (data.data.type.get_data_type().is_array()) {
		h_buffer *buffer_handle_unused =
			m_task_graph.get_task_data_array_element_buffer_and_node_reference(data, m_array_index, &node_reference);
	} else {
		node_reference = data.data.value.execution_graph_reference_a;
	}

	return node_reference;
}

c_task_graph::c_task_graph() {
	m_buffer_count = 0;
	m_max_task_concurrency = 0;
	m_initial_tasks_start = k_invalid_list_index;
	m_initial_tasks_count = 0;
	m_inputs_start = k_invalid_list_index;
	m_inputs_count = 0;
	m_outputs_start = k_invalid_list_index;
	m_outputs_count = 0;
	m_remain_active_output = k_invalid_list_index;
}

c_task_graph::~c_task_graph() {
}

uint32 c_task_graph::get_task_count() const {
	return cast_integer_verify<uint32>(m_tasks.size());
}

uint32 c_task_graph::get_max_task_concurrency() const {
	return m_max_task_concurrency;
}

uint32 c_task_graph::get_task_function_index(uint32 task_index) const {
	return m_tasks[task_index].task_function_index;
}

c_task_graph_data_array c_task_graph::get_task_arguments(uint32 task_index) const {
	const s_task &task = m_tasks[task_index];
	const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);
	return c_task_graph_data_array(
		task_function.argument_count == 0 ? nullptr : &m_data_lists[task.arguments_start],
		task_function.argument_count);
}

size_t c_task_graph::get_task_predecessor_count(uint32 task_index) const {
	return m_tasks[task_index].predecessor_count;
}

c_task_graph_task_array c_task_graph::get_task_successors(uint32 task_index) const {
	const s_task &task = m_tasks[task_index];
	return c_task_graph_task_array(
		task.successors_count == 0 ? nullptr : &m_task_lists[task.successors_start],
		task.successors_count);
}

c_task_graph_task_array c_task_graph::get_initial_tasks() const {
	return c_task_graph_task_array(
		m_initial_tasks_count == 0 ? nullptr : &m_task_lists[m_initial_tasks_start],
		m_initial_tasks_count);
}

c_task_graph_data_array c_task_graph::get_inputs() const {
	return c_task_graph_data_array(
		m_inputs_count == 0 ? nullptr : &m_data_lists[m_inputs_start],
		m_inputs_count);
}

c_task_graph_data_array c_task_graph::get_outputs() const {
	return c_task_graph_data_array(
		m_outputs_count == 0 ? nullptr : &m_data_lists[m_outputs_start],
		m_outputs_count);
}

const s_task_graph_data &c_task_graph::get_remain_active_output() const {
	return m_data_lists[m_remain_active_output];
}

uint32 c_task_graph::get_buffer_count() const {
	return m_buffer_count;
}

c_buffer_handle_iterator c_task_graph::iterate_buffers() const {
	return c_buffer_handle_iterator(m_buffer_count);
}

c_wrapped_array<const c_task_graph::s_buffer_usage_info> c_task_graph::get_buffer_usage_info() const {
	return c_wrapped_array<const c_task_graph::s_buffer_usage_info>(
		m_buffer_usage_info.empty() ? nullptr : &m_buffer_usage_info.front(),
		m_buffer_usage_info.size());
}

uint32 c_task_graph::get_buffer_usages(h_buffer buffer_handle) const {
	return m_buffer_usages[buffer_handle.get_data()];
}

bool c_task_graph::build(const c_execution_graph &execution_graph) {
	m_tasks.clear();
	m_data_lists.clear();
	m_task_lists.clear();
	m_string_table.clear();
	m_real_array_element_lists.clear();
	m_bool_array_element_lists.clear();
	m_string_array_element_lists.clear();
	m_real_array_node_reference_lists.clear();
	m_bool_array_node_reference_lists.clear();
	m_buffer_count = 0;
	m_buffer_usages.clear();
	m_max_task_concurrency = 0;
	m_buffer_usage_info.clear();
	m_initial_tasks_start = k_invalid_list_index;
	m_initial_tasks_count = 0;
	m_inputs_start = k_invalid_list_index;
	m_inputs_count = 0;
	m_outputs_start = k_invalid_list_index;
	m_outputs_count = 0;
	m_remain_active_output = k_invalid_list_index;

	// Maps nodes in the execution graph to tasks in the task graph
	std::map<c_node_reference, uint32> nodes_to_tasks;

	bool success = true;

	// Loop over each node and build up the graph
	for (c_node_reference node_reference = execution_graph.nodes_begin();
		success && node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		// We should never encounter these node types, even if the graph was loaded from a file (they should never
		// appear in a file and if they do we should fail to load).
		wl_assert(execution_graph.get_node_type(node_reference) != e_execution_graph_node_type::k_invalid);

		// We only care about native module calls - we access other node types through the call's edges
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call) {
			success &= add_task_for_node(execution_graph, node_reference, nodes_to_tasks);
		}
	}

	if (success) {
		resolve_arrays();
		resolve_strings();
		build_task_successor_lists(execution_graph, nodes_to_tasks);
		allocate_buffers(execution_graph);
		calculate_max_concurrency();
		calculate_buffer_usages();

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
		output_task_graph_build_result(*this, "graph_build_result.csv");
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
	}

	if (!success) {
		m_tasks.clear();
		m_data_lists.clear();
		m_task_lists.clear();
		m_string_table.clear();
		m_real_array_element_lists.clear();
		m_bool_array_element_lists.clear();
		m_string_array_element_lists.clear();
		m_real_array_node_reference_lists.clear();
		m_bool_array_node_reference_lists.clear();
		m_buffer_count = 0;
		m_buffer_usages.clear();
		m_max_task_concurrency = 0;
		m_buffer_usage_info.clear();
		m_initial_tasks_start = k_invalid_list_index;
		m_initial_tasks_count = 0;
		m_inputs_start = k_invalid_list_index;
		m_inputs_count = 0;
		m_outputs_start = k_invalid_list_index;
		m_outputs_count = 0;
		m_remain_active_output = k_invalid_list_index;
	}

	return success;
}

static size_t get_task_data_array_count(const s_task_graph_data &data) {
	wl_assert(data.data.type.get_data_type().is_array());

	size_t result = 0;
	switch (data.data.type.get_data_type().get_primitive_type()) {
	case e_task_primitive_type::k_real:
		result = data.data.value.real_array_in.get_count();
		break;

	case e_task_primitive_type::k_bool:
		result = data.data.value.bool_array_in.get_count();
		break;

	case e_task_primitive_type::k_string:
		result = data.data.value.string_array_in.get_count();
		break;

	default:
		wl_unreachable();
	}

	return result;
}

static bool is_task_data_array_element_constant(const s_task_graph_data &data, size_t index) {
	wl_assert(data.data.type.get_data_type().is_array());

	bool result = true;
	switch (data.data.type.get_data_type().get_primitive_type()) {
	case e_task_primitive_type::k_real:
		result = data.data.value.real_array_in[index].is_constant;
		break;

	case e_task_primitive_type::k_bool:
		result = data.data.value.bool_array_in[index].is_constant;
		break;

	case e_task_primitive_type::k_string:
		result = true;
		break;

	default:
		wl_unreachable();
	}

	return result;
}

static h_buffer get_task_data_array_element_buffer_const(const s_task_graph_data &data, size_t index) {
	wl_assert(data.data.type.get_data_type().is_array());
	wl_assert(!is_task_data_array_element_constant(data, index));

	h_buffer result = h_buffer::invalid();
	switch (data.data.type.get_data_type().get_primitive_type()) {
	case e_task_primitive_type::k_real:
		result = data.data.value.real_array_in[index].buffer_handle_value;
		break;

	case e_task_primitive_type::k_bool:
		result = data.data.value.bool_array_in[index].buffer_handle_value;
		break;

	case e_task_primitive_type::k_string:
		break;

	default:
		wl_unreachable();
	}

	return result;
}

static void get_native_module_call_input_types(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	s_task_function_mapping_native_module_input_type_array &out_input_types) {
	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);

	const s_native_module &native_module = c_native_module_registry::get_native_module(
		execution_graph.get_native_module_call_native_module_index(node_reference));

	size_t input_index = 0;
	for (size_t index = 0; index < native_module.argument_count; index++) {
		if (native_module.arguments[index].type.get_qualifier() == e_native_module_qualifier::k_out) {
			// Skip outputs
			out_input_types[index] = e_task_function_mapping_native_module_input_type::k_none;
		} else {
			wl_assert(native_module_qualifier_is_input(native_module.arguments[index].type.get_qualifier()));

			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, input_index);
			c_node_reference source_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);

			if (execution_graph.get_node_type(source_node_reference) == e_execution_graph_node_type::k_constant) {
				// The input is a constant
				out_input_types[index] = e_task_function_mapping_native_module_input_type::k_variable;
			} else {
				// The input is the result of a module call
				IF_ASSERTS_ENABLED(e_execution_graph_node_type node_type =
					execution_graph.get_node_type(source_node_reference);)
				wl_assert(
					node_type == e_execution_graph_node_type::k_indexed_output ||
					node_type == e_execution_graph_node_type::k_input);

				bool branches = does_native_module_call_input_branch(execution_graph, node_reference, input_index);
				out_input_types[index] = branches ?
					e_task_function_mapping_native_module_input_type::k_variable :
					e_task_function_mapping_native_module_input_type::k_branchless_variable;
			}

			input_index++;
		}
	}
}

static bool does_native_module_call_input_branch(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	size_t in_arg_index) {
	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);

	c_node_reference input_node_reference =
		execution_graph.get_node_incoming_edge_reference(node_reference, in_arg_index);
	c_node_reference source_node_reference =
		execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);

	if (execution_graph.get_node_type(source_node_reference) == e_execution_graph_node_type::k_constant) {
		// We don't care if a constant value is branched because constant values don't take up buffers, they are
		// directly inlined into the task.
		// Note: arrays do use buffers, but we don't perform branch optimization on them
		return false;
	} else {
		IF_ASSERTS_ENABLED(e_execution_graph_node_type node_type =
			execution_graph.get_node_type(source_node_reference);)
		wl_assert(
			node_type == e_execution_graph_node_type::k_indexed_output ||
			node_type == e_execution_graph_node_type::k_input);
		return (execution_graph.get_node_outgoing_edge_count(source_node_reference) > 1);
	}
}

static bool do_native_module_inputs_match(
	size_t argument_count,
	const s_task_function_mapping_native_module_input_type_array &native_module_inputs,
	const s_task_function_mapping_native_module_input_type_array &inputs_to_match) {
	for (size_t arg = 0; arg < argument_count; arg++) {
		e_task_function_mapping_native_module_input_type native_module_input = native_module_inputs[arg];
		e_task_function_mapping_native_module_input_type input_to_match = inputs_to_match[arg];
		wl_assert(valid_enum_index(native_module_input));
		wl_assert(valid_enum_index(input_to_match));

		// "None" inputs should always match
		wl_assert((native_module_input == e_task_function_mapping_native_module_input_type::k_none) ==
			(input_to_match == e_task_function_mapping_native_module_input_type::k_none));

		if (input_to_match == e_task_function_mapping_native_module_input_type::k_variable) {
			// We expect a variable, either branching or branchless
			if (native_module_input != e_task_function_mapping_native_module_input_type::k_variable &&
				native_module_input != e_task_function_mapping_native_module_input_type::k_branchless_variable) {
				return false;
			}
		} else if (input_to_match == e_task_function_mapping_native_module_input_type::k_branchless_variable) {
			// We expect a branchless variable
			if (native_module_input != e_task_function_mapping_native_module_input_type::k_branchless_variable) {
				return false;
			}
		} else {
			wl_assert(input_to_match == e_task_function_mapping_native_module_input_type::k_none);
		}
	}

	return true;
}

static const s_task_function_mapping *get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	const s_task_function_mapping_native_module_input_type_array &native_module_inputs) {
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);
	c_task_function_mapping_list task_function_mapping_list =
		c_task_function_registry::get_task_function_mapping_list(native_module_index);

	// Go through the mappings in order and return the first one that matches
	for (size_t index = 0; index < task_function_mapping_list.get_count(); index++) {
		const s_task_function_mapping &mapping = task_function_mapping_list[index];

		s_task_function_mapping_native_module_input_type_array mapping_input_types;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			mapping_input_types[arg] = mapping.native_module_argument_mapping[arg].input_type;
		}

		bool match = do_native_module_inputs_match(
			native_module.argument_count, native_module_inputs, mapping_input_types);

		if (match) {
			return &mapping;
		}
	}

	return nullptr;
}

void c_task_graph::resolve_arrays() {
	const s_real_array_element *real_array_base_pointer =
		m_real_array_element_lists.empty() ? nullptr : &m_real_array_element_lists.front();
	const s_bool_array_element *bool_array_base_pointer =
		m_bool_array_element_lists.empty() ? nullptr : &m_bool_array_element_lists.front();
	const s_string_array_element *string_array_base_pointer =
		m_string_array_element_lists.empty() ? nullptr : &m_string_array_element_lists.front();

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			if (argument.data.type.get_data_type().is_array()) {
				switch (argument.data.type.get_data_type().get_primitive_type()) {
				case e_task_primitive_type::k_real:
				{
					// We previously stored the offset in the pointer
					c_real_array real_array = argument.data.value.real_array_in;
					argument.data.value.real_array_in = c_real_array(
						real_array_base_pointer + reinterpret_cast<size_t>(real_array.get_pointer()),
						real_array.get_count());
					break;
				}

				case e_task_primitive_type::k_bool:
				{
					// We previously stored the offset in the pointer
					c_bool_array bool_array = argument.data.value.bool_array_in;
					argument.data.value.bool_array_in = c_bool_array(
						bool_array_base_pointer + reinterpret_cast<size_t>(bool_array.get_pointer()),
						bool_array.get_count());
					break;
				}

				case e_task_primitive_type::k_string:
				{
					// We previously stored the offset in the pointer
					c_string_array string_array = argument.data.value.string_array_in;
					argument.data.value.string_array_in = c_string_array(
						string_array_base_pointer + reinterpret_cast<size_t>(string_array.get_pointer()),
						string_array.get_count());
					break;
				}

				default:
					wl_unreachable();
				}
			}
		}
	}
}

void c_task_graph::resolve_strings() {
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			if (argument.data.type.get_data_type().get_primitive_type() == e_task_primitive_type::k_string) {
				if (argument.data.type.get_data_type().is_array()) {
					for (size_t index = 0; index < argument.data.value.string_array_in.get_count(); index++) {
						// Access the element in a non-const way
						size_t element_index =
							&argument.data.value.string_array_in[index] - &m_string_array_element_lists.front();
						s_string_array_element &element = m_string_array_element_lists[element_index];
						element.constant_value = m_string_table.get_string(
							reinterpret_cast<size_t>(element.constant_value));
					}
				} else {
					// We previously stored the offset in the pointer
					argument.data.value.string_constant_in = m_string_table.get_string(
						reinterpret_cast<size_t>(argument.data.value.string_constant_in));
				}
			}
		}
	}
}

bool c_task_graph::add_task_for_node(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	std::map<c_node_reference, uint32> &nodes_to_tasks) {
	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);

	uint32 task_index = cast_integer_verify<uint32>(m_tasks.size());
	m_tasks.push_back(s_task());

	wl_assert(nodes_to_tasks.find(node_reference) == nodes_to_tasks.end());
	nodes_to_tasks.insert(std::make_pair(node_reference, task_index));

	// Construct string representing inputs - 1 extra character for null terminator
	s_task_function_mapping_native_module_input_type_array input_types;
	get_native_module_call_input_types(execution_graph, node_reference, input_types);

	uint32 native_module_index = execution_graph.get_native_module_call_native_module_index(node_reference);

	const s_task_function_mapping *mapping =
		get_task_mapping_for_native_module_and_inputs(native_module_index, input_types);

	if (!mapping) {
		return false;
	} else {
		setup_task(execution_graph, node_reference, task_index, *mapping);
		return true;
	}
}

void c_task_graph::setup_task(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	uint32 task_index,
	const s_task_function_mapping &task_function_mapping) {
	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);

	s_task &task = m_tasks[task_index];
	task.task_function_index =
		c_task_function_registry::get_task_function_index(task_function_mapping.task_function_uid);
	wl_assert(task.task_function_index != k_invalid_task_function_index);

	const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

	// Set up the lists of task arguments
	// Initially, we store the execution graph nodes in this list
	task.arguments_start = m_data_lists.size();

	IF_ASSERTS_ENABLED(size_t old_size = m_data_lists.size());
	m_data_lists.resize(m_data_lists.size() + task_function.argument_count);

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_data_lists.size(); index++) {
		// Fill with 0xffffffff so we can assert that we don't set a value twice
		m_data_lists[index].data.type = c_task_qualified_data_type::invalid();
		m_data_lists[index].data.value.execution_graph_reference_a = c_node_reference();
		m_data_lists[index].data.value.execution_graph_reference_b = c_node_reference();
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Map each input and output to its defined location in the task
	size_t input_output_count =
		execution_graph.get_node_incoming_edge_count(node_reference) +
		execution_graph.get_node_outgoing_edge_count(node_reference);

	size_t input_index = 0;
	size_t output_index = 0;
	for (size_t index = 0; index < input_output_count; index++) {
		// Determine if this is an input or output from the task function mapping input type
		wl_assert(valid_enum_index(task_function_mapping.native_module_argument_mapping[index].input_type));

		bool is_input = task_function_mapping.native_module_argument_mapping[index].input_type !=
			e_task_function_mapping_native_module_input_type::k_none;

		if (is_input) {
			uint32 argument_index =
				task_function_mapping.native_module_argument_mapping[index].task_function_argument_index;
			wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
			s_task_graph_data &argument_data = m_data_lists[task.arguments_start + argument_index];
			argument_data.data.type = task_function.argument_types[argument_index];

			// Obtain the source input node
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, input_index);
			c_node_reference source_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);
			e_execution_graph_node_type source_node_type = execution_graph.get_node_type(source_node_reference);

			bool is_array = argument_data.data.type.get_data_type().is_array();
			bool is_constant = (source_node_type == e_execution_graph_node_type::k_constant);
			if (is_constant) {
				wl_assert(argument_data.data.type.get_qualifier() == e_task_qualifier::k_in);
			}

			bool is_buffer = false;
			switch (argument_data.data.type.get_data_type().get_primitive_type()) {
			case e_task_primitive_type::k_real:
				if (is_array) {
					s_build_real_array_settings settings;
					settings.element_lists = &m_real_array_element_lists;
					settings.node_reference_lists = &m_real_array_node_reference_lists;
					argument_data.data.value.real_array_in =
						build_array(execution_graph, settings, source_node_reference, argument_data.data.is_constant);
				} else if (is_constant) {
					wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
					argument_data.data.is_constant = true;
					argument_data.data.value.real_constant_in =
						execution_graph.get_constant_node_real_value(source_node_reference);
				} else {
					is_buffer = true;
				}
				break;

			case e_task_primitive_type::k_bool:
				if (is_array) {
					s_build_bool_array_settings settings;
					settings.element_lists = &m_bool_array_element_lists;
					settings.node_reference_lists = &m_bool_array_node_reference_lists;
					argument_data.data.value.bool_array_in =
						build_array(execution_graph, settings, source_node_reference, argument_data.data.is_constant);
				} else if (is_constant) {
					wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
					argument_data.data.is_constant = true;
					argument_data.data.value.bool_constant_in =
						execution_graph.get_constant_node_bool_value(source_node_reference);
				} else {
					is_buffer = true;
				}
				break;

			case e_task_primitive_type::k_string:
				if (is_array) {
					s_build_string_array_settings settings;
					settings.element_lists = &m_string_array_element_lists;
					settings.node_reference_lists = nullptr;
					settings.string_table = &m_string_table;
					argument_data.data.value.string_array_in =
						build_array(execution_graph, settings, source_node_reference, argument_data.data.is_constant);
				} else {
					wl_assert(is_constant);
					wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
					// add_string() returns an offset, so temporarily store this in the pointer. This is because as we
					// add more strings, the string table may be resized and pointers would be invalidated, so instead
					// we store offsets and resolve them to pointers at the end.
					argument_data.data.is_constant = true;
					argument_data.data.value.string_constant_in = reinterpret_cast<const char *>(
						m_string_table.add_string(
							execution_graph.get_constant_node_string_value(source_node_reference)));
				}
				break;

			default:
				wl_unreachable();
			}

			switch (argument_data.data.type.get_qualifier()) {
			case e_task_qualifier::k_in:
				if (is_buffer) {
					// Verify that this is a buffer input
					wl_assert(source_node_type == e_execution_graph_node_type::k_indexed_output ||
						source_node_type == e_execution_graph_node_type::k_input);
					wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
					// Store the input node reference for now
					argument_data.data.is_constant = false;
					argument_data.data.value.execution_graph_reference_a = input_node_reference;
				}
				break;

			case e_task_qualifier::k_out:
				wl_vhalt("Can't map inputs to outputs");
				break;

			case e_task_qualifier::k_inout:
				wl_assert(is_buffer);
				// Verify that this is a buffer input
				wl_assert(source_node_type == e_execution_graph_node_type::k_indexed_output ||
					source_node_type == e_execution_graph_node_type::k_input);
				wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
				// Store the input node reference for now
				argument_data.data.is_constant = false;
				argument_data.data.value.execution_graph_reference_a = input_node_reference;
				break;

			default:
				wl_unreachable();
			}

			input_index++;
		} else {
			uint32 argument_index =
				task_function_mapping.native_module_argument_mapping[index].task_function_argument_index;
			wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
			s_task_graph_data &argument_data = m_data_lists[task.arguments_start + argument_index];
			argument_data.data.type = task_function.argument_types[argument_index];

			// Obtain the output node
			c_node_reference output_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, output_index);

			switch (argument_data.data.type.get_qualifier()) {
			case e_task_qualifier::k_in:
				wl_vhalt("Can't map outputs to inputs");
				break;

			case e_task_qualifier::k_out:
				wl_assert(!argument_data.data.value.execution_graph_reference_a.is_valid());
				// Store the output node reference for now
				argument_data.data.value.execution_graph_reference_a = output_node_reference;
				break;

			case e_task_qualifier::k_inout:
				wl_assert(!argument_data.data.value.execution_graph_reference_b.is_valid());
				// Store the output node reference for now
				argument_data.data.value.execution_graph_reference_b = output_node_reference;
				break;

			default:
				wl_unreachable();
			}

			output_index++;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_data_lists.size(); index++) {
		// Make sure we have filled in all values
		wl_assert(m_data_lists[index].data.type.is_valid());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)
}

h_buffer *c_task_graph::get_task_data_array_element_buffer_and_node_reference(
	const s_task_graph_data &data,
	size_t index,
	c_node_reference *out_node_reference) {
	wl_assert(data.data.type.get_data_type().is_array());
	wl_assert(!is_task_data_array_element_constant(data, index));

	h_buffer *result = nullptr;
	switch (data.data.type.get_data_type().get_primitive_type()) {
	case e_task_primitive_type::k_real:
	{
		size_t element_index = &data.data.value.real_array_in[index] - &m_real_array_element_lists.front();
		result = &m_real_array_element_lists[element_index].buffer_handle_value;
		if (out_node_reference) {
			*out_node_reference = m_real_array_node_reference_lists[element_index];
		}
		break;
	}

	case e_task_primitive_type::k_bool:
	{
		size_t element_index = &data.data.value.bool_array_in[index] - &m_bool_array_element_lists.front();
		result = &m_bool_array_element_lists[element_index].buffer_handle_value;
		if (out_node_reference) {
			*out_node_reference = m_bool_array_node_reference_lists[element_index];
		}
		break;
	}

	case e_task_primitive_type::k_string:
		result = nullptr;
		if (out_node_reference) {
			*out_node_reference = c_node_reference();
		}
		break;

	default:
		wl_unreachable();
	}

	return result;
}

void c_task_graph::build_task_successor_lists(
	const c_execution_graph &execution_graph,
	const std::map<c_node_reference, uint32> &nodes_to_tasks) {
	// Clear predecessor counts first since those are set in a random order
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		m_tasks[task_index].predecessor_count = 0;
	}

	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (nodes_to_tasks.find(node_reference) == nodes_to_tasks.end()) {
			// This node is not a task
			wl_assert(
				execution_graph.get_node_type(node_reference) != e_execution_graph_node_type::k_native_module_call);
			continue;
		}

		wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);
		uint32 task_index = nodes_to_tasks.at(node_reference);
		s_task &task = m_tasks[task_index];

		task.successors_start = m_task_lists.size();
		task.successors_count = 0;
		for (size_t edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_reference); edge++) {
			c_node_reference output_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, edge);

			for (size_t dest = 0; dest < execution_graph.get_node_outgoing_edge_count(output_node_reference); dest++) {
				c_node_reference dest_node_reference =
					execution_graph.get_node_outgoing_edge_reference(output_node_reference, dest);

				// We only care about native module inputs
				if (execution_graph.get_node_type(dest_node_reference) !=
					e_execution_graph_node_type::k_indexed_input) {
					continue;
				}

				c_node_reference input_native_module_call_node_reference =
					execution_graph.get_node_outgoing_edge_reference(dest_node_reference, 0);

				// Skip arrays
				if (execution_graph.get_node_type(input_native_module_call_node_reference) !=
					e_execution_graph_node_type::k_native_module_call) {
					continue;
				}

				// Find the task belonging to this node
				uint32 successor_task_index = nodes_to_tasks.at(input_native_module_call_node_reference);
				wl_assert(successor_task_index != k_invalid_task);

				// Avoid adding duplicates
				bool duplicate = false;
				for (size_t successor = 0; !duplicate && successor < task.successors_count; successor++) {
					uint32 existing_successor_index = m_task_lists[task.successors_start + successor];
					duplicate = (successor_task_index == existing_successor_index);
				}

				if (!duplicate) {
					m_task_lists.push_back(successor_task_index);
					task.successors_count++;

					// Add this task as a predecessor to the successor
					m_tasks[successor_task_index].predecessor_count++;
				}
			}
		}
	}

	// Find all initial tasks - tasks with no predecessors
	m_initial_tasks_start = m_task_lists.size();
	m_initial_tasks_count = 0;
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		if (get_task_predecessor_count(task_index) == 0) {
			m_initial_tasks_count++;
			m_task_lists.push_back(task_index);
		}
	}
}

void c_task_graph::allocate_buffers(const c_execution_graph &execution_graph) {
	// Build the input buffer list. The buffers are listed in input-index order
	m_inputs_count = 0;
	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_input) {
			m_inputs_count++;
		}
	}

	m_inputs_start = m_data_lists.size();
	// Make space for all inputs
	m_data_lists.resize(m_data_lists.size() + m_inputs_count);

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = m_inputs_start; index < m_data_lists.size(); index++) {
		m_data_lists[index].data.type = c_task_qualified_data_type::invalid();
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_input) {
			uint32 input_index = execution_graph.get_input_node_input_index(node_reference);

			// Offset into the list by the output index
			size_t data_list_index = m_inputs_start + input_index;
			s_task_graph_data &input_data = m_data_lists[data_list_index];

			input_data.data.type = c_task_qualified_data_type(e_task_primitive_type::k_real, e_task_qualifier::k_in);
			input_data.data.value.execution_graph_reference_a = node_reference;
			input_data.data.value.execution_graph_reference_b = c_node_reference();
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	// Verify that we assigned all inputs
	for (size_t index = m_inputs_start; index < m_data_lists.size(); index++) {
		wl_assert(m_data_lists[index].data.type.is_valid());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Associates input and output nodes for when inout buffers have been chosen
	std::map<c_node_reference, c_node_reference> inout_connections;

	// Maps input/output nodes to allocated buffers
	std::map<c_node_reference, h_buffer> nodes_to_buffers;

	// Associate nodes which share an inout buffer
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			const s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			if (argument.data.type.get_qualifier() == e_task_qualifier::k_inout) {
				inout_connections.insert(std::make_pair(
					argument.data.value.execution_graph_reference_a,
					argument.data.value.execution_graph_reference_b));
				inout_connections.insert(std::make_pair(
					argument.data.value.execution_graph_reference_b,
					argument.data.value.execution_graph_reference_a));
			}
		}
	}

	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_input) {
			if (nodes_to_buffers.find(node_reference) != nodes_to_buffers.end()) {
				// Already allocated, skip this one
			} else {
				assign_buffer_to_related_nodes(
					execution_graph,
					node_reference,
					inout_connections,
					nodes_to_buffers,
					h_buffer::construct(m_buffer_count));
				m_buffer_count++;
			}
		}
	}

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		c_task_graph_data_array arguments = get_task_arguments(task_index);
		for (c_task_buffer_iterator_internal it(*this, arguments); it.is_valid(); it.next()) {
#if IS_TRUE(ASSERTS_ENABLED)
			const s_task_graph_data &argument = it.get_task_graph_data();
			wl_assert(!argument.data.is_constant);

			c_node_reference node_reference_a = argument.data.value.execution_graph_reference_a;
			c_node_reference node_reference_b = argument.data.value.execution_graph_reference_b;
			if (argument.data.type.get_qualifier() == e_task_qualifier::k_inout) {
				// Inout buffers should have both nodes filled in
				wl_assert(node_reference_b.is_valid());
				// This mapping should always be equal because this is an inout buffer - there is only one buffer
				if (nodes_to_buffers.find(node_reference_a) != nodes_to_buffers.end()) {
					wl_assert(nodes_to_buffers.at(node_reference_a) == nodes_to_buffers.at(node_reference_b));
				} else {
					wl_assert(nodes_to_buffers.find(node_reference_b) == nodes_to_buffers.end());
				}
			} else if (!argument.data.type.get_data_type().is_array()) {
				wl_assert(!node_reference_b.is_valid());
			}
#endif // IS_TRUE(ASSERTS_ENABLED)

			c_node_reference node_reference = it.get_node_reference();
			if (nodes_to_buffers.find(node_reference) != nodes_to_buffers.end()) {
				// Already allocated, skip this one
			} else {
				assign_buffer_to_related_nodes(
					execution_graph,
					node_reference,
					inout_connections,
					nodes_to_buffers,
					h_buffer::construct(m_buffer_count));
				m_buffer_count++;
			}
		}
	}

	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_input) {
			uint32 input_index = execution_graph.get_input_node_input_index(node_reference);

			// Offset into the list by the output index
			size_t data_list_index = m_inputs_start + input_index;
			s_task_graph_data &input_data = m_data_lists[data_list_index];

			// For each buffer, find all attached nodes and create a mapping
			convert_nodes_to_buffers(input_data, nodes_to_buffers);
		}
	}

	// Finally, follow the node-to-buffer mapping
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			// For each buffer, find all attached nodes and create a mapping
			convert_nodes_to_buffers(argument, nodes_to_buffers);
		}
	}

	// Build final output buffer list. The buffers are listed in output-index order
	m_outputs_count = 0;
	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_output) {
			uint32 output_index = execution_graph.get_output_node_output_index(node_reference);
			if (output_index != c_execution_graph::k_remain_active_output_index) {
				m_outputs_count++;
			}
		}
	}

	m_outputs_start = m_data_lists.size();
	// Make space for all outputs including the remain-active output
	m_data_lists.resize(m_data_lists.size() + m_outputs_count + 1);

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = m_outputs_start; index < m_data_lists.size(); index++) {
		m_data_lists[index].data.type = c_task_qualified_data_type::invalid();
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		if (execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_output) {
			uint32 output_index = execution_graph.get_output_node_output_index(node_reference);
			bool is_remain_active_output = (output_index == c_execution_graph::k_remain_active_output_index);

			// Offset into the list by the output index
			size_t data_list_index = m_outputs_start + (is_remain_active_output ? m_outputs_count : output_index);

			if (is_remain_active_output) {
				wl_assert(m_remain_active_output == k_invalid_list_index);
				m_remain_active_output = data_list_index;
			}

			s_task_graph_data &output_data = m_data_lists[data_list_index];
			wl_assert(!output_data.data.type.is_valid());

			output_data.data.type = is_remain_active_output ?
				c_task_qualified_data_type(e_task_primitive_type::k_bool, e_task_qualifier::k_in) :
				c_task_qualified_data_type(e_task_primitive_type::k_real, e_task_qualifier::k_in);

			if (nodes_to_buffers.find(node_reference) != nodes_to_buffers.end()) {
				output_data.data.is_constant = false;
				// Buffer type is bool if remain-active output, real otherwise
				output_data.data.value.buffer_handle = nodes_to_buffers[node_reference];
			} else {
				// This output has a constant piped directly into it
				c_node_reference constant_node_reference =
					execution_graph.get_node_incoming_edge_reference(node_reference, 0);
				wl_assert(
					execution_graph.get_node_type(constant_node_reference) == e_execution_graph_node_type::k_constant);
				output_data.data.is_constant = true;

				if (is_remain_active_output) {
					output_data.data.value.bool_constant_in =
						execution_graph.get_constant_node_bool_value(constant_node_reference);
				} else {
					output_data.data.value.real_constant_in =
						execution_graph.get_constant_node_real_value(constant_node_reference);
				}
			}
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	// Verify that we assigned all outputs
	for (size_t index = m_outputs_start; index < m_data_lists.size(); index++) {
		wl_assert(m_data_lists[index].data.type.is_valid());
	}

	wl_assert(m_remain_active_output != k_invalid_list_index);
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_task_graph::convert_nodes_to_buffers(
	s_task_graph_data &task_graph_data,
	const std::map<c_node_reference, h_buffer> &nodes_to_buffers) {
	if (task_graph_data.data.type.get_data_type().is_array()) {
		size_t array_count = get_task_data_array_count(task_graph_data);
		for (size_t index = 0; index < array_count; index++) {
			if (!is_task_data_array_element_constant(task_graph_data, index)) {
				c_node_reference node_reference;
				h_buffer *buffer_handle_value =
					get_task_data_array_element_buffer_and_node_reference(task_graph_data, index, &node_reference);
				wl_assert(nodes_to_buffers.find(node_reference) != nodes_to_buffers.end());
				*buffer_handle_value = nodes_to_buffers.at(node_reference);
				wl_assert(buffer_handle_value->is_valid());
			}
		}
	} else if (!task_graph_data.data.is_constant) {
		if (task_graph_data.data.type.get_qualifier() == e_task_qualifier::k_inout) {
			wl_assert(nodes_to_buffers.at(task_graph_data.data.value.execution_graph_reference_a) ==
				nodes_to_buffers.at(task_graph_data.data.value.execution_graph_reference_b));
		}

		task_graph_data.data.value.buffer_handle =
			nodes_to_buffers.at(task_graph_data.data.value.execution_graph_reference_a);
		wl_assert(task_graph_data.data.value.buffer_handle.is_valid());
	}

	// These are no longer needed
	m_real_array_node_reference_lists.clear();
	m_bool_array_node_reference_lists.clear();
}

void c_task_graph::assign_buffer_to_related_nodes(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	const std::map<c_node_reference, c_node_reference> &inout_connections,
	std::map<c_node_reference, h_buffer> &nodes_to_buffers,
	h_buffer buffer_handle) {
	if (nodes_to_buffers.find(node_reference) != nodes_to_buffers.end()) {
		// Already assigned, don't recurse forever
		wl_assert(nodes_to_buffers.at(node_reference) == buffer_handle);
		return;
	}

	nodes_to_buffers[node_reference] = buffer_handle;

	e_execution_graph_node_type node_type = execution_graph.get_node_type(node_reference);
	if (node_type == e_execution_graph_node_type::k_indexed_input) {
		// Recurse on all connected inputs
		for (uint32 edge = 0; edge < execution_graph.get_node_incoming_edge_count(node_reference); edge++) {
			c_node_reference edge_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, edge);

			// Skip constants (including arrays)
			if (execution_graph.get_node_type(edge_node_reference) == e_execution_graph_node_type::k_constant) {
				continue;
			}

			// This call will either be performed on indexed outputs or input nodes, which will be asserted on the
			// recursive call
			assign_buffer_to_related_nodes(
				execution_graph, edge_node_reference, inout_connections, nodes_to_buffers, buffer_handle);
		}
	} else if (node_type == e_execution_graph_node_type::k_indexed_output) {
		// Recurse on all connected outputs
		for (uint32 edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_reference); edge++) {
			c_node_reference edge_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, edge);

			// This call will either be performed on indexed inputs or output nodes, which will be asserted on the
			// recursive call
			assign_buffer_to_related_nodes(
				execution_graph, edge_node_reference, inout_connections, nodes_to_buffers, buffer_handle);
		}
	} else if (node_type == e_execution_graph_node_type::k_input) {
		// Recurse on all connected outputs
		for (uint32 edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_reference); edge++) {
			c_node_reference edge_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, edge);

			// This call will either be performed on indexed inputs or output nodes, which will be asserted on the
			// recursive call
			assign_buffer_to_related_nodes(
				execution_graph, edge_node_reference, inout_connections, nodes_to_buffers, buffer_handle);
		}
	} else if (node_type == e_execution_graph_node_type::k_output) {
		// No recursive work to do here
	} else {
		// We should never hit any other node types
		wl_unreachable();
	}

	if (inout_connections.find(node_reference) != inout_connections.end()) {
		// This node has been assigned to an inout buffer, so also run on the corresponding input or output
		assign_buffer_to_related_nodes(
			execution_graph, inout_connections.at(node_reference), inout_connections, nodes_to_buffers, buffer_handle);
	}
}

void c_task_graph::calculate_max_concurrency() {
	// In this function we calculate the worst case for the number of buffers which must be in memory concurrently. This
	// number may be much less than m_buffer_count because in many cases, buffer B can only ever be created once buffer
	// A is no longer needed.

	// The optimal solution is the following: create an NxN matrix, where N is the total number of tasks. Each cell
	// (a,b) contains a boolean value determining whether it is possible for tasks a and b to execute in parallel (this
	// can be determined by analyzing task successors/predecessors). All cells (a,a) are set to true. Next, create a new
	// graph containing a node for each buffer used for each task (so if a task uses B buffers, B nodes would be
	// created). For two nodes xb1 and xb2 (where x is the original task index and b is the original buffer index), add
	// an edge if the cell (x1,x2) in the matrix is set. The maximum buffer concurrency is equal to the size of the
	// maximum clique in this graph.

	// Unfortunately, calculating the maximum clique in a graph is NP-hard, so we will take a different approach. We
	// start by assuming all buffers are independent. We then attempt to merge buffers which cannot ever exist in
	// parallel. This is a greedy (and suboptimal) algorithm because we don't know if merging buffers a and b will
	// prevent future merges that could be better.

	// In the future, we may want to use a heuristic to try to guess the best buffers to merge next, but for now we will
	// just pick the first valid pair we come across, for simplicity.

	// 1) Find task successors/predecessors. We use a pretty inefficient but simple algorithm, but make it better by
	// ORing uint32s to combine successor lists.

	c_predecessor_resolver predecessor_resolver(cast_integer_verify<uint32>(m_tasks.size()));
	// First add all direct successors
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];

		for (size_t suc = 0; suc < task.successors_count; suc++) {
			uint32 successor_index = m_task_lists[task.successors_start + suc];
			predecessor_resolver.a_precedes_b(task_index, successor_index);
		}
	}

	predecessor_resolver.resolve();

	{
		// At this point it is convenient to calculate task concurrency
		std::vector<bool> task_concurrency(m_tasks.size() * m_tasks.size());
		for (uint32 task_b = 0; task_b < m_tasks.size(); task_b++) {
			for (uint32 task_a = 0; task_a < m_tasks.size(); task_a++) {
				task_concurrency[task_b * m_tasks.size() + task_a] =
					predecessor_resolver.are_a_b_concurrent(task_a, task_b);
			}
		}

		m_max_task_concurrency =
			estimate_max_concurrency(cast_integer_verify<uint32>(m_tasks.size()), task_concurrency);
	}

	// 2) Determine which buffers are concurrent. This means that it is possible or necessary for the buffers to exist
	// at the same time.

	// Each buffer type can only be concurrent with other buffers of its same type, so first we identify all the
	// different buffer types that exist

	c_task_graph_data_array inputs = get_inputs();
	for (size_t input_index = 0; input_index < inputs.get_count(); input_index++) {
		const s_task_graph_data &input_data = inputs[input_index];

		wl_assert(!input_data.is_constant());
		c_task_data_type buffer_type = input_data.get_type().get_data_type();
		add_usage_info_for_buffer_type(predecessor_resolver, buffer_type);
	}

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		for (c_task_buffer_iterator it(get_task_arguments(task_index)); it.is_valid(); it.next()) {
			c_task_data_type buffer_type = it.get_buffer_type().get_data_type();
			add_usage_info_for_buffer_type(predecessor_resolver, buffer_type);
		}
	}
}

void c_task_graph::add_usage_info_for_buffer_type(
	const c_predecessor_resolver &predecessor_resolver, c_task_data_type type) {
	// Skip if we've already calculated for this type
	size_t found = false;
	for (size_t info_index = 0; info_index < m_buffer_usage_info.size(); info_index++) {
		if (m_buffer_usage_info[info_index].type == type) {
			found = true;
			break;
		}
	}

	if (!found) {
		m_buffer_usage_info.push_back(s_buffer_usage_info());
		s_buffer_usage_info &info = m_buffer_usage_info.back();
		info.type = type;
		info.max_concurrency = calculate_max_buffer_concurrency(predecessor_resolver, type);
	}
}

uint32 c_task_graph::calculate_max_buffer_concurrency(
	const c_predecessor_resolver &task_predecessor_resolver, c_task_data_type type) const {
	// To determine whether two tasks (a,b) can be parallel, we just ask whether (a !precedes b) and (b !precedes a).
	// The predecessor resolver provides this data.

	// The information we actually want to know is whether we can merge buffers, so we actually need to know which
	// buffers can exist in parallel. We can convert task concurrency data to buffer concurrency data.
	std::vector<bool> buffer_concurrency(m_buffer_count * m_buffer_count, false);

	// Not all buffers are of the correct type - this bitvector masks out invalid buffers
	std::vector<bool> buffer_type_mask(m_buffer_count, false);
	uint32 valid_buffer_count = 0;

	// For any task, all buffer pairs of the same type are concurrent
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		for (c_task_buffer_iterator it_a(get_task_arguments(task_index), type); it_a.is_valid(); it_a.next()) {
			h_buffer buffer_a = it_a.get_buffer_handle();

			if (!buffer_type_mask[buffer_a.get_data()]) {
				buffer_type_mask[buffer_a.get_data()] = true;
				valid_buffer_count++;
			}

			for (c_task_buffer_iterator it_b(get_task_arguments(task_index), type); it_b.is_valid(); it_b.next()) {
				h_buffer buffer_b = it_b.get_buffer_handle();
				buffer_concurrency[buffer_a.get_data() * m_buffer_count + buffer_b.get_data()] = true;
				buffer_concurrency[buffer_b.get_data() * m_buffer_count + buffer_a.get_data()] = true;
			}
		}
	}

	// For any pair of concurrent tasks, all buffer pairs between the two tasks are concurrent
	for (uint32 task_a_index = 0; task_a_index < m_tasks.size(); task_a_index++) {
		for (uint32 task_b_index = task_a_index + 1; task_b_index < m_tasks.size(); task_b_index++) {
			if (!task_predecessor_resolver.are_a_b_concurrent(task_a_index, task_b_index)) {
				continue;
			}

			for (c_task_buffer_iterator it_a(get_task_arguments(task_a_index), type); it_a.is_valid(); it_a.next()) {
				h_buffer buffer_a = it_a.get_buffer_handle();
				for (c_task_buffer_iterator it_b(get_task_arguments(task_b_index), type);
					it_b.is_valid();
					it_b.next()) {
					h_buffer buffer_b = it_b.get_buffer_handle();
					buffer_concurrency[buffer_a.get_data() * m_buffer_count + buffer_b.get_data()] = true;
					buffer_concurrency[buffer_b.get_data() * m_buffer_count + buffer_a.get_data()] = true;
				}
			}
		}
	}

	// All input buffers are concurrent
	for (size_t input_a = 0; input_a < m_inputs_count; input_a++) {
		const s_task_graph_data &input_a_data = m_data_lists[m_inputs_start + input_a];
		wl_assert(!input_a_data.is_constant());
		h_buffer input_buffer_a_handle = input_a_data.data.value.buffer_handle;

		if (input_a_data.get_type().get_data_type() == type && !buffer_type_mask[input_buffer_a_handle.get_data()]) {
			buffer_type_mask[input_buffer_a_handle.get_data()] = true;
			valid_buffer_count++;
		}

		for (size_t input_b = input_a + 1; input_b < m_inputs_count; input_b++) {
			const s_task_graph_data &input_b_data = m_data_lists[m_inputs_start + input_b];
			wl_assert(!input_b_data.is_constant());
			h_buffer input_buffer_b_handle = input_b_data.data.value.buffer_handle;

			uint32 index_ab = input_buffer_a_handle.get_data() * m_buffer_count + input_buffer_b_handle.get_data();
			uint32 index_ba = input_buffer_b_handle.get_data() * m_buffer_count + input_buffer_a_handle.get_data();
			buffer_concurrency[index_ab] = true;
			buffer_concurrency[index_ba] = true;
		}
	}

	// All output buffers are concurrent
	for (size_t output_a = 0; output_a < m_outputs_count; output_a++) {
		const s_task_graph_data &output_a_data = m_data_lists[m_outputs_start + output_a];
		c_task_buffer_iterator it_a(c_task_graph_data_array(&output_a_data, 1));
		if (!it_a.is_valid()) {
			continue;
		}

		h_buffer output_buffer_a_handle = it_a.get_buffer_handle();

		for (size_t output_b = output_a + 1; output_b < m_outputs_count; output_b++) {
			const s_task_graph_data &output_b_data = m_data_lists[m_outputs_start + output_b];
			c_task_buffer_iterator it_b(c_task_graph_data_array(&output_b_data, 1));
			if (!it_b.is_valid()) {
				continue;
			}

			h_buffer output_buffer_b_handle = it_b.get_buffer_handle();

			uint32 index_ab = output_buffer_a_handle.get_data() * m_buffer_count + output_buffer_b_handle.get_data();
			uint32 index_ba = output_buffer_b_handle.get_data() * m_buffer_count + output_buffer_a_handle.get_data();
			buffer_concurrency[index_ab] = true;
			buffer_concurrency[index_ba] = true;
		}
	}

	// The output buffers are also concurrent with the remain-active output buffer, but they are different types (real
	// and bool) so we don't need to bother marking them as concurrent since they will never be combined anyway

	// Reduce the matrix down to only valid buffers. The actual indices no longer matter at this point.
	std::vector<bool> reduced_buffer_concurrency;
	reduced_buffer_concurrency.reserve(valid_buffer_count * valid_buffer_count);

	for (uint32 buffer_a_index = 0; buffer_a_index < m_buffer_count; buffer_a_index++) {
		if (!buffer_type_mask[buffer_a_index]) {
			continue;
		}

		for (uint32 buffer_b_index = 0; buffer_b_index < m_buffer_count; buffer_b_index++) {
			if (!buffer_type_mask[buffer_b_index]) {
				continue;
			}

			reduced_buffer_concurrency.push_back(buffer_concurrency[buffer_a_index * m_buffer_count + buffer_b_index]);
		}
	}

	wl_assert(reduced_buffer_concurrency.size() == valid_buffer_count * valid_buffer_count);

	// Merge buffers which are not concurrent. To merge buffer x with buffers S=(a,b,c,...), buffer x must not be
	// concurrent with any buffers in S.
	return estimate_max_concurrency(valid_buffer_count, reduced_buffer_concurrency);
}

uint32 c_task_graph::estimate_max_concurrency(uint32 node_count, const std::vector<bool> &concurrency_matrix) const {
	wl_assert(node_count * node_count == concurrency_matrix.size());

	static const size_t k_none = static_cast<size_t>(-1);
	struct s_merged_node_group {
		uint32 node_index;
		size_t next;

	};

	std::vector<s_merged_node_group> merge_group_lists;
	std::vector<size_t> merge_group_heads;

	// Attempt to merge each node with the first possible merge group we encounter. If no merge is successful, make a
	// new merge group.
	for (uint32 node_index_a = 0; node_index_a < node_count; node_index_a++) {
		bool merged = false;
		for (size_t merge_group_index = 0;
			!merged && merge_group_index < merge_group_heads.size();
			merge_group_index++) {
			bool can_merge = true;
			size_t node_index_b = merge_group_heads[merge_group_index];
			while (node_index_b != k_none && can_merge) {
				s_merged_node_group merged_node_group = merge_group_lists[node_index_b];
				if (concurrency_matrix[node_index_a * node_count + merged_node_group.node_index]) {
					// These two nodes are concurrent, so we can't merge them
					can_merge = false;
				}

				node_index_b = merged_node_group.next;
			}

			if (can_merge) {
				size_t new_index = merge_group_lists.size();

				s_merged_node_group merged_node_group;
				merged_node_group.node_index = node_index_a;
				merged_node_group.next = merge_group_heads[merge_group_index];
				merge_group_lists.push_back(merged_node_group);
				merge_group_heads[merge_group_index] = new_index;

				merged = true;
			}
		}

		if (!merged) {
			// No valid merges, add a new merge group
			merge_group_heads.push_back(merge_group_lists.size());

			s_merged_node_group merged_node_group;
			merged_node_group.node_index = node_index_a;
			merged_node_group.next = k_none;
			merge_group_lists.push_back(merged_node_group);
		}
	}

	// The number of merge groups we end up with is the max concurrency. It is an overestimate, meaning we will reserve
	// more memory than necessary.
	return cast_integer_verify<uint32>(merge_group_heads.size());
}

void c_task_graph::calculate_buffer_usages() {
	m_buffer_usages.resize(m_buffer_count, 0);

	// Each time a buffer is used in a task, increment its usage count
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		for (c_task_buffer_iterator it(get_task_arguments(task_index)); it.is_valid(); it.next()) {
			m_buffer_usages[it.get_buffer_handle().get_data()]++;
		}
	}

	// Each output buffer contributes to usage to prevent the buffer from deallocating at the end of the last task
	for (size_t index = 0; index < m_outputs_count; index++) {
		const s_task_graph_data &output = m_data_lists[m_outputs_start + index];
		c_task_buffer_iterator it(c_task_graph_data_array(&output, 1));
		if (!it.is_valid()) {
			continue;
		}

		m_buffer_usages[it.get_buffer_handle().get_data()]++;
	}

	// Remain-active buffer contributes as well
	{
		const s_task_graph_data &remain_active_output = m_data_lists[m_remain_active_output];
		c_task_buffer_iterator it(c_task_graph_data_array(&remain_active_output, 1));
		if (it.is_valid()) {
			m_buffer_usages[it.get_buffer_handle().get_data()]++;
		}
	}
}

template<typename t_build_array_settings>
typename t_build_array_settings::t_array build_array(
	const c_execution_graph &execution_graph,
	t_build_array_settings &settings,
	c_node_reference node_reference,
	bool &out_is_constant) {
	// Build a list of array elements using the array node at node_reference.

	typedef typename t_build_array_settings::t_array t_array;
	typedef typename t_build_array_settings::t_element t_element;

	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_constant);
	wl_assert(execution_graph.get_constant_node_data_type(node_reference) ==
		c_native_module_data_type(t_build_array_settings::k_native_module_primitive_type, true));

	size_t array_count = execution_graph.get_node_incoming_edge_count(node_reference);

	// First scan the list of elements to see if this array has already been instantiated
	out_is_constant = true;
	bool match = false;
	size_t start_offset = static_cast<size_t>(-1);
	for (size_t start_index = 0; match && (start_index + array_count < settings.element_lists->size()); start_index++) {
		// Check if the next N elements match starting at start_index
		bool match_inner = true;
		for (size_t array_index = 0; match_inner && array_index < array_count; array_index++) {
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, array_index);
			c_node_reference source_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);
			bool source_node_is_constant =
				(execution_graph.get_node_type(source_node_reference) == e_execution_graph_node_type::k_constant);

			size_t element_index = start_index + array_index;
			const t_element &element = (*settings.element_lists)[element_index];
			bool is_element_constant = settings.is_element_constant(element);

			if (is_element_constant && source_node_is_constant) {
				match_inner =
					settings.compare_element_to_constant_node(element, execution_graph, source_node_reference);
			} else if (!is_element_constant && !source_node_is_constant) {
				wl_assert(settings.node_reference_lists); // Strings are always constant
				match_inner = ((*settings.node_reference_lists)[element_index] == source_node_reference);
				out_is_constant = false;
			} else {
				match_inner = false;
			}
		}

		if (match_inner) {
			match = true;
			start_offset = start_index;
		}
	}

	if (!match) {
		// If we didn't match, we need to construct elements
		start_offset = settings.element_lists->size();

		out_is_constant = true;
		for (size_t array_index = 0; array_index < array_count; array_index++) {
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, array_index);
			c_node_reference source_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);
			bool source_node_is_constant =
				(execution_graph.get_node_type(source_node_reference) == e_execution_graph_node_type::k_constant);

			settings.element_lists->push_back(t_element());
			t_element &element = settings.element_lists->back();
			if (settings.node_reference_lists) {
				settings.node_reference_lists->push_back(c_node_reference());
			}

			settings.set_element_data(
				start_offset + array_index, source_node_is_constant, execution_graph, source_node_reference);
			out_is_constant &= source_node_is_constant;
		}
	}

	// We store the offset in the pointer, which we will later convert
	const t_element *start_offset_pointer = reinterpret_cast<const t_element *>(start_offset);
	return t_array(start_offset_pointer, array_count);
}

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
static bool output_task_graph_build_result(const c_task_graph &task_graph, const char *filename) {
	std::ofstream out(filename);
	if (!out.is_open()) {
		return false;
	}

	out << "Task Index,Task UID,Task Name";
	for (size_t arg = 0; arg < k_max_task_function_arguments; arg++) {
		out << ", Arg " << arg;
	}
	out << "\n";

	for (uint32 index = 0; index < task_graph.get_task_count(); index++) {
		uint32 task_function_index = task_graph.get_task_function_index(index);
		const s_task_function &task_function = c_task_function_registry::get_task_function(task_function_index);

		std::stringstream uid_stream;
		uid_stream << "0x" << std::setfill('0') << std::setw(2) << std::hex;
		for (size_t b = 0; b < NUMBEROF(task_function.uid.data); b++) {
			uid_stream << static_cast<uint32>(task_function.uid.data[b]);
		}

		out << index << "," <<
			uid_stream.str() << "," <<
			task_function.name.get_string();

		c_task_graph_data_array arguments = task_graph.get_task_arguments(index);
		for (size_t arg = 0; arg < arguments.get_count(); arg++) {
			out << ",";
			if (arguments[arg].data.type.get_data_type().is_array()) {
				// $TODO support this case
				out << "array";
			} else {
				if (arguments[arg].data.is_constant) {
					switch (arguments[arg].data.type.get_data_type().get_primitive_type()) {
					case e_task_primitive_type::k_real:
						out << arguments[arg].data.value.real_constant_in;
						break;

					case e_task_primitive_type::k_bool:
						out << (arguments[arg].data.value.bool_constant_in ? "true" : "false");
						break;

					case e_task_primitive_type::k_string:
						out << arguments[arg].data.value.string_constant_in;
						break;

					default:
						wl_unreachable();
					}
				} else {
					out << "[" << arguments[arg].data.value.buffer_handle.get_data() << "]";
				}
			}
		}
		out << "\n";

		// $TODO output task dependency information
	}

	return !out.fail();
}
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
