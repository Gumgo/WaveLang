#include "engine/task_graph.h"
#include "execution_graph/native_modules.h"
#include "execution_graph/execution_graph.h"

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
}

c_task_graph::~c_task_graph() {
}

bool c_task_graph::build(const c_execution_graph &execution_graph) {
	m_task_nodes.clear();
	m_constant_lists.clear();
	m_buffer_lists.clear();
	m_node_lists.clear();
	m_buffer_count = 0;
	m_max_buffer_concurrency = 0;

	bool success = true;

	// Loop over each node and build up the graph
	for (uint32 index = 0; success && index < execution_graph.get_node_count(); index++) {
		// We should never encounter these node types, even if the graph was loaded from a file (they should never
		// appear in a file and if they do we should fail to load).
		wl_assert(execution_graph.get_node_type(index) != k_execution_graph_node_type_invalid);
		wl_assert(execution_graph.get_node_type(index) != k_execution_graph_node_type_intermediate_value);

		// We only care about native module calls - we access other node types through the call's edges
		if (execution_graph.get_node_type(index) == k_execution_graph_node_type_native_module_call) {
			success &= add_task_for_node(execution_graph, index);
		}
	}

	if (success) {
		allocate_buffers(execution_graph);
		calculate_max_buffer_concurrency();
	}

	if (!success) {
		m_task_nodes.clear();
		m_constant_lists.clear();
		m_buffer_lists.clear();
		m_node_lists.clear();
		m_buffer_count = 0;
		m_max_buffer_concurrency = 0;
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
bool c_task_graph::add_task_for_node(const c_execution_graph &execution_graph, uint32 node_index) {
	wl_assert(execution_graph.get_node_type(node_index) == k_execution_graph_node_type_native_module_call);

	uint32 task_node_index = static_cast<uint32>(m_task_nodes.size());
	m_task_nodes.push_back(s_task_node());

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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_negation_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_addition_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_subtraction_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_multiplication_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_division_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
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
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_bufferio_buffer, c_task_mapping_array::construct(mapping));
				return true;
			} else if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_I(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_buffer_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_I(1), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_buffer_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "vc")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 0)) {
				s_task_mapping mapping[] = { TM_IO(0), TM_C(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_bufferio_constant, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_I(0), TM_C(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_buffer_constant, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else if (do_native_module_call_inputs_match(execution_graph, node_index, "cv")) {
			if (!does_native_module_call_input_branch(execution_graph, node_index, 1)) {
				s_task_mapping mapping[] = { TM_C(0), TM_IO(0), TM_IO(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_constant_bufferio, c_task_mapping_array::construct(mapping));
				return true;
			} else {
				s_task_mapping mapping[] = { TM_C(0), TM_I(0), TM_O(0) };
				setup_task(execution_graph, node_index, task_node_index,
					k_task_function_modulo_constant_buffer, c_task_mapping_array::construct(mapping));
				return true;
			}
		} else {
			return false;
		}

	case k_native_module_test:
		if (do_native_module_call_inputs_match(execution_graph, node_index, "")) {
			s_task_mapping mapping[] = { TM_O(0) };
			setup_task(execution_graph, node_index, task_node_index,
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

	s_task_node &task_node = m_task_nodes[task_index];
	const s_task_function_description &task_function_description =
		c_task_function_registry::get_task_function_description(task_function);

	// Set up the lists of task inputs/outputs
	// Initially, we store the execution graph nodes in this list
	// For the inout list, we need to store both input and output node index, so we double its size (for now)
	task_node.in_constants_start = m_constant_lists.size();
	task_node.in_buffers_start = m_buffer_lists.size();
	task_node.out_buffers_start = task_node.in_buffers_start + task_function_description.in_buffer_count;
	task_node.inout_buffers_start = task_node.out_buffers_start + task_function_description.out_buffer_count;

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
			m_constant_lists[task_node.in_constants_start + task_mapping.index] =
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
			wl_assert(m_buffer_lists[task_node.in_buffers_start + task_mapping.index] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task_node.in_buffers_start + task_mapping.index] = input_node_index;
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
			wl_assert(m_buffer_lists[task_node.inout_buffers_start + task_mapping.index * 2] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task_node.inout_buffers_start + task_mapping.index * 2] = input_node_index;
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
			wl_assert(m_buffer_lists[task_node.out_buffers_start + task_mapping.index] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task_node.out_buffers_start + task_mapping.index] = output_node_index;
		} else if (task_mapping.location == k_task_mapping_location_buffer_inout) {
			wl_assert(VALID_INDEX(task_mapping.index, task_function_description.inout_buffer_count));

			// Obtain the output node
			uint32 output_node_index = execution_graph.get_node_outgoing_edge_index(node_index, index);

			// Store the output node index for now - we're storing both input and output, so make space for both
			wl_assert(m_buffer_lists[task_node.inout_buffers_start + task_mapping.index * 2 + 1] ==
				c_execution_graph::k_invalid_index);
			m_buffer_lists[task_node.inout_buffers_start + task_mapping.index * 2 + 1] = output_node_index;
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

static const uint32 k_no_buffer = static_cast<uint32>(-1);

void c_task_graph::allocate_buffers(const c_execution_graph &execution_graph) {
	// Associates input and output nodes for when inout buffers have been chosen
	std::vector<uint32> inout_connections(execution_graph.get_node_count(), c_execution_graph::k_invalid_index);

	// Maps input/output nodes to allocated buffers
	std::vector<uint32> nodes_to_buffers(execution_graph.get_node_count(), k_no_buffer);

	size_t total_buffer_lists_size = 0;
	for (uint32 task_index = 0; task_index < m_task_nodes.size(); task_index++) {
		const s_task_node &task_node = m_task_nodes[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task_node.task_function);

		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			uint32 input_node_index = m_buffer_lists[task_node.inout_buffers_start + index * 2];
			uint32 output_node_index = m_buffer_lists[task_node.inout_buffers_start + index * 2 + 1];
			inout_connections[input_node_index] = output_node_index;
			inout_connections[output_node_index] = input_node_index;
		}

		total_buffer_lists_size += task_function_description.in_buffer_count;
		total_buffer_lists_size += task_function_description.out_buffer_count;
		total_buffer_lists_size += task_function_description.inout_buffer_count;
	}

	for (uint32 task_index = 0; task_index < m_task_nodes.size(); task_index++) {
		const s_task_node &task_node = m_task_nodes[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task_node.task_function);

		for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
			// For each in buffer, find all attached nodes and create a mapping
			uint32 input_node_index = m_buffer_lists[task_node.in_buffers_start + index];
			if (nodes_to_buffers[input_node_index] != k_no_buffer) {
				// Already allocated, skip this one
				continue;
			}

			assign_buffer_to_related_nodes(execution_graph, input_node_index, inout_connections, nodes_to_buffers,
				m_buffer_count);
		}

		for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
			// For each out buffer, find all attached nodes and create a mapping
			uint32 output_node_index = m_buffer_lists[task_node.out_buffers_start + index];
			if (nodes_to_buffers[output_node_index] != k_no_buffer) {
				// Already allocated, skip this one
				continue;
			}

			assign_buffer_to_related_nodes(execution_graph, output_node_index, inout_connections, nodes_to_buffers,
				m_buffer_count);
		}

		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			// For each inout buffer, find all attached nodes and create a mapping
			uint32 input_node_index = m_buffer_lists[task_node.inout_buffers_start + index * 2];
#if PREDEFINED(ASSERTS_ENABLED)
			uint32 output_node_index = m_buffer_lists[task_node.inout_buffers_start + index * 2 + 1];
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

	for (uint32 task_index = 0; task_index < m_task_nodes.size(); task_index++) {
		s_task_node &task_node = m_task_nodes[task_index];
		const s_task_function_description &task_function_description =
			c_task_function_registry::get_task_function_description(task_node.task_function);

		size_t new_in_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.in_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task_node.in_buffers_start + index];
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task_node.in_buffers_start = new_in_buffers_start;

		size_t new_out_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.out_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task_node.out_buffers_start + index];
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task_node.out_buffers_start = new_out_buffers_start;

		size_t new_inout_buffers_start = new_buffer_lists.size();
		for (size_t index = 0; index < task_function_description.inout_buffer_count; index++) {
			uint32 node_index = m_buffer_lists[task_node.out_buffers_start + index * 2];

			// Both nodes should be mapped to the same buffer
#if PREDEFINED(ASSERTS_ENABLED)
			uint32 node_index_verify = m_buffer_lists[task_node.out_buffers_start + index * 2 + 1];
#endif // PREDEFINED(ASSERTS_ENABLED)
			wl_assert(nodes_to_buffers[node_index] == nodes_to_buffers[node_index_verify]);
			wl_assert(nodes_to_buffers[node_index] != k_no_buffer);
			new_buffer_lists.push_back(nodes_to_buffers[node_index]);
		}
		task_node.inout_buffers_start = new_inout_buffers_start;
	}

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

			// Skip output nodes
			if (execution_graph.get_node_type(edge_node_index) == k_execution_graph_node_type_output) {
				continue;
			}

			// The only other option is native module inputs, which will be asserted on the recursive call
			assign_buffer_to_related_nodes(execution_graph, edge_node_index,
				inout_connections, nodes_to_buffers, buffer_index);
		}
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
}
