#include "engine/task_graph.h"
#include "engine/task_function_registry.h"
#include "engine/predecessor_resolver.h"
#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/execution_graph.h"

static const uint32 k_invalid_task = static_cast<uint32>(-1);

const uint32 c_task_graph::k_invalid_buffer;

// Fills out the input types for the given native module call node
static void get_native_module_call_input_types(const c_execution_graph &execution_graph, uint32 node_index,
	e_task_function_mapping_native_module_input_type (&out_input_types)[k_max_native_module_arguments]);

// Returns true if the given input is used as more than once. This indicates that the input cannot be used as an inout
// because overwriting the buffer would invalidate its second usage as an input.
static bool does_native_module_call_input_branch(const c_execution_graph &execution_graph, uint32 node_index,
	size_t in_arg_index);

static bool do_native_module_inputs_match(
	size_t argument_count,
	const e_task_function_mapping_native_module_input_type (&native_module_inputs)[k_max_native_module_arguments],
	const e_task_function_mapping_native_module_input_type (&inputs_to_match)[k_max_native_module_arguments]);

const s_task_function_mapping *get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	e_task_function_mapping_native_module_input_type (&native_module_inputs)[k_max_native_module_arguments]);

class c_task_buffer_iterator {
public:
	c_task_buffer_iterator(c_task_graph_data_array data_array) {
		m_data_array = data_array;
		m_argument_index = static_cast<size_t>(-1);
		m_array_index = static_cast<size_t>(-1);
		next();
	}

	bool is_valid() const {
		return m_argument_index < m_data_array.get_count();
	}

	void next() {
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
				if (current_data.type == k_task_data_type_real_array_in) {
					current_array_count = current_data.data.real_array_in.get_count();
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
				switch (new_data.type) {
				case k_task_data_type_real_buffer_in:
				case k_task_data_type_real_buffer_out:
				case k_task_data_type_real_buffer_inout:
					advance = false;
					break;

				case k_task_data_type_real_constant_in:
					advance = true;
					break;

				case k_task_data_type_real_array_in:
					// Skip past 0-sized arrays
					advance = (new_data.data.real_array_in.get_count() == 0) ||
						new_data.data.real_array_in[m_array_index].is_constant;
					break;

				case k_task_data_type_bool_constant_in:
				case k_task_data_type_bool_array_in:
				case k_task_data_type_string_constant_in:
				case k_task_data_type_string_array_in:
					advance = true;
					break;

				default:
					advance = false;
					wl_unreachable();
				}
			}
		} while (advance);
	}

	uint32 get_buffer_index() const {
		uint32 buffer_index = c_task_graph::k_invalid_buffer;

		const s_task_graph_data &data = m_data_array[m_argument_index];
		switch (data.type) {
		case k_task_data_type_real_buffer_in:
			buffer_index = data.data.real_buffer_in;
			break;

		case k_task_data_type_real_buffer_out:
			buffer_index = data.data.real_buffer_out;
			break;

		case k_task_data_type_real_buffer_inout:
			buffer_index = data.data.real_buffer_inout;
			break;

		case k_task_data_type_real_constant_in:
			wl_unreachable();
			break;

		case k_task_data_type_real_array_in:
			wl_assert(!data.data.real_array_in[m_array_index].is_constant);
			buffer_index = data.data.real_array_in[m_array_index].buffer_index_value;
			break;

		case k_task_data_type_bool_constant_in:
		case k_task_data_type_bool_array_in:
		case k_task_data_type_string_constant_in:
		case k_task_data_type_string_array_in:
			wl_unreachable();
			break;

		default:
			wl_unreachable();
		}

		return buffer_index;
	}

private:
	c_task_graph_data_array m_data_array;
	size_t m_argument_index;
	size_t m_array_index;
};

c_task_graph::c_task_graph() {
	m_buffer_count = 0;
	m_max_task_concurrency = 0;
	m_max_buffer_concurrency = 0;
	m_initial_tasks_start = 0;
	m_initial_tasks_count = 0;
	m_outputs_start = 0;
	m_outputs_count = 0;

	ZERO_STRUCT(&m_globals);
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

c_task_graph_data_array c_task_graph::get_outputs() const {
	return c_task_graph_data_array(
		m_outputs_count == 0 ? nullptr : &m_data_lists[m_outputs_start],
		m_outputs_count);
}

uint32 c_task_graph::get_buffer_count() const {
	return m_buffer_count;
}

uint32 c_task_graph::get_max_buffer_concurrency() const {
	return m_max_buffer_concurrency;
}

uint32 c_task_graph::get_buffer_usages(uint32 buffer_index) const {
	return m_buffer_usages[buffer_index];
}

const s_task_graph_globals &c_task_graph::get_globals() const {
	return m_globals;
}

bool c_task_graph::build(const c_execution_graph &execution_graph) {
	m_tasks.clear();
	m_data_lists.clear();
	m_task_lists.clear();
	m_string_table.clear();
	m_real_array_element_lists.clear();
	m_buffer_count = 0;
	m_buffer_usages.clear();
	m_max_task_concurrency = 0;
	m_max_buffer_concurrency = 0;
	m_initial_tasks_start = 0;
	m_initial_tasks_count = 0;
	m_outputs_start = 0;
	m_outputs_count = 0;

	// Maps nodes in the execution graph to tasks in the task graph
	std::vector<uint32> nodes_to_tasks(execution_graph.get_node_count(), k_invalid_task);

	bool success = true;

	// Loop over each node and build up the graph
	for (uint32 index = 0; success && index < execution_graph.get_node_count(); index++) {
		// We should never encounter these node types, even if the graph was loaded from a file (they should never
		// appear in a file and if they do we should fail to load).
		wl_assert(execution_graph.get_node_type(index) != k_execution_graph_node_type_invalid);

		// We only care about native module calls - we access other node types through the call's edges
		if (execution_graph.get_node_type(index) == k_execution_graph_node_type_native_module_call) {
			success &= add_task_for_node(execution_graph, index, nodes_to_tasks);
		}
	}

	if (success) {
		resolve_arrays();
		resolve_strings();
		build_task_successor_lists(execution_graph, nodes_to_tasks);
		allocate_buffers(execution_graph);
		calculate_max_concurrency();
		calculate_buffer_usages();

		const s_execution_graph_globals &execution_graph_globals = execution_graph.get_globals();
		m_globals.max_voices = execution_graph_globals.max_voices;
	}

	if (!success) {
		m_tasks.clear();
		m_data_lists.clear();
		m_task_lists.clear();
		m_string_table.clear();
		m_real_array_element_lists.clear();
		m_buffer_count = 0;
		m_buffer_usages.clear();
		m_max_task_concurrency = 0;
		m_max_buffer_concurrency = 0;
		m_initial_tasks_start = 0;
		m_initial_tasks_count = 0;
		m_outputs_start = 0;
		m_outputs_count = 0;
	}

	return success;
}

static void get_native_module_call_input_types(const c_execution_graph &execution_graph, uint32 node_index,
	e_task_function_mapping_native_module_input_type (&out_input_types)[k_max_native_module_arguments]) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	const s_native_module &native_module = c_native_module_registry::get_native_module(
		execution_graph.get_native_module_call_native_module_index(node_index));

	size_t input_index = 0;
	for (size_t index = 0; index < native_module.argument_count; index++) {
		if (native_module.arguments[index].qualifier == k_native_module_argument_qualifier_out) {
			// Skip outputs
			out_input_types[index] = k_task_function_mapping_native_module_input_type_none;
		} else {
			wl_assert(native_module.arguments[index].qualifier == k_native_module_argument_qualifier_in ||
				native_module.arguments[index].qualifier == k_native_module_argument_qualifier_constant);

			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, input_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);

			if (execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_constant) {
				// The input is a constant
				out_input_types[index] = k_task_function_mapping_native_module_input_type_constant;
			} else {
				// The input is the result of a module call
				wl_assert(execution_graph.get_node_type(source_node_index) ==
					k_execution_graph_node_type_indexed_output);

				bool branches = does_native_module_call_input_branch(execution_graph, node_index, input_index);
				out_input_types[index] = branches ?
					k_task_function_mapping_native_module_input_type_variable :
					k_task_function_mapping_native_module_input_type_branchless_variable;
			}

			input_index++;
		}
	}
}

static bool does_native_module_call_input_branch(const c_execution_graph &execution_graph, uint32 node_index,
	size_t in_arg_index) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, in_arg_index);
	uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);

	if (execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_constant) {
		// We don't care if a constant value is branched because constant values don't take up buffers, they are
		// directly inlined into the task.
		// Note: arrays do use buffers, but we don't perform branch optimization on them
		return false;
	} else {
		wl_assert(execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_indexed_output);
		return (execution_graph.get_node_outgoing_edge_count(source_node_index) > 1);
	}
}

static bool do_native_module_inputs_match(
	size_t argument_count,
	const e_task_function_mapping_native_module_input_type (&native_module_inputs)[k_max_native_module_arguments],
	const e_task_function_mapping_native_module_input_type (&inputs_to_match)[k_max_native_module_arguments]) {
	for (size_t arg = 0; arg < argument_count; arg++) {
		e_task_function_mapping_native_module_input_type native_module_input = native_module_inputs[arg];
		e_task_function_mapping_native_module_input_type input_to_match = inputs_to_match[arg];
		wl_assert(VALID_INDEX(native_module_input, k_task_function_mapping_native_module_input_type_count));
		wl_assert(VALID_INDEX(input_to_match, k_task_function_mapping_native_module_input_type_count));

		// "None" inputs should always match
		wl_assert((native_module_input == k_task_function_mapping_native_module_input_type_none) ==
			(input_to_match == k_task_function_mapping_native_module_input_type_none));

		if (input_to_match == k_task_function_mapping_native_module_input_type_constant) {
			// We expect a constant as input
			if (native_module_input != k_task_function_mapping_native_module_input_type_constant) {
				return false;
			}
		} else if (input_to_match == k_task_function_mapping_native_module_input_type_variable) {
			// We expect a variable, either branching or branchless
			if (native_module_input != k_task_function_mapping_native_module_input_type_variable &&
				native_module_input != k_task_function_mapping_native_module_input_type_branchless_variable) {
				return false;
			}
		} else if (input_to_match == k_task_function_mapping_native_module_input_type_branchless_variable) {
			// We expect a branchless variable
			if (native_module_input != k_task_function_mapping_native_module_input_type_branchless_variable) {
				return false;
			}
		} else {
			wl_assert(input_to_match == k_task_function_mapping_native_module_input_type_none);
		}
	}

	return true;
}

static const s_task_function_mapping *get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	e_task_function_mapping_native_module_input_type (&native_module_inputs)[k_max_native_module_arguments]) {
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);
	c_task_function_mapping_list task_function_mapping_list =
		c_task_function_registry::get_task_function_mapping_list(native_module_index);

	// Go through the mappings in order and return the first one that matches
	for (size_t index = 0; index < task_function_mapping_list.get_count(); index++) {
		const s_task_function_mapping &mapping = task_function_mapping_list[index];
		bool match = do_native_module_inputs_match(
			native_module.argument_count, native_module_inputs, mapping.native_module_input_types);

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

			switch (argument.type) {
			case k_task_data_type_real_buffer_in:
			case k_task_data_type_real_buffer_out:
			case k_task_data_type_real_buffer_inout:
			case k_task_data_type_real_constant_in:
				break;

			case k_task_data_type_real_array_in:
			{
				// We previously stored the offset in the pointer
				c_real_array real_array = argument.data.real_array_in;
				argument.data.real_array_in = c_real_array(
					real_array_base_pointer + reinterpret_cast<size_t>(real_array.get_pointer()),
					real_array.get_count());
				break;
			}

			case k_task_data_type_bool_constant_in:
				break;

			case k_task_data_type_bool_array_in:
			{
				// We previously stored the offset in the pointer
				c_bool_array bool_array = argument.data.bool_array_in;
				argument.data.bool_array_in = c_bool_array(
					bool_array_base_pointer + reinterpret_cast<size_t>(bool_array.get_pointer()),
					bool_array.get_count());
				break;
			}

			case k_task_data_type_string_constant_in:
				break;

			case k_task_data_type_string_array_in:
			{
				// We previously stored the offset in the pointer
				c_string_array string_array = argument.data.string_array_in;
				argument.data.string_array_in = c_string_array(
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

void c_task_graph::resolve_strings() {
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			if (argument.type == k_task_data_type_string_constant_in) {
				// We previously stored the offset in the pointer
				argument.data.string_constant_in = m_string_table.get_string(
					reinterpret_cast<size_t>(argument.data.string_constant_in));
			} else if (argument.type == k_task_data_type_string_array_in) {
				for (size_t index = 0; index < argument.data.string_array_in.get_count(); index++) {
					// Access the element in a non-const way
					size_t element_index =
						&argument.data.string_array_in[index] - &m_string_array_element_lists.front();
					s_string_array_element &element = m_string_array_element_lists[element_index];
					element.constant_value = m_string_table.get_string(
						reinterpret_cast<size_t>(element.constant_value));
				}
			}
		}
	}
}

bool c_task_graph::add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index,
	std::vector<uint32> &nodes_to_tasks) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	uint32 task_index = cast_integer_verify<uint32>(m_tasks.size());
	m_tasks.push_back(s_task());

	wl_assert(nodes_to_tasks[node_index] == k_invalid_task);
	nodes_to_tasks[node_index] = task_index;

	// Construct string representing inputs - 1 extra character for null terminator
	e_task_function_mapping_native_module_input_type input_types[k_max_native_module_arguments];
	get_native_module_call_input_types(execution_graph, node_index, input_types);

	uint32 native_module_index = execution_graph.get_native_module_call_native_module_index(node_index);

	const s_task_function_mapping *mapping =
		get_task_mapping_for_native_module_and_inputs(native_module_index, input_types);

	if (!mapping) {
		return false;
	} else {
		setup_task(execution_graph, node_index, task_index, *mapping);
		return true;
	}
}

void c_task_graph::setup_task(const c_execution_graph &execution_graph, uint32 node_index,
	uint32 task_index, const s_task_function_mapping &task_function_mapping) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

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

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_data_lists.size(); index++) {
		// Fill with 0xffffffff so we can assert that we don't set a value twice
		m_data_lists[index].type = k_task_data_type_count;
		m_data_lists[index].data.execution_graph_index_a = c_execution_graph::k_invalid_index;
		m_data_lists[index].data.execution_graph_index_b = c_execution_graph::k_invalid_index;
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Map each input and output to its defined location in the task
	size_t input_output_count =
		execution_graph.get_node_incoming_edge_count(node_index) +
		execution_graph.get_node_outgoing_edge_count(node_index);

	size_t input_index = 0;
	size_t output_index = 0;
	for (size_t index = 0; index < input_output_count; index++) {
		// Determine if this is an input or output from the task function mapping input type
		wl_assert(VALID_INDEX(task_function_mapping.native_module_input_types[index],
			k_task_function_mapping_native_module_input_type_count));

		bool is_input = task_function_mapping.native_module_input_types[index] !=
			k_task_function_mapping_native_module_input_type_none;

		if (is_input) {
			uint32 argument_index = task_function_mapping.native_module_argument_mapping.mapping[index];
			wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
			s_task_graph_data &argument_data = m_data_lists[task.arguments_start + argument_index];
			argument_data.type = task_function.argument_types[argument_index];

			// Obtain the source input node
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, input_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);

			switch (argument_data.type) {
			case k_task_data_type_real_buffer_in:
				// Verify that this is a buffer input
				wl_assert(execution_graph.get_node_type(source_node_index) ==
					k_execution_graph_node_type_indexed_output);
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				// Store the input node index for now
				argument_data.data.execution_graph_index_a = input_node_index;
				break;

			case k_task_data_type_real_buffer_out:
				wl_vhalt("Can't map inputs to outputs");
				break;

			case k_task_data_type_real_buffer_inout:
				// Verify that this is a buffer input
				wl_assert(execution_graph.get_node_type(source_node_index) ==
					k_execution_graph_node_type_indexed_output);
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				// Store the input node index for now
				argument_data.data.execution_graph_index_a = input_node_index;
				break;

			case k_task_data_type_real_constant_in:
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				argument_data.data.real_constant_in = execution_graph.get_constant_node_real_value(source_node_index);
				break;

			case k_task_data_type_real_array_in:
				argument_data.data.real_array_in = build_real_array(execution_graph, source_node_index);
				break;

			case k_task_data_type_bool_constant_in:
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				argument_data.data.bool_constant_in = execution_graph.get_constant_node_bool_value(source_node_index);
				break;

			case k_task_data_type_bool_array_in:
				argument_data.data.bool_array_in = build_bool_array(execution_graph, source_node_index);
				break;

			case k_task_data_type_string_constant_in:
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				// add_string() returns an offset, so temporarily store this in the pointer. This is because as we add more
				// strings, the string table may be resized and pointers would be invalidated, so instead we store offsets
				// and resolve them to pointers at the end.
				argument_data.data.string_constant_in = reinterpret_cast<const char *>(
					m_string_table.add_string(execution_graph.get_constant_node_string_value(source_node_index)));
				break;

			case k_task_data_type_string_array_in:
				argument_data.data.string_array_in = build_string_array(execution_graph, source_node_index);
				break;

			default:
				wl_unreachable();
			}

			input_index++;
		} else {
			uint32 argument_index = task_function_mapping.native_module_argument_mapping.mapping[index];
			wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
			s_task_graph_data &argument_data = m_data_lists[task.arguments_start + argument_index];
			argument_data.type = task_function.argument_types[argument_index];

			// Obtain the output node
			uint32 output_node_index = execution_graph.get_node_outgoing_edge_index(node_index, output_index);

			switch (argument_data.type) {
			case k_task_data_type_real_buffer_in:
				wl_vhalt("Can't map outputs to inputs");
				break;

			case k_task_data_type_real_buffer_out:
				wl_assert(argument_data.data.execution_graph_index_a == c_execution_graph::k_invalid_index);
				// Store the output node index for now
				argument_data.data.execution_graph_index_a = output_node_index;
				break;

			case k_task_data_type_real_buffer_inout:
				wl_assert(argument_data.data.execution_graph_index_b == c_execution_graph::k_invalid_index);
				// Store the output node index for now
				argument_data.data.execution_graph_index_b = output_node_index;
				break;

			case k_task_data_type_real_constant_in:
			case k_task_data_type_real_array_in:
			case k_task_data_type_bool_constant_in:
			case k_task_data_type_bool_array_in:
			case k_task_data_type_string_constant_in:
			case k_task_data_type_string_array_in:
				wl_vhalt("Can't map outputs to inputs");
				break;

			default:
				wl_unreachable();
			}

			output_index++;
		}
	}

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_data_lists.size(); index++) {
		// Make sure we have filled in all values
		wl_assert(m_data_lists[index].type != k_task_data_type_count);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)
}

c_real_array c_task_graph::build_real_array(const c_execution_graph &execution_graph, uint32 node_index) {
	// Build a list of array elements using the array node at node_index. Note that at this point, we're actually
	// storing the node indices in the element buffer_index_value field - we will later convert these to buffer indices.

	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_constant);
	wl_assert(execution_graph.get_constant_node_data_type(node_index) == k_native_module_argument_type_real_array);

	size_t array_count = execution_graph.get_node_incoming_edge_count(node_index);

	// First scan the list of elements to see if this array has already been instantiated
	bool match = false;
	size_t start_offset = static_cast<size_t>(-1);
	for (size_t start_index = 0;
		 match && (start_index + array_count < m_real_array_element_lists.size());
		 start_index++) {
		// Check if the next N elements match starting at start_index
		bool match_inner = true;
		for (size_t array_index = 0; match_inner && array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			bool source_node_is_constant =
				(execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_constant);

			const s_real_array_element &element = m_real_array_element_lists[start_index + array_index];

			if (element.is_constant && source_node_is_constant) {
				match_inner =
					(element.constant_value == execution_graph.get_constant_node_real_value(source_node_index));
			} else if (!element.is_constant && !source_node_is_constant) {
				// Recall that this is currently storing the node index
				match_inner = (element.buffer_index_value == source_node_index);
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
		start_offset = m_real_array_element_lists.size();

		for (size_t array_index = 0; array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			bool source_node_is_constant =
				(execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_constant);

			m_real_array_element_lists.push_back(s_real_array_element());
			s_real_array_element &element = m_real_array_element_lists.back();
			element.is_constant = source_node_is_constant;

			if (source_node_is_constant) {
				element.constant_value = execution_graph.get_constant_node_real_value(source_node_index);
			} else {
				// Store the node index for now
				element.buffer_index_value = source_node_index;
			}
		}
	}

	// We store the offset in the pointer, which we will later convert
	const s_real_array_element *start_offset_pointer = reinterpret_cast<const s_real_array_element *>(start_offset);
	return c_real_array(start_offset_pointer, array_count);
}

c_bool_array c_task_graph::build_bool_array(const c_execution_graph &execution_graph, uint32 node_index) {
	// Build a list of array elements using the array node at node_index. Note that at this point, we're actually
	// storing the node indices in the element buffer_index_value field - we will later convert these to buffer indices.

	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_constant);
	wl_assert(execution_graph.get_constant_node_data_type(node_index) == k_native_module_argument_type_bool_array);

	size_t array_count = execution_graph.get_node_incoming_edge_count(node_index);

	// First scan the list of elements to see if this array has already been instantiated
	bool match = false;
	size_t start_offset = static_cast<size_t>(-1);
	for (size_t start_index = 0;
		 match && (start_index + array_count < m_bool_array_element_lists.size());
		 start_index++) {
		// Check if the next N elements match starting at start_index
		bool match_inner = true;
		for (size_t array_index = 0; match_inner && array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			const s_bool_array_element &element = m_bool_array_element_lists[start_index + array_index];

			match_inner = (element.constant_value == execution_graph.get_constant_node_bool_value(source_node_index));
		}

		if (match_inner) {
			match = true;
			start_offset = start_index;
		}
	}

	if (!match) {
		// If we didn't match, we need to construct elements
		start_offset = m_bool_array_element_lists.size();

		for (size_t array_index = 0; array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			m_bool_array_element_lists.push_back(s_bool_array_element());
			s_bool_array_element &element = m_bool_array_element_lists.back();

			element.constant_value = execution_graph.get_constant_node_bool_value(source_node_index);
		}
	}

	// We store the offset in the pointer, which we will later convert
	const s_bool_array_element *start_offset_pointer = reinterpret_cast<const s_bool_array_element *>(start_offset);
	return c_bool_array(start_offset_pointer, array_count);
}

c_string_array c_task_graph::build_string_array(const c_execution_graph &execution_graph, uint32 node_index) {
	// Build a list of array elements using the array node at node_index. Note that at this point, we're actually
	// storing the node indices in the element buffer_index_value field - we will later convert these to buffer indices.

	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_constant);
	wl_assert(execution_graph.get_constant_node_data_type(node_index) == k_native_module_argument_type_string_array);

	size_t array_count = execution_graph.get_node_incoming_edge_count(node_index);

	// First scan the list of elements to see if this array has already been instantiated
	bool match = false;
	size_t start_offset = static_cast<size_t>(-1);
	for (size_t start_index = 0;
		 match && (start_index + array_count < m_string_array_element_lists.size());
		 start_index++) {
		// Check if the next N elements match starting at start_index
		bool match_inner = true;
		for (size_t array_index = 0; match_inner && array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			const s_string_array_element &element = m_string_array_element_lists[start_index + array_index];

			match_inner =
				strcmp(element.constant_value, execution_graph.get_constant_node_string_value(source_node_index)) == 0;
		}

		if (match_inner) {
			match = true;
			start_offset = start_index;
		}
	}

	if (!match) {
		// If we didn't match, we need to construct elements
		start_offset = m_string_array_element_lists.size();

		for (size_t array_index = 0; array_index < array_count; array_index++) {
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, array_index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			m_string_array_element_lists.push_back(s_string_array_element());
			s_string_array_element &element = m_string_array_element_lists.back();

			element.constant_value = reinterpret_cast<const char *>(
				m_string_table.add_string(execution_graph.get_constant_node_string_value(source_node_index)));
		}
	}

	// We store the offset in the pointer, which we will later convert
	const s_string_array_element *start_offset_pointer = reinterpret_cast<const s_string_array_element *>(start_offset);
	return c_string_array(start_offset_pointer, array_count);
}

bool c_task_graph::is_real_array_constant(c_real_array real_array) const {
	for (size_t index = 0; index < real_array.get_count(); index++) {
		if (!real_array[index].is_constant) {
			return false;
		}
	}

	return true;
}

void c_task_graph::build_task_successor_lists(const c_execution_graph &execution_graph,
	const std::vector<uint32> &nodes_to_tasks) {
	// Clear predecessor counts first since those are set in a random order
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		m_tasks[task_index].predecessor_count = 0;
	}

	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (nodes_to_tasks[node_index] == k_invalid_task) {
			// This node is not a task
			wl_assert(execution_graph.get_node_type(node_index) != k_execution_graph_node_type_native_module_call);
			continue;
		}

		wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);
		uint32 task_index = nodes_to_tasks[node_index];
		s_task &task = m_tasks[task_index];

		task.successors_start = m_task_lists.size();
		task.successors_count = 0;
		for (size_t edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_index); edge++) {
			uint32 output_node_index = execution_graph.get_node_outgoing_edge_index(node_index, edge);

			for (size_t dest = 0; dest < execution_graph.get_node_outgoing_edge_count(output_node_index); dest++) {
				uint32 dest_node_index = execution_graph.get_node_outgoing_edge_index(output_node_index, dest);

				// We only care about native module inputs
				if (execution_graph.get_node_type(dest_node_index) != k_execution_graph_node_type_indexed_input) {
					continue;
				}

				uint32 input_native_module_call_node_index =
					execution_graph.get_node_outgoing_edge_index(dest_node_index, 0);

				// Skip arrays
				if (execution_graph.get_node_type(input_native_module_call_node_index) !=
					k_execution_graph_node_type_native_module_call) {
					continue;
				}

				// Find the task belonging to this node
				uint32 successor_task_index = nodes_to_tasks[input_native_module_call_node_index];
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

	// Find all initial tasks - tasks which have no buffer inputs
	m_initial_tasks_start = m_task_lists.size();
	m_initial_tasks_count = 0;
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		bool any_input_buffers = false;
		for (size_t arg = 0; !any_input_buffers && arg < task_function.argument_count; arg++) {
			const s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];
			e_task_data_type arg_type = task_function.argument_types[arg];

			any_input_buffers =
				(arg_type == k_task_data_type_real_buffer_in) ||
				(arg_type == k_task_data_type_real_buffer_inout) ||
				(arg_type == k_task_data_type_real_array_in && !is_real_array_constant(argument.data.real_array_in));
		}

		if (!any_input_buffers) {
			m_initial_tasks_count++;
			m_task_lists.push_back(task_index);
		}
	}
}

void c_task_graph::allocate_buffers(const c_execution_graph &execution_graph) {
	// Associates input and output nodes for when inout buffers have been chosen
	std::vector<uint32> inout_connections(execution_graph.get_node_count(), c_execution_graph::k_invalid_index);

	// Maps input/output nodes to allocated buffers
	std::vector<uint32> nodes_to_buffers(execution_graph.get_node_count(), k_invalid_buffer);

	// Associate nodes which share an inout buffer
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			const s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			if (argument.type == k_task_data_type_real_buffer_inout) {
				inout_connections[argument.data.execution_graph_index_a] = argument.data.execution_graph_index_b;
				inout_connections[argument.data.execution_graph_index_b] = argument.data.execution_graph_index_a;
			}
		}
	}

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			// For each buffer, find all attached nodes and create a mapping
			switch (argument.type) {
			case k_task_data_type_real_buffer_in:
			case k_task_data_type_real_buffer_out:
			case k_task_data_type_real_buffer_inout:
				if (argument.type == k_task_data_type_real_buffer_inout) {
					// Inout buffers should have both nodes filled in
					wl_assert(argument.data.execution_graph_index_b != c_execution_graph::k_invalid_index);
					// This mapping should always be equal because this is an inout buffer - there is only one buffer
					wl_assert(nodes_to_buffers[argument.data.execution_graph_index_a] ==
						nodes_to_buffers[argument.data.execution_graph_index_b]);
				} else {
					wl_assert(argument.data.execution_graph_index_b == c_execution_graph::k_invalid_index);
				}

				if (nodes_to_buffers[argument.data.execution_graph_index_a] != k_invalid_buffer) {
					// Already allocated, skip this one
				} else {
					assign_buffer_to_related_nodes(execution_graph, argument.data.execution_graph_index_a,
						inout_connections, nodes_to_buffers, m_buffer_count);
					m_buffer_count++;
				}

				break;

			case k_task_data_type_real_constant_in:
				// Nothing to do
				break;

			case k_task_data_type_real_array_in:
				// Allocate a buffer for each array element
				for (size_t index = 0; index < argument.data.real_array_in.get_count(); index++) {
					const s_real_array_element &element = argument.data.real_array_in[index];
					if (!element.is_constant) {
						if (nodes_to_buffers[element.buffer_index_value] != k_invalid_buffer) {
							// Already allocated, skip this one
						} else {
							assign_buffer_to_related_nodes(execution_graph, element.buffer_index_value,
								inout_connections, nodes_to_buffers, m_buffer_count);
							m_buffer_count++;
						}
					}
				}
				break;

			case k_task_data_type_bool_constant_in:
			case k_task_data_type_bool_array_in:
			case k_task_data_type_string_constant_in:
			case k_task_data_type_string_array_in:
				// Nothing to do
				break;

			default:
				wl_unreachable();
			}
		}
	}

	// Finally, follow the node-to-buffer mapping
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

		for (size_t arg = 0; arg < task_function.argument_count; arg++) {
			s_task_graph_data &argument = m_data_lists[task.arguments_start + arg];

			// For each buffer, find all attached nodes and create a mapping
			switch (argument.type) {
			case k_task_data_type_real_buffer_in:
				argument.data.real_buffer_in = nodes_to_buffers[argument.data.execution_graph_index_a];
				wl_assert(argument.data.real_buffer_in != k_invalid_buffer);
				break;

			case k_task_data_type_real_buffer_out:
				argument.data.real_buffer_out = nodes_to_buffers[argument.data.execution_graph_index_a];
				wl_assert(argument.data.real_buffer_out != k_invalid_buffer);
				break;

			case k_task_data_type_real_buffer_inout:
				wl_assert(nodes_to_buffers[argument.data.execution_graph_index_a] ==
					nodes_to_buffers[argument.data.execution_graph_index_b]);
				argument.data.real_buffer_inout = nodes_to_buffers[argument.data.execution_graph_index_a];
				wl_assert(argument.data.real_buffer_inout != k_invalid_buffer);
				break;

			case k_task_data_type_real_constant_in:
				break;

			case k_task_data_type_real_array_in:
				for (size_t index = 0; index < argument.data.real_array_in.get_count(); index++) {
					// Access the element in a non-const way
					size_t element_index = &argument.data.real_array_in[index] - &m_real_array_element_lists.front();
					s_real_array_element &element = m_real_array_element_lists[element_index];
					if (!element.is_constant) {
						element.buffer_index_value = nodes_to_buffers[element.buffer_index_value];
						wl_assert(element.buffer_index_value != k_invalid_buffer);
					}
				}
				break;

			case k_task_data_type_bool_constant_in:
			case k_task_data_type_bool_array_in:
			case k_task_data_type_string_constant_in:
			case k_task_data_type_string_array_in:
				// Nothing to do
				break;

			default:
				wl_unreachable();
			}
		}
	}

	// Build final output buffer list. The buffers are listed in output-index order
	m_outputs_count = 0;
	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (execution_graph.get_node_type(node_index) == k_execution_graph_node_type_output) {
			m_outputs_count++;
		}
	}

	m_outputs_start = m_data_lists.size();
	m_data_lists.resize(m_data_lists.size() + m_outputs_count);

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = m_outputs_start; index < m_data_lists.size(); index++) {
		m_data_lists[index].type = k_task_data_type_count;
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (execution_graph.get_node_type(node_index) == k_execution_graph_node_type_output) {
			uint32 output_index = execution_graph.get_output_node_output_index(node_index);

			// Offset into the list by the output index
			s_task_graph_data &output_data = m_data_lists[m_outputs_start + output_index];
			wl_assert(output_data.type == k_task_data_type_count);

			if (nodes_to_buffers[node_index] != k_invalid_buffer) {
				output_data.type = k_task_data_type_real_buffer_in;
				output_data.data.real_buffer_in = nodes_to_buffers[node_index];
			} else {
				// This output has a constant piped directly into it
				uint32 constant_node_index = execution_graph.get_node_incoming_edge_index(node_index, 0);
				wl_assert(execution_graph.get_node_type(constant_node_index) == k_execution_graph_node_type_constant);
				output_data.type = k_task_data_type_real_constant_in;
				output_data.data.real_constant_in = execution_graph.get_constant_node_real_value(constant_node_index);
			}
		}
	}

#if PREDEFINED(ASSERTS_ENABLED)
	// Verify that we assigned all outputs
	for (size_t index = m_outputs_start; index < m_data_lists.size(); index++) {
		wl_assert(m_data_lists[index].type != k_task_data_type_count);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)
}

void c_task_graph::assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
	const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index) {
	if (nodes_to_buffers[node_index] != k_invalid_buffer) {
		// Already assigned, don't recurse forever
		wl_assert(nodes_to_buffers[node_index] == buffer_index);
		return;
	}

	nodes_to_buffers[node_index] = buffer_index;

	e_execution_graph_node_type node_type = execution_graph.get_node_type(node_index);
	if (node_type == k_execution_graph_node_type_indexed_input) {
		// Recurse on all connected outputs
		for (uint32 edge = 0; edge < execution_graph.get_node_incoming_edge_count(node_index); edge++) {
			uint32 edge_node_index = execution_graph.get_node_incoming_edge_index(node_index, edge);

			// Skip constants (including arrays)
			if (execution_graph.get_node_type(edge_node_index) == k_execution_graph_node_type_constant) {
				continue;
			}

			// The only other option is native module outputs, which will be asserted on the recursive call
			assign_buffer_to_related_nodes(execution_graph, edge_node_index,
				inout_connections, nodes_to_buffers, buffer_index);
		}
	} else if (node_type == k_execution_graph_node_type_indexed_output) {
		// Recurse on all connected inputs
		for (uint32 edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_index); edge++) {
			uint32 edge_node_index = execution_graph.get_node_outgoing_edge_index(node_index, edge);

			// This call will either be performed on indexed inputs or output nodes, which will be asserted on the
			// recursive call
			assign_buffer_to_related_nodes(execution_graph, edge_node_index,
				inout_connections, nodes_to_buffers, buffer_index);
		}
	} else if (node_type == k_execution_graph_node_type_output) {
		// No recursive work to do here
	} else {
		// We should never hit any other node types
		wl_unreachable();
	}

	if (inout_connections[node_index] != c_execution_graph::k_invalid_index) {
		// This node has been assigned to an inout buffer, so also run on the corresponding input or output
		assign_buffer_to_related_nodes(execution_graph, inout_connections[node_index],
			inout_connections, nodes_to_buffers, buffer_index);
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

		m_max_task_concurrency = estimate_max_concurrency(cast_integer_verify<uint32>(m_tasks.size()), task_concurrency);
	}

	// 2) Determine which buffers are concurrent. This means that it is possible or necessary for the buffers to exist
	// at the same time.

	// To determine whether two tasks (a,b) can be parallel, we just ask whether (a !precedes b) and (b !precedes a).
	// The predecessor resolver provides this data.

	// The information we actually want to know is whether we can merge buffers, so we actually need to know which
	// buffers can exist in parallel. We can convert task concurrency data to buffer concurrency data.
	std::vector<bool> buffer_concurrency(m_buffer_count * m_buffer_count, false);

	// For any task, all buffer pairs are concurrent
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		for (c_task_buffer_iterator it_a(get_task_arguments(task_index)); it_a.is_valid(); it_a.next()) {
			uint32 buffer_a = it_a.get_buffer_index();
			for (c_task_buffer_iterator it_b(get_task_arguments(task_index)); it_b.is_valid(); it_b.next()) {
				uint32 buffer_b = it_b.get_buffer_index();
				buffer_concurrency[buffer_a * m_buffer_count + buffer_b] = true;
				buffer_concurrency[buffer_b * m_buffer_count + buffer_a] = true;
			}
		}
	}

	// For any pair of concurrent tasks, all buffer pairs between the two tasks are concurrent
	for (uint32 task_a_index = 0; task_a_index < m_tasks.size(); task_a_index++) {
		for (uint32 task_b_index = task_a_index + 1; task_b_index < m_tasks.size(); task_b_index++) {
			if (!predecessor_resolver.are_a_b_concurrent(task_a_index, task_b_index)) {
				continue;
			}

			for (c_task_buffer_iterator it_a(get_task_arguments(task_a_index)); it_a.is_valid(); it_a.next()) {
				uint32 buffer_a = it_a.get_buffer_index();
				for (c_task_buffer_iterator it_b(get_task_arguments(task_b_index)); it_b.is_valid(); it_b.next()) {
					uint32 buffer_b = it_b.get_buffer_index();
					buffer_concurrency[buffer_a * m_buffer_count + buffer_b] = true;
					buffer_concurrency[buffer_b * m_buffer_count + buffer_a] = true;
				}
			}
		}
	}

	// All output buffers are concurrent
	for (size_t output_a = 0; output_a < m_outputs_count; output_a++) {
		const s_task_graph_data &output_a_data = m_data_lists[m_outputs_start + output_a];
		c_task_buffer_iterator it_a(c_task_graph_data_array(&output_a_data, 1));
		if (!it_a.is_valid()) {
			continue;
		}

		uint32 output_buffer_a_index = it_a.get_buffer_index();

		for (size_t output_b = output_a + 1; output_b < m_outputs_count; output_b++) {
			const s_task_graph_data &output_b_data = m_data_lists[m_outputs_start + output_b];
			c_task_buffer_iterator it_b(c_task_graph_data_array(&output_b_data, 1));
			if (!it_b.is_valid()) {
				continue;
			}

			uint32 output_buffer_b_index = it_b.get_buffer_index();

			buffer_concurrency[output_buffer_a_index * m_buffer_count + output_buffer_b_index] = true;
			buffer_concurrency[output_buffer_b_index * m_buffer_count + output_buffer_a_index] = true;
		}
	}

	// 3) Merge buffers which are not concurrent. To merge buffer x with buffers S=(a,b,c,...), buffer x must not be
	// concurrent with any buffers in S.
	m_max_buffer_concurrency = estimate_max_concurrency(m_buffer_count, buffer_concurrency);
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
			m_buffer_usages[it.get_buffer_index()]++;
		}
	}

	// Each output buffer contributes to usage to prevent the buffer from deallocating at the end of the last task
	for (size_t index = 0; index < m_outputs_count; index++) {
		const s_task_graph_data &output = m_data_lists[m_outputs_start + index];
		c_task_buffer_iterator it(c_task_graph_data_array(&output, 1));
		if (!it.is_valid()) {
			continue;
		}

		m_buffer_usages[it.get_buffer_index()]++;
	}
}
