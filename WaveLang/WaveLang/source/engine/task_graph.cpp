#include "engine/task_graph.h"
#include "engine/predecessor_resolver.h"
#include "execution_graph/native_modules.h"
#include "execution_graph/execution_graph.h"

static const uint32 k_no_task = static_cast<uint32>(-1);
static const uint32 k_no_buffer = static_cast<uint32>(-1);

// Specify a string like the following: "cvv". The character count is the number of arguments. Each 'c' indicates a
// constant input, each 'v' indicates a non-constant input. Returns true if the module call inputs match this pattern.
static bool do_native_module_call_inputs_match(const c_execution_graph &execution_graph, uint32 node_index,
	const char *in_args);

// Returns true if the given input is used as more than once. This indicates that the input cannot be used as an inout
// because overwriting the buffer would invalidate its second usage as an input.
static bool does_native_module_call_input_branch(const c_execution_graph &execution_graph, uint32 node_index,
	size_t in_arg_index);

c_task_graph::c_task_graph() {
	m_buffer_count = 0;
	m_max_buffer_concurrency = 0;
	m_output_buffers_start = 0;
	m_output_buffers_count = 0;
}

c_task_graph::~c_task_graph() {
}

bool c_task_graph::build(const c_execution_graph &execution_graph) {
	m_tasks.clear();
	m_constant_lists.clear();
	m_buffer_lists.clear();
	m_task_lists.clear();
	m_buffer_count = 0;
	m_max_buffer_concurrency = 0;
	m_output_buffers_start = 0;
	m_output_buffers_count = 0;

	// Maps nodes in the execution graph to tasks in the task graph
	std::vector<uint32> nodes_to_tasks(execution_graph.get_node_count(), k_no_task);

	bool success = true;

	// Loop over each node and build up the graph
	for (uint32 index = 0; success && index < execution_graph.get_node_count(); index++) {
		// We should never encounter these node types, even if the graph was loaded from a file (they should never
		// appear in a file and if they do we should fail to load).
		wl_assert(execution_graph.get_node_type(index) != k_execution_graph_node_type_invalid);
		wl_assert(execution_graph.get_node_type(index) != k_execution_graph_node_type_intermediate_value);

		// We only care about native module calls - we access other node types through the call's edges
		if (execution_graph.get_node_type(index) == k_execution_graph_node_type_native_module_call) {
			success &= add_task_for_node(execution_graph, index, nodes_to_tasks);
		}
	}

	if (success) {
		build_task_successor_lists(execution_graph, nodes_to_tasks);
		allocate_buffers(execution_graph);
		calculate_max_buffer_concurrency();
	}

	if (!success) {
		m_tasks.clear();
		m_constant_lists.clear();
		m_buffer_lists.clear();
		m_task_lists.clear();
		m_buffer_count = 0;
		m_max_buffer_concurrency = 0;
		m_output_buffers_start = 0;
		m_output_buffers_count = 0;
	}

	return success;
}

static bool do_native_module_call_inputs_match(const c_execution_graph &execution_graph, uint32 node_index,
	const char *in_args) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);
	wl_assert(in_args != nullptr);

	size_t incoming_edge_count = execution_graph.get_node_incoming_edge_count(node_index);

#if PREDEFINED(ASSERTS_ENABLED)
	for (const char *ch = in_args; *ch != '\0'; ch++) {
		wl_assert(*ch == 'c' || *ch == 'v');
	}
#endif // PREDEFINED(ASSERTS_ENABLED)
	wl_assert(in_args[incoming_edge_count] == '\0');

	for (size_t index = 0; index < incoming_edge_count; index++) {
		uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, index);
		uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);

		if (in_args[index] == 'c') {
			if (execution_graph.get_node_type(source_node_index) != k_execution_graph_node_type_constant) {
				// The input is not a constant
				return false;
			}
		} else {
			wl_assert(in_args[index] == 'v');
			if (execution_graph.get_node_type(source_node_index) != k_execution_graph_node_type_native_module_output) {
				// The input is not the result of a module call, so it must be a constant
				return false;
			}
		}
	}

	// All inputs match
	return true;
}

static bool does_native_module_call_input_branch(const c_execution_graph &execution_graph, uint32 node_index,
	size_t in_arg_index) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, in_arg_index);
	uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);

	if (execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_constant) {
		// We don't care if a constant value is branched because constant values don't take up buffers, they are
		// directly inlined into the task.
		return true;
	} else {
		wl_assert(execution_graph.get_node_type(source_node_index) == k_execution_graph_node_type_native_module_output);
		return (execution_graph.get_node_outgoing_edge_count(source_node_index) == 1);
	}
}

// Shorthand
#define TM_C(index) s_task_mapping(k_task_mapping_location_constant_in, (index))
#define TM_I(index) s_task_mapping(k_task_mapping_location_buffer_in, (index))
#define TM_O(index) s_task_mapping(k_task_mapping_location_buffer_out, (index))
#define TM_IO(index) s_task_mapping(k_task_mapping_location_buffer_inout, (index))

// This is the function which maps native module calls in the execution graph to tasks in the task graph. A single
// native module can map to many different tasks - e.g. for performance reasons, we have different tasks for v + v and
// v + c, and for memory optimization (and performance) we also have tasks which directly modify buffers which never
// need to be reused.
bool c_task_graph::add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index,
	std::vector<uint32> &nodes_to_tasks) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	uint32 task_index = static_cast<uint32>(m_tasks.size());
	m_tasks.push_back(s_task());

	wl_assert(nodes_to_tasks[node_index] == k_no_task);
	nodes_to_tasks[node_index] = task_index;

	uint32 native_module_index = execution_graph.get_native_module_call_native_module_index(node_index);
	switch (native_module_index) {
	case k_native_module_noop:
		// We don't allow no-ops at this point
		wl_halt();
		return false;

	case k_native_module_negation:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "v")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_negation_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_negation_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_addition:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "vv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_I(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_addition_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_subtraction:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "vv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_I(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_subtraction_constant_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_multiplication:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "vv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_I(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_multiplication_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_division:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "vv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_I(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_division_constant_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_modulo:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "vv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_I(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_index,
					k_task_function_modulo_constant_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_test:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "")) {
			s_task_mapping mapping[] = { TM_O(0) };
			setup_task(execution_graph, node_index, task_index,
				k_task_function_test, c_task_mapping_array::construct(mapping));
			return true;
		} else {
			return false;
		}

	default:
		wl_halt();
		return false;
	}
}

void c_task_graph::setup_task(const c_execution_graph &execution_graph, uint32 node_index,
	uint32 task_index, e_task_function task_function, const c_task_mapping_array &task_mapping_array) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	s_task &task = m_tasks[task_index];
	const s_task_function_description &task_function_description =
		c_task_function_registry::get_task_function_description(task_function);

	// Set up the lists of task inputs/outputs
	// Initially, we store the execution graph nodes in this list
	// For the inout list, we need to store both input and output node index, so we double its size (for now)
	task.in_constants_start = m_constant_lists.size();
	task.in_buffers_start = m_buffer_lists.size();
	task.out_buffers_start = task.in_buffers_start + task_function_description.in_buffer_count;
	task.inout_buffers_start = task.out_buffers_start + task_function_description.out_buffer_count;

	IF_ASSERTS_ENABLED(size_t old_size = m_buffer_lists.size());
	m_constant_lists.resize(m_constant_lists.size() + task_function_description.in_constant_count);
	m_buffer_lists.resize(m_buffer_lists.size() +
		task_function_description.in_buffer_count +
		task_function_description.out_buffer_count +
		task_function_description.inout_buffer_count * 2); // Double its size to store both input and output nodes

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_buffer_lists.size(); index++) {
		// Fill with 0xffffffff so we can assert that we don't set a value twice
		m_buffer_lists[index] = c_execution_graph::k_invalid_index;
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	wl_assert(task_mapping_array.get_count() ==
		execution_graph.get_node_incoming_edge_count(node_index) +
		execution_graph.get_node_outgoing_edge_count(node_index));

	size_t task_mapping_index = 0;

	// Map each input to its defined location in the task
	for (size_t index = 0; index < execution_graph.get_node_incoming_edge_count(node_index); index++) {
		s_task_mapping task_mapping = task_mapping_array[task_mapping_index];

		if (task_mapping.location == k_task_mapping_location_constant_in) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.in_constant_count));

			// Obtain the constant node and value
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, index);
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			m_constant_lists[task.in_constants_start + task_mapping.index] =
				execution_graph.get_constant_node_value(source_node_index);
		} else if (task_mapping.location == k_task_mapping_location_buffer_in) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.in_buffer_count));

			// Obtain the input node
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, index);
#if PREDEFINED(ASSERTS_ENABLED)
			// Verify that this is a buffer input
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			wl_assert(execution_graph.get_node_type(source_node_index) ==
				k_execution_graph_node_type_native_module_output);
#endif // PREDEFINED(ASSERTS_ENABLED)

			// Store the input node index for now
			wl_assert(m_buffer_lists[task.in_buffers_start + task_mapping.index] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task.in_buffers_start + task_mapping.index] = input_node_index;
		} else if (task_mapping.location == k_task_mapping_location_buffer_inout) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.inout_buffer_count));

			// Obtain the input node
			uint32 input_node_index = execution_graph.get_node_incoming_edge_index(node_index, index);
#if PREDEFINED(ASSERTS_ENABLED)
			// Verify that this is a buffer input
			uint32 source_node_index = execution_graph.get_node_incoming_edge_index(input_node_index, 0);
			wl_assert(execution_graph.get_node_type(source_node_index) ==
				k_execution_graph_node_type_native_module_output);
#endif // PREDEFINED(ASSERTS_ENABLED)

			// Store the input node index for now - we're storing both input and output, so make space for both
			wl_assert(m_buffer_lists[task.inout_buffers_start + task_mapping.index * 2] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task.inout_buffers_start + task_mapping.index * 2] = input_node_index;
		} else {
			// Can't map inputs to outputs
			wl_halt();
		}

		task_mapping_index++;
	}

	// Map each output to its defined location in the task
	for (size_t index = 0; index < execution_graph.get_node_outgoing_edge_count(node_index); index++) {
		s_task_mapping task_mapping = task_mapping_array[task_mapping_index];

		if (task_mapping.location == k_task_mapping_location_buffer_out) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.out_buffer_count));

			// Obtain the output node
			uint32 output_node_index = execution_graph.get_node_outgoing_edge_index(node_index, index);

			// Store the output node index for now
			wl_assert(m_buffer_lists[task.out_buffers_start + task_mapping.index] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task.out_buffers_start + task_mapping.index] = output_node_index;
		} else if (task_mapping.location == k_task_mapping_location_buffer_inout) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.inout_buffer_count));

			// Obtain the output node
			uint32 output_node_index = execution_graph.get_node_outgoing_edge_index(node_index, index);

			// Store the output node index for now - we're storing both input and output, so make space for both
			wl_assert(m_buffer_lists[task.inout_buffers_start + task_mapping.index * 2 + 1] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task.inout_buffers_start + task_mapping.index * 2 + 1] = output_node_index;
		} else {
			// Can't map inputs to outputs
			wl_halt();
		}

		task_mapping_index++;
	}

	wl_assert(task_mapping_index == task_mapping_array.get_count());

#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t index = old_size; index < m_buffer_lists.size(); index++) {
		// Make sure we have filled in all values
		wl_assert(m_buffer_lists[index] != c_execution_graph::k_invalid_index);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)
}

void c_task_graph::build_task_successor_lists(const c_execution_graph &execution_graph,
	const std::vector<uint32> &nodes_to_tasks) {
	// Clear predecessor counts first since those are set in a random order
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		m_tasks[task_index].predecessor_count = 0;
	}

	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (nodes_to_tasks[node_index] == k_no_task) {
			// This node is not a task
			wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);
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
				if (execution_graph.get_node_type(dest_node_index) != k_execution_graph_node_type_native_module_input) {
					continue;
				}

				uint32 input_native_module_call_node_index =
					execution_graph.get_node_outgoing_edge_index(dest_node_index, 0);
				wl_assert(execution_graph.get_node_type(input_native_module_call_node_index) ==
					k_execution_graph_node_type_native_module_call);

				// Find the task belonging to this node
				uint32 successor_task_index = nodes_to_tasks[input_native_module_call_node_index];
				wl_assert(successor_task_index != k_no_task);

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
}

void c_task_graph::allocate_buffers(const c_execution_graph &execution_graph) {
	// Associates input and output nodes for when inout buffers have been chosen
	std::vector<uint32> inout_connections(execution_graph.get_node_count(), c_execution_graph::k_invalid_index);

	// Maps input/output nodes to allocated buffers
	std::vector<uint32> nodes_to_buffers(execution_graph.get_node_count(), k_no_buffer);

	size_t total_buffer_lists_size = 0;
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task.task_function);

		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			uint32 input_node_index = m_buffer_lists[task.inout_buffers_start + index * 2];
			uint32 output_node_index = m_buffer_lists[task.inout_buffers_start + index * 2 + 1];
			inout_connections[input_node_index] = output_node_index;
			inout_connections[output_node_index] = input_node_index;
		}

		total_buffer_lists_size += task_function_description.in_buffer_count;
		total_buffer_lists_size += task_function_description.out_buffer_count;
		total_buffer_lists_size += task_function_description.inout_buffer_count;
	}

	// We need a list of final output nodes
	m_output_buffers_count = 0;
	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (execution_graph.get_node_type(node_index) == k_execution_graph_node_type_output) {
			m_output_buffers_count++;
		}
	}

	total_buffer_lists_size += m_output_buffers_count;

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task.task_function);

		for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
			// For each in buffer, find all attached nodes and create a mapping
			uint32 input_node_index = m_buffer_lists[task.in_buffers_start + index];
			if (nodes_to_buffers[input_node_index] != k_no_buffer) {
				// Already allocated, skip this one
				continue;
			}

			assign_buffer_to_related_nodes(execution_graph, input_node_index, inout_connections, nodes_to_buffers,
				m_buffer_count);
		}

		for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
			// For each out buffer, find all attached nodes and create a mapping
			uint32 output_node_index = m_buffer_lists[task.out_buffers_start + index];
			if (nodes_to_buffers[output_node_index] != k_no_buffer) {
				// Already allocated, skip this one
				continue;
			}

			assign_buffer_to_related_nodes(execution_graph, output_node_index, inout_connections, nodes_to_buffers,
				m_buffer_count);
		}

		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			// For each inout buffer, find all attached nodes and create a mapping
			uint32 input_node_index = m_buffer_lists[task.inout_buffers_start + index * 2];
#if PREDEFINED(ASSERTS_ENABLED)
			uint32 output_node_index = m_buffer_lists[task.inout_buffers_start + index * 2 + 1];
#endif // PREDEFINED(ASSERTS_ENABLED)

			// This mapping should always be equal because this is an inout buffer - there is only one buffer
			wl_assert(nodes_to_buffers[input_node_index] == nodes_to_buffers[output_node_index]);

			if (nodes_to_buffers[input_node_index] != k_no_buffer) {
				// Already allocated, skip this one
				continue;
			}

			// Because this is an inout node, we only need to call the assignment function on one side, and the function
			// will recurse and assign both sides to the buffer
			assign_buffer_to_related_nodes(execution_graph, input_node_index, inout_connections, nodes_to_buffers,
				m_buffer_count);
			m_buffer_count++;
		}
	}

	// Finally, rebuild the buffer lists by following the node-to-buffer mapping
	std::vector<uint32> new_buffer_lists;
	new_buffer_lists.reserve(total_buffer_lists_size);

	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		s_task &task = m_tasks[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task.task_function);

		size_t new_in_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task.in_buffers_start + index];
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task.in_buffers_start = new_in_buffers_start;

		size_t new_out_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task.out_buffers_start + index];
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task.out_buffers_start = new_out_buffers_start;

		size_t new_inout_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task.out_buffers_start + index * 2];

			// Both nodes should be mapped to the same buffer
#if PREDEFINED(ASSERTS_ENABLED)
			uint32 node_index_verify = m_buffer_lists[task.out_buffers_start + index * 2 + 1];
#endif // PREDEFINED(ASSERTS_ENABLED)
			wl_assert(nodes_to_buffers[node_index] == nodes_to_buffers[node_index_verify]);
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task.inout_buffers_start = new_inout_buffers_start;
	}

	// Build final output buffer list. The buffers are listed in output-index order
	m_output_buffers_start = new_buffer_lists.size();
	new_buffer_lists.resize(new_buffer_lists.size() + m_output_buffers_count, k_no_buffer);
	for (uint32 node_index = 0; node_index < execution_graph.get_node_count(); node_index++) {
		if (execution_graph.get_node_type(node_index) == k_execution_graph_node_type_output) {
			// Offset into the list by the output index
			uint32 output_index = execution_graph.get_output_node_output_index(node_index);
			wl_assert(new_buffer_lists[m_output_buffers_start + output_index] == k_no_buffer);
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists[m_output_buffers_start + output_index] = nodes_to_buffers[node_index];
		}
	}

#if PREDEFINED(ASSERTS_ENABLED)
	// Final verification that we didn't set anything to k_no_buffer
	for (size_t index = 0; index < new_buffer_lists.size(); index++) {
		wl_assert(new_buffer_lists[index] != k_no_buffer);
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	wl_assert(new_buffer_lists.size() == total_buffer_lists_size);
	m_buffer_lists.swap(new_buffer_lists);
}

void c_task_graph::assign_buffer_to_related_nodes(const c_execution_graph &execution_graph, uint32 node_index,
	const std::vector<uint32> &inout_connections, std::vector<uint32> &nodes_to_buffers, uint32 buffer_index) {
	if (nodes_to_buffers[node_index] != k_no_buffer) {
		// Already assigned, don't recurse forever
		wl_assert(nodes_to_buffers[node_index] == buffer_index);
		return;
	}

	nodes_to_buffers[node_index] = buffer_index;

	e_execution_graph_node_type node_type = execution_graph.get_node_type(node_index);
	if (node_type == k_execution_graph_node_type_native_module_input) {
		// Recurse on all connected outputs
		for (uint32 edge = 0; edge < execution_graph.get_node_incoming_edge_count(node_index); edge++) {
			uint32 edge_node_index = execution_graph.get_node_incoming_edge_index(node_index, edge);

			// Skip constants
			if (execution_graph.get_node_type(edge_node_index) == k_execution_graph_node_type_constant) {
				continue;
			}

			// The only other option is native module outputs, which will be asserted on the recursive call
			assign_buffer_to_related_nodes(execution_graph, edge_node_index,
				inout_connections, nodes_to_buffers, buffer_index);
		}
	} else if (node_type == k_execution_graph_node_type_native_module_output) {
		// Recurse on all connected inputs
		for (uint32 edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_index); edge++) {
			uint32 edge_node_index = execution_graph.get_node_outgoing_edge_index(node_index, edge);

			// This call will either be performed on native module inputs or output nodes, which will be asserted on the
			// recursive call
			assign_buffer_to_related_nodes(execution_graph, edge_node_index,
				inout_connections, nodes_to_buffers, buffer_index);
		}
	} else if (node_type == k_execution_graph_node_type_output) {
		// No recursive work to do here
	} else {
		// We should never hit any other node types
		wl_halt();
	}

	if (inout_connections[node_index] != c_execution_graph::k_invalid_index) {
		// This node has been assigned to an inout buffer, so also run on the corresponding input or output
		assign_buffer_to_related_nodes(execution_graph, inout_connections[node_index],
			inout_connections, nodes_to_buffers, buffer_index);
	}
}

void c_task_graph::calculate_max_buffer_concurrency() {
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

	c_predecessor_resolver predecessor_resolver(static_cast<uint32>(m_tasks.size()));
	// First add all direct successors
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];

		for (size_t suc = 0; suc < task.successors_count; suc++) {
			uint32 successor_index = m_task_lists[task.successors_start + suc];
			predecessor_resolver.a_precedes_b(successor_index, task_index);
		}
	}

	predecessor_resolver.resolve();

	// 2) Determine which buffers are concurrent. This means that it is possible or necessary for the buffers to exist
	// at the same time.

	// To determine whether two tasks (a,b) can be parallel, we just ask whether (a !precedes b) and (b !precedes a).
	// The predecessor resolver provides this data.

	// The information we actually want to know is whether we can merge buffers, so we actually need to know which
	// buffers can exist in parallel. We can convert task concurrency data to buffer concurrency data.
	std::vector<bool> buffer_concurrency(m_buffer_count * m_buffer_count, false);

	// For any task, all buffer pairs are concurrent
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		const s_task &task = m_tasks[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task.task_function);

		size_t list_starts[] = {
			task.in_buffers_start,
			task.out_buffers_start,
			task.inout_buffers_start
		};
		size_t list_counts[] = {
			task_function_description.in_buffer_count,
			task_function_description.out_buffer_count,
			task_function_description.inout_buffer_count
		};

		static_assert(NUMBEROF(list_starts) == NUMBEROF(list_counts), "Start/count mismatch?");

		for (size_t list_a = 0; list_a < NUMBEROF(list_starts); list_a++) {
			size_t start_a = list_starts[list_a];
			size_t count_a = list_counts[list_a];

			for (size_t index_a = 0; index_a < count_a; index_a++) {
				uint32 buffer_a = m_buffer_lists[start_a + index_a];

				for (size_t list_b = 0; list_b < NUMBEROF(list_starts); list_b++) {
					size_t start_b = list_starts[list_b];
					size_t count_b = list_counts[list_b];

					for (size_t index_b = 0; index_b < count_b; index_b++) {
						uint32 buffer_b = m_buffer_lists[start_b + index_b];

						buffer_concurrency[buffer_a * m_buffer_count + buffer_b] = true;
						buffer_concurrency[buffer_b * m_buffer_count + buffer_a] = true;
					}
				}
			}
		}
	}

	// For any pair of concurrent tasks, all buffer pairs between the two tasks are concurrent
	for (uint32 task_a_index = 0; task_a_index < m_tasks.size(); task_a_index++) {
		const s_task &task_a = m_tasks[task_a_index];
		const s_task_function_description &task_a_function_description =
			c_task_function_registry::get_task_function_description(task_a.task_function);

		size_t list_starts_a[] = {
			task_a.in_buffers_start,
			task_a.out_buffers_start,
			task_a.inout_buffers_start
		};
		size_t list_counts_a[] = {
			task_a_function_description.in_buffer_count,
			task_a_function_description.out_buffer_count,
			task_a_function_description.inout_buffer_count
		};

		static_assert(NUMBEROF(list_starts_a) == NUMBEROF(list_counts_a), "Start/count mismatch?");

		for (uint32 task_b_index = task_a_index + 1; task_b_index < m_tasks.size(); task_b_index++) {
			if (!predecessor_resolver.are_a_b_concurrent(task_a_index, task_b_index)) {
				continue;
			}

			const s_task &task_b = m_tasks[task_b_index];
			const s_task_function_description &task_b_function_description =
				c_task_function_registry::get_task_function_description(task_b.task_function);

			size_t list_starts_b[] = {
				task_b.in_buffers_start,
				task_b.out_buffers_start,
				task_b.inout_buffers_start
			};
			size_t list_counts_b[] = {
				task_b_function_description.in_buffer_count,
				task_b_function_description.out_buffer_count,
				task_b_function_description.inout_buffer_count
			};

			static_assert(NUMBEROF(list_starts_b) == NUMBEROF(list_counts_b), "Start/count mismatch?");

			for (size_t list_a = 0; list_a < NUMBEROF(list_starts_a); list_a++) {
				size_t start_a = list_starts_a[list_a];
				size_t count_a = list_counts_a[list_a];

				for (size_t index_a = 0; index_a < count_a; index_a++) {
					uint32 buffer_a = m_buffer_lists[start_a + index_a];

					for (size_t list_b = 0; list_b < NUMBEROF(list_starts_b); list_b++) {
						size_t start_b = list_starts_b[list_b];
						size_t count_b = list_counts_b[list_b];

						for (size_t index_b = 0; index_b < count_b; index_b++) {
							uint32 buffer_b = m_buffer_lists[start_b + index_b];

							buffer_concurrency[buffer_a * m_buffer_count + buffer_b] = true;
							buffer_concurrency[buffer_b * m_buffer_count + buffer_a] = true;
						}
					}
				}
			}
		}
	}

	// All output buffers are concurrent
	for (size_t out_buffer_a = 0; out_buffer_a < m_output_buffers_count; out_buffer_a++) {
		for (size_t out_buffer_b = out_buffer_a + 1; out_buffer_b < m_output_buffers_count; out_buffer_b++) {
			uint32 output_buffer_a_index = m_buffer_lists[m_output_buffers_start + out_buffer_a];
			uint32 output_buffer_b_index = m_buffer_lists[m_output_buffers_start + out_buffer_b];

			buffer_concurrency[output_buffer_a_index * m_buffer_count + output_buffer_b_index] = true;
			buffer_concurrency[output_buffer_b_index * m_buffer_count + output_buffer_a_index] = true;
		}
	}

	// 3) Merge buffers which are not concurrent. To merge buffer x with buffers S=(a,b,c,...), buffer x must not be
	// concurrent with any buffers in S.

	static const size_t k_none = static_cast<size_t>(-1);
	struct s_merged_buffer {
		uint32 buffer_index;
		size_t next;

	};

	std::vector<s_merged_buffer> merge_group_lists;
	std::vector<size_t> merge_group_heads;

	// Attempt to merge each buffer with the first possible merge group we encounter. If no merge is successful, make a
	// new merge group.
	for (uint32 buffer_index = 0; buffer_index < m_buffer_count; buffer_index++) {
		bool merged = false;
		for (size_t merge_group_index = 0;
			 !merged && merge_group_index < merge_group_heads.size();
			 merge_group_index++) {
			bool can_merge = true;
			size_t node_index = merge_group_heads[merge_group_index];
			while (node_index != k_none && can_merge) {
				s_merged_buffer merged_buffer = merge_group_lists[node_index];
				if (buffer_concurrency[buffer_index * m_buffer_count + merged_buffer.buffer_index]) {
					// These two buffers are concurrent, so we can't merge them
					can_merge = false;
				}

				node_index = merged_buffer.next;
			}

			if (can_merge) {
				merge_group_heads[merge_group_index] = merge_group_lists.size();

				s_merged_buffer merged_buffer;
				merged_buffer.buffer_index = buffer_index;
				merged_buffer.next = merge_group_heads[merge_group_index];
				merge_group_lists.push_back(merged_buffer);

				merged = true;
			}
		}

		if (!merged) {
			// No valid merges, add a new merge group
			merge_group_heads.push_back(merge_group_lists.size());

			s_merged_buffer merged_buffer;
			merged_buffer.buffer_index = buffer_index;
			merged_buffer.next = k_none;
			merge_group_lists.push_back(merged_buffer);
		}
	}

	// The number of merge groups we end up with is the max buffer concurrency. It is an overestimate, meaning we will
	// reserve more memory than necessary.
	m_max_buffer_concurrency = static_cast<uint32>(merge_group_heads.size());
}
