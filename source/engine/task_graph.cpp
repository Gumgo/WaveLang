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

// Rebasing helpers
template<typename t_pointer> static t_pointer *store_index_in_pointer(size_t index);
template<typename t_pointer> static size_t extract_index_from_pointer(t_pointer *pointer);

// Returns true if the given input is used as more than once. This indicates that the input cannot be shared with an
// output because overwriting the buffer would invalidate its second usage as an input.
static bool does_native_module_call_input_branch(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	size_t in_arg_index);

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
bool output_task_graph_build_result(const c_task_graph &task_graph, const char *filename);
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)

c_task_buffer_iterator::c_task_buffer_iterator(c_task_function_arguments arguments) {
	m_arguments = arguments;
	m_argument_index = k_invalid_index;
	m_array_index = k_invalid_index;
	next();
}

bool c_task_buffer_iterator::is_valid() const {
	return m_argument_index < m_arguments.get_count();
}

void c_task_buffer_iterator::next() {
	if (m_argument_index == k_invalid_index) {
		wl_assert(m_array_index == k_invalid_index);
	} else {
		wl_assert(is_valid());
	}

	bool advance;
	do {
		if (m_argument_index == k_invalid_index) {
			m_argument_index = 0;
			m_array_index = 0;
		} else {
			const s_task_function_argument &argument = m_arguments[m_argument_index];
			if (argument.data.type.get_qualifier() == e_task_qualifier::k_constant) {
				// Always skip constants
				m_argument_index++;
				m_array_index = 0;
			} else if (argument.data.type.get_data_type().is_array()) {
				size_t current_array_count = argument.data.value.buffer_array.get_count();
				m_array_index++;
				if (m_array_index == current_array_count) {
					m_argument_index++;
					m_array_index = 0;
				}
			} else {
				m_argument_index++;
				m_array_index = 0;
			}
		}

		if (!is_valid()) {
			advance = false;
		} else {
			const s_task_function_argument &new_argument = m_arguments[m_argument_index];

			// Keep skipping past constants
			advance = new_argument.data.type.get_qualifier() == e_task_qualifier::k_constant;
			if (!advance) {
				c_buffer *buffer = nullptr;
				if (new_argument.data.type.get_data_type().is_array()) {
					size_t new_array_count = new_argument.data.value.buffer_array.get_count();
					if (new_array_count == 0) {
						// Skip past 0-sized arrays
						advance = true;
					} else {
						buffer = new_argument.data.value.buffer_array[m_array_index];
					}
				} else {
					buffer = new_argument.data.value.buffer;
				}

				if (buffer && buffer->is_compile_time_constant()) {
					// Skip buffers pointing to compile-time constants
					advance = true;
				}
			}
		}
	} while (advance);
}

c_buffer *c_task_buffer_iterator::get_buffer() const {
	wl_assert(is_valid());
	const s_task_function_argument &argument = m_arguments[m_argument_index];
	wl_assert(argument.data.type.get_qualifier() != e_task_qualifier::k_constant);

	if (argument.data.type.get_data_type().is_array()) {
		return argument.data.value.buffer_array[m_array_index];
	} else {
		return argument.data.value.buffer;
	}
}

c_task_qualified_data_type c_task_buffer_iterator::get_buffer_type() const {
	wl_assert(is_valid());
	const s_task_function_argument &argument = m_arguments[m_argument_index];
	wl_assert(argument.data.type.get_qualifier() != e_task_qualifier::k_constant);

	c_task_qualified_data_type result = argument.data.type;
	if (result.get_data_type().is_array()) {
		result = c_task_qualified_data_type(result.get_data_type().get_element_type(), result.get_qualifier());
	}

	return result;
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

c_task_function_arguments c_task_graph::get_task_arguments(uint32 task_index) const {
	const s_task &task = m_tasks[task_index];
	const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);
	return c_task_function_arguments(
		task_function.argument_count == 0 ? nullptr : &m_task_function_arguments[task.arguments_start],
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

c_buffer_array c_task_graph::get_inputs() const {
	return c_buffer_array(
		m_input_buffers.empty() ? nullptr : &m_input_buffers.front(),
		m_input_buffers.size());
}

c_buffer_array c_task_graph::get_outputs() const {
	return c_buffer_array(
		m_output_buffers.empty() ? nullptr : &m_output_buffers.front(),
		m_output_buffers.size());
}

c_buffer *c_task_graph::get_remain_active_output() const {
	return m_remain_active_output_buffer;
}

size_t c_task_graph::get_buffer_count() const {
	return m_buffers.size();
}

c_buffer *c_task_graph::get_buffer_by_index(size_t buffer_index) const {
	return const_cast<c_buffer *>(&m_buffers[buffer_index]); // Don't love the const_cast here...
}

size_t c_task_graph::get_buffer_index(const c_buffer *buffer) const {
	wl_assert(buffer >= &m_buffers.front() && buffer < &m_buffers.front() + m_buffers.size());
	return buffer - &m_buffers.front();
}

c_wrapped_array<const c_task_graph::s_buffer_usage_info> c_task_graph::get_buffer_usage_info() const {
	return c_wrapped_array<const c_task_graph::s_buffer_usage_info>(
		m_buffer_usage_info.empty() ? nullptr : &m_buffer_usage_info.front(),
		m_buffer_usage_info.size());
}

bool c_task_graph::build(const c_execution_graph &execution_graph) {
	clear();

	// Maps nodes in the execution graph to tasks in the task graph
	std::map<c_node_reference, uint32> nodes_to_tasks;

	bool success = true;

	// Order the nodes so we can walk traverse them in a directed way. Also count up the number of inputs and outputs.
	size_t input_count = 0;
	size_t output_count = 0;
	std::vector<c_node_reference> ordered_nodes;
	ordered_nodes.reserve(execution_graph.get_node_count());

	std::map<c_node_reference, size_t> node_dependencies_remaining;
	std::vector<c_node_reference> node_stack;
	node_stack.reserve(execution_graph.get_node_count());

	// Find our initial nodes without any predecessors
	for (c_node_reference node_reference = execution_graph.nodes_begin();
		node_reference.is_valid();
		node_reference = execution_graph.nodes_next(node_reference)) {
		e_execution_graph_node_type node_type = execution_graph.get_node_type(node_reference);
		if (node_type == e_execution_graph_node_type::k_input) {
			input_count++;
		} else if (node_type == e_execution_graph_node_type::k_output) {
			output_count++;
		}

		node_dependencies_remaining.insert(
			std::make_pair(node_reference, execution_graph.get_node_incoming_edge_count(node_reference)));
		if (execution_graph.get_node_incoming_edge_count(node_reference) == 0) {
			node_stack.push_back(node_reference);
		}
	}

	while (!node_stack.empty()) {
		c_node_reference node_reference = node_stack.back();
		node_stack.pop_back();
		wl_assert(node_dependencies_remaining[node_reference] == 0);

		ordered_nodes.push_back(node_reference);
		for (size_t edge = 0; edge < execution_graph.get_node_outgoing_edge_count(node_reference); edge++) {
			c_node_reference edge_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, edge);
			size_t dependencies_remaining = --node_dependencies_remaining[edge_node_reference];
			wl_assert(dependencies_remaining >= 0);

			if (dependencies_remaining == 0) {
				node_stack.push_back(edge_node_reference);
			}
		}
	}

	wl_assert(ordered_nodes.size() == execution_graph.get_node_count());

	// The remain-active buffer must always exist
	wl_assert(output_count >= 1);

	m_input_buffers.resize(input_count, nullptr);
	m_output_buffers.resize(output_count - 1, nullptr); // Remain-active buffer tracked separately
	m_remain_active_output_buffer = nullptr;

	// Traverse each node and build up the graph
	for (c_node_reference node_reference : ordered_nodes) {
		// We should never encounter these node types, even if the graph was loaded from a file (they should never
		// appear in a file and if they do we should fail to load).
		e_execution_graph_node_type node_type = execution_graph.get_node_type(node_reference);
		wl_assert(node_type != e_execution_graph_node_type::k_invalid);

		if (node_type == e_execution_graph_node_type::k_input) {
			create_buffer_for_input(execution_graph, node_reference);
		} else if (node_type == e_execution_graph_node_type::k_output) {
			assign_buffer_to_output(execution_graph, node_reference);
		} else if (node_type == e_execution_graph_node_type::k_native_module_call) {
			success &= add_task_for_node(execution_graph, node_reference, nodes_to_tasks);
		}
	}

	if (success) {
		rebase_arrays();
		rebase_strings();
		rebase_buffers();

#if IS_TRUE(ASSERTS_ENABLED)
		// Make sure all inputs and outputs were assigned
		for (size_t index = 0; index < m_input_buffers.size(); index++) {
			wl_assert(m_input_buffers[index]);
		}

		for (size_t index = 0; index < m_output_buffers.size(); index++) {
			wl_assert(m_output_buffers[index]);
		}

		wl_assert(m_remain_active_output_buffer);
#endif // IS_TRUE(ASSERTS_ENABLED)

		build_task_successor_lists(execution_graph, nodes_to_tasks);
		calculate_max_concurrency();

#if IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
		output_task_graph_build_result(*this, "graph_build_result.csv");
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
	}

	// These are no longer needed
	m_nodes_to_buffers.clear();
	m_output_nodes_to_shared_input_nodes.clear();

	if (!success) {
		clear();
	}

	return success;
}

template<typename t_pointer> static t_pointer *store_index_in_pointer(size_t index) {
	return reinterpret_cast<t_pointer *>(index);
}

template<typename t_pointer> static size_t extract_index_from_pointer(t_pointer *pointer) {
	return reinterpret_cast<size_t>(pointer);
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

void c_task_graph::clear() {
	m_tasks.clear();
	m_task_function_arguments.clear();
	m_buffers.clear();
	m_nodes_to_buffers.clear();
	m_output_nodes_to_shared_input_nodes.clear();
	m_input_buffers.clear();
	m_output_buffers.clear();
	m_remain_active_output_buffer = nullptr;
	m_buffer_arrays.clear();
	m_real_constant_arrays.clear();
	m_bool_constant_arrays.clear();
	m_string_constant_arrays.clear();
	m_string_table.clear();
	m_task_lists.clear();
	m_max_task_concurrency = 0;
	m_buffer_usage_info.clear();
	m_initial_tasks_start = k_invalid_list_index;
	m_initial_tasks_count = 0;
}

void c_task_graph::create_buffer_for_input(const c_execution_graph &execution_graph, c_node_reference node_reference) {
	uint32 input_index = execution_graph.get_input_node_input_index(node_reference);
	c_buffer *input_buffer = add_or_get_buffer(
		execution_graph,
		node_reference,
		c_task_data_type(e_task_primitive_type::k_real));

	wl_assert(!m_input_buffers[input_index]);
	m_input_buffers[input_index] = input_buffer;
}

void c_task_graph::assign_buffer_to_output(const c_execution_graph &execution_graph, c_node_reference node_reference) {
	uint32 output_index = execution_graph.get_output_node_output_index(node_reference);
	c_node_reference input_node_reference = execution_graph.get_node_incoming_edge_reference(node_reference, 0);
	bool is_remain_active_output = (output_index == c_execution_graph::k_remain_active_output_index);

	c_buffer *output_buffer;
	if (m_nodes_to_buffers.find(input_node_reference) != m_nodes_to_buffers.end()) {
		output_buffer = m_nodes_to_buffers[input_node_reference];
	} else {
		wl_assert(execution_graph.get_node_type(input_node_reference) == e_execution_graph_node_type::k_constant);
		output_buffer = add_or_get_buffer(
			execution_graph,
			input_node_reference,
			c_task_data_type(is_remain_active_output ? e_task_primitive_type::k_bool : e_task_primitive_type::k_real));
	}

	if (is_remain_active_output) {
		wl_assert(!m_remain_active_output_buffer);
		m_remain_active_output_buffer = output_buffer;
	} else {
		wl_assert(!m_output_buffers[output_index]);
		m_output_buffers[output_index] = output_buffer;
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

	uint32 native_module_index = execution_graph.get_native_module_call_native_module_index(node_reference);
	return setup_task(execution_graph, node_reference, task_index);
}

bool c_task_graph::setup_task(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	uint32 task_index) {
	wl_assert(execution_graph.get_node_type(node_reference) == e_execution_graph_node_type::k_native_module_call);
	uint32 native_module_index = execution_graph.get_native_module_call_native_module_index(node_reference);
	s_task_function_uid task_function_uid = c_task_function_registry::get_task_function_mapping(native_module_index);
	if (task_function_uid == s_task_function_uid::k_invalid) {
		// No valid task function mapping for the native module
		return false;
	}

	s_task &task = m_tasks[task_index];
	task.task_function_index = c_task_function_registry::get_task_function_index(task_function_uid);
	wl_assert(task.task_function_index != k_invalid_task_function_index);

	const s_task_function &task_function = c_task_function_registry::get_task_function(task.task_function_index);

	// Set up the task arguments
	task.arguments_start = m_task_function_arguments.size();

	size_t old_task_function_arguments_size = m_task_function_arguments.size();
	m_task_function_arguments.resize(old_task_function_arguments_size + task_function.argument_count);

	// Make a wrapped array of the arguments for convenience
	c_wrapped_array<s_task_function_argument> task_function_arguments(
		&m_task_function_arguments[old_task_function_arguments_size],
		task_function.argument_count);

	// Map each input and output to its defined location in the task
	size_t input_output_count =
		execution_graph.get_node_incoming_edge_count(node_reference) +
		execution_graph.get_node_outgoing_edge_count(node_reference);

	// This is a candidate for sharing a single buffer for input and output
	struct s_share_candidate {
		c_node_reference node_reference;
		c_task_data_type data_type;
	};
	std::vector<s_share_candidate> share_candidates;

	// Process inputs first
	size_t input_index = 0;
	for (size_t index = 0; index < input_output_count; index++) {
		uint32 argument_index = task_function.task_function_argument_indices[index];
		wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
		s_task_function_argument &argument = task_function_arguments[argument_index];
		c_task_qualified_data_type argument_type = task_function.argument_types[argument_index];
		argument.data.type = argument_type;

		e_task_primitive_type primitive_type = argument_type.get_data_type().get_primitive_type();
		bool is_input = argument_type.get_qualifier() != e_task_qualifier::k_out;
		bool is_array = argument_type.get_data_type().is_array();
		bool is_constant = argument_type.get_qualifier() == e_task_qualifier::k_constant;

		if (is_input) {
			// Obtain the source input node
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, input_index);
			c_node_reference source_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);

			if (is_constant) {
				wl_assert(
					execution_graph.get_node_type(source_node_reference) == e_execution_graph_node_type::k_constant);
			}

			bool is_buffer = false;
			if (is_constant) {
				switch (primitive_type) {
				case e_task_primitive_type::k_real:
					if (is_array) {
						argument.data.value.real_constant_array =
							build_real_constant_array(execution_graph, source_node_reference);
					} else {
						argument.data.value.real_constant =
							execution_graph.get_constant_node_real_value(source_node_reference);
					}
					break;

				case e_task_primitive_type::k_bool:
					if (is_array) {
						argument.data.value.bool_constant_array =
							build_bool_constant_array(execution_graph, source_node_reference);
					} else {
						argument.data.value.bool_constant =
							execution_graph.get_constant_node_bool_value(source_node_reference);
					}
					break;

				case e_task_primitive_type::k_string:
					if (is_array) {
						argument.data.value.string_constant_array =
							build_string_constant_array(execution_graph, source_node_reference);
					} else {
						argument.data.value.string_constant =
							add_string(execution_graph.get_constant_node_string_value(source_node_reference));
					}
					break;

				default:
					wl_unreachable();
				}
			} else {
				wl_assert(argument_type.get_data_type().get_primitive_type_traits().is_dynamic);
				if (is_array) {
					argument.data.value.buffer_array = add_or_get_buffer_array(
						execution_graph,
						source_node_reference,
						argument_type.get_data_type().get_element_type());
				} else {
					argument.data.value.buffer = add_or_get_buffer(
						execution_graph,
						source_node_reference,
						argument_type.get_data_type());
					is_buffer = true;
				}
			}

			// See if we can share this input buffer with an output buffer
			if (!task_function.arguments_unshared[argument_index]
				&& is_buffer
				&& !does_native_module_call_input_branch(execution_graph, node_reference, input_index)) {
				// Determine if this is a non-constant sourced buffer. Note: we can't just check is_constant because we
				// may have a non-constant buffer that's pointing to constant data. is_constant just means compile-time
				// constant.
				const c_buffer &buffer = m_buffers[extract_index_from_pointer(argument.data.value.buffer)];
				if (!buffer.is_constant()) {
					// This is a buffer that will be filled with dynamic data, and it's only used by the current task
					// function. This means we might be able to share it with an output buffer.
					share_candidates.push_back(s_share_candidate());
					share_candidates.back().node_reference = source_node_reference;
					share_candidates.back().data_type = argument_type.get_data_type();
				}
			}

			input_index++;
		}
	}

	// Process outputs after inputs - this way, we can tell if an output is shared with an input and skip buffer
	// allocation in that case.
	size_t output_index = 0;
	for (size_t index = 0; index < input_output_count; index++) {
		uint32 argument_index = task_function.task_function_argument_indices[index];
		wl_assert(VALID_INDEX(argument_index, task_function.argument_count));
		s_task_function_argument &argument = task_function_arguments[argument_index];
		c_task_qualified_data_type argument_type = task_function.argument_types[argument_index];
		argument.data.type = argument_type;

		bool is_output = argument_type.get_qualifier() == e_task_qualifier::k_out;

		if (is_output) {
			// Obtain the output node
			c_node_reference output_node_reference =
				execution_graph.get_node_outgoing_edge_reference(node_reference, output_index);

			if (!task_function.arguments_unshared[argument_index]) {
				for (size_t share_index = 0; share_index < share_candidates.size(); share_index++) {
					const s_share_candidate &share_candidate = share_candidates[share_index];
					if (share_candidate.data_type == argument_type.get_data_type()) {
						// Share this argument with the share candidate
						m_output_nodes_to_shared_input_nodes[output_node_reference] = share_candidate.node_reference;

						// This candidate is used, so remove it
						share_candidates.erase(share_candidates.begin() + share_index);
						break;
					}
				}
			}

			// Outputs can't be arrays or constants
			wl_assert(!argument_type.get_data_type().is_array());
			wl_assert(argument_type.get_qualifier() != e_task_qualifier::k_constant);

			argument.data.value.buffer = add_or_get_buffer(
				execution_graph,
				output_node_reference,
				argument_type.get_data_type());
			output_index++;
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (s_task_function_argument &argument : task_function_arguments) {
		// Make sure we have filled in all values
		wl_assert(argument.data.type.is_valid());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	return true;
}

c_buffer *c_task_graph::add_or_get_buffer(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	c_task_data_type data_type) {
	wl_assert(data_type.is_valid());
	wl_assert(!data_type.is_array());

	if (m_nodes_to_buffers.find(node_reference) != m_nodes_to_buffers.end()) {
		return m_nodes_to_buffers[node_reference];
	}

	e_execution_graph_node_type node_type = execution_graph.get_node_type(node_reference);
	if (node_type == e_execution_graph_node_type::k_constant) {
		// Check to see if a buffer representing this constant already exists
		bool match = false;
		size_t buffer_index;
		for (buffer_index = 0; !match && buffer_index < m_buffers.size(); buffer_index++) {
			const c_buffer &buffer = m_buffers[buffer_index];
			if (buffer.is_compile_time_constant() && buffer.get_data_type() == data_type) {
				bool match = false;
				switch (data_type.get_primitive_type()) {
				case e_task_primitive_type::k_real:
				{
					real32 existing_value = buffer.get_as<c_real_buffer>().get_constant();
					real32 node_value = execution_graph.get_constant_node_real_value(node_reference);
					match = existing_value == node_value;
					break;
				}

				case e_task_primitive_type::k_bool:
				{
					bool existing_value = buffer.get_as<c_bool_buffer>().get_constant();
					bool node_value = execution_graph.get_constant_node_real_value(node_reference);
					match = existing_value == node_value;
					break;
				}

				case e_task_primitive_type::k_string:
					wl_halt();
					break;

				default:
					wl_unreachable();
				}
			}
		}

		if (!match) {
			// No match was found so create a new constant buffer
			buffer_index = m_buffers.size();
			switch (data_type.get_primitive_type()) {
			case e_task_primitive_type::k_real:
			{
				real32 node_value = execution_graph.get_constant_node_real_value(node_reference);
				m_buffers.push_back(c_real_buffer::construct_compile_time_constant(data_type, node_value));
				break;
			}

			case e_task_primitive_type::k_bool:
			{
				bool node_value = execution_graph.get_constant_node_bool_value(node_reference);
				m_buffers.push_back(c_bool_buffer::construct_compile_time_constant(data_type, node_value));
				break;
			}

			case e_task_primitive_type::k_string:
				wl_halt();
				break;

			default:
				wl_unreachable();
			}
		}

		// Return the index of the matching buffer, disguised as a pointer, to be rebased later
		c_buffer *buffer = store_index_in_pointer<c_buffer>(buffer_index);
		m_nodes_to_buffers.insert(std::make_pair(node_reference, buffer));
		return buffer;
	} else if (node_type == e_execution_graph_node_type::k_input) {
		// $TODO $UPSAMPLE the FX graph could have upsampled inputs
		wl_assert(data_type.get_primitive_type() == e_task_primitive_type::k_real);
	} else if (node_type == e_execution_graph_node_type::k_indexed_output) {
		// Don't allocate a buffer for this output if it's being shared with an input
		if (m_output_nodes_to_shared_input_nodes.find(node_reference) != m_output_nodes_to_shared_input_nodes.end()) {
			c_node_reference shared_input_node_reference = m_output_nodes_to_shared_input_nodes[node_reference];
			c_buffer *buffer = m_nodes_to_buffers[shared_input_node_reference];
			m_nodes_to_buffers.insert(std::make_pair(node_reference, buffer));
			return buffer;
		}
	} else {
		// We shouldn't be hitting any other node types here
		wl_unreachable();
	}

	// If we haven't returned already, allocate a new buffer
	size_t buffer_index = m_buffers.size();
	m_buffers.push_back(c_buffer::construct(data_type));

	c_buffer *buffer = store_index_in_pointer<c_buffer>(buffer_index);
	m_nodes_to_buffers.insert(std::make_pair(node_reference, buffer));
	return buffer;
}

c_buffer_array c_task_graph::add_or_get_buffer_array(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference,
	c_task_data_type data_type) {
	size_t array_count = execution_graph.get_node_incoming_edge_count(node_reference);
	size_t start_index;
	bool match = false;

	// Check to see if any existing buffer arrays already match
	for (start_index = 0; start_index + array_count < m_buffer_arrays.size(); start_index++) {
		bool match_inner = true;
		for (size_t index = 0; index < array_count; index++) {
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			c_node_reference element_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);
			c_buffer *existing_buffer = m_buffer_arrays[start_index + index];

			if (m_nodes_to_buffers.find(element_node_reference) == m_nodes_to_buffers.end()
				|| m_nodes_to_buffers[element_node_reference] != existing_buffer) {
				match_inner = false;
				break;
			}
		}

		if (match_inner) {
			match = true;
			break;
		}
	}

	if (!match) {
		start_index = m_buffer_arrays.size();
		m_buffer_arrays.resize(start_index + array_count);
		for (size_t index = 0; index < array_count; index++) {
			c_node_reference input_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			c_node_reference element_node_reference =
				execution_graph.get_node_incoming_edge_reference(input_node_reference, 0);
			c_buffer *buffer = add_or_get_buffer(execution_graph, element_node_reference, data_type);
			m_buffer_arrays[start_index + index] = buffer;
		}
	}

	return c_buffer_array(store_index_in_pointer<c_buffer *>(start_index), array_count);
}

c_real_constant_array c_task_graph::build_real_constant_array(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference) {
	size_t array_count = execution_graph.get_node_incoming_edge_count(node_reference);
	size_t start_index;
	bool match = false;

	// Check to see if any existing real arrays already match
	for (start_index = 0; start_index + array_count < m_real_constant_arrays.size(); start_index++) {
		bool match_inner = true;
		for (size_t index = 0; index < array_count; index++) {
			real32 existing_value = m_real_constant_arrays[start_index + index];
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			real32 node_value = execution_graph.get_constant_node_real_value(constant_node_reference);

			if (existing_value != node_value) {
				match_inner = false;
				break;
			}
		}

		if (match_inner) {
			match = true;
			break;
		}
	}

	if (!match) {
		start_index = m_real_constant_arrays.size();
		m_real_constant_arrays.resize(start_index + array_count);
		for (size_t index = 0; index < array_count; index++) {
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			real32 node_value = execution_graph.get_constant_node_real_value(constant_node_reference);
			m_real_constant_arrays[start_index + index] = node_value;
		}
	}

	return c_real_constant_array(store_index_in_pointer<const real32>(start_index), array_count);
}

c_bool_constant_array c_task_graph::build_bool_constant_array(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference) {
	size_t array_count = execution_graph.get_node_incoming_edge_count(node_reference);
	size_t start_index;
	bool match = false;

	// Check to see if any existing bool arrays already match
	for (start_index = 0; start_index + array_count < m_bool_constant_arrays.size(); start_index++) {
		bool match_inner = true;
		for (size_t index = 0; index < array_count; index++) {
			bool existing_value = static_cast<bool>(m_bool_constant_arrays[start_index + index]);
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			bool node_value = execution_graph.get_constant_node_bool_value(constant_node_reference);

			if (existing_value != node_value) {
				match_inner = false;
				break;
			}
		}

		if (match_inner) {
			match = true;
			break;
		}
	}

	if (!match) {
		start_index = m_bool_constant_arrays.size();
		m_bool_constant_arrays.resize(start_index + array_count);
		for (size_t index = 0; index < array_count; index++) {
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			bool node_value = execution_graph.get_constant_node_bool_value(constant_node_reference);
			m_bool_constant_arrays[start_index + index] = node_value;
		}
	}

	return c_bool_constant_array(store_index_in_pointer<const bool>(start_index), array_count);
}

c_string_constant_array c_task_graph::build_string_constant_array(
	const c_execution_graph &execution_graph,
	c_node_reference node_reference) {
	size_t array_count = execution_graph.get_node_incoming_edge_count(node_reference);
	size_t start_index;
	bool match = false;

	// Check to see if any existing string arrays already match
	for (start_index = 0; start_index + array_count < m_string_constant_arrays.size(); start_index++) {
		bool match_inner = true;
		for (size_t index = 0; index < array_count; index++) {
			const char *existing_value = get_pre_rebased_string(m_string_constant_arrays[start_index + index]);
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			const char *node_value = execution_graph.get_constant_node_string_value(constant_node_reference);

			if (strcmp(existing_value, node_value) == 0) {
				match_inner = false;
				break;
			}
		}

		if (match_inner) {
			match = true;
			break;
		}
	}

	if (!match) {
		start_index = m_string_constant_arrays.size();
		m_string_constant_arrays.resize(start_index + array_count);
		for (size_t index = 0; index < array_count; index++) {
			c_node_reference constant_node_reference =
				execution_graph.get_node_incoming_edge_reference(node_reference, index);
			const char *node_value = execution_graph.get_constant_node_string_value(constant_node_reference);
			m_string_constant_arrays[start_index + index] = add_string(node_value);
		}
	}

	return c_string_constant_array(store_index_in_pointer<const char *>(start_index), array_count);
}

void c_task_graph::rebase_arrays() {
	for (s_task_function_argument &argument : m_task_function_arguments) {
		if (argument.data.type.get_data_type().is_array()) {
			if (argument.data.type.get_qualifier() == e_task_qualifier::k_constant) {
				// The start index is currently stored in the pointer - convert this to the final pointer
				switch (argument.data.type.get_data_type().get_primitive_type()) {
				case e_task_primitive_type::k_real:
				{
					size_t start_index =
						extract_index_from_pointer(argument.data.value.real_constant_array.get_pointer());
					argument.data.value.real_constant_array = c_real_constant_array(
						&m_real_constant_arrays.front() + start_index,
						argument.data.value.real_constant_array.get_count());
					break;
				}

				case e_task_primitive_type::k_bool:
				{
					size_t start_index =
						extract_index_from_pointer(argument.data.value.bool_constant_array.get_pointer());
					argument.data.value.bool_constant_array = c_bool_constant_array(
						reinterpret_cast<bool *>(&m_bool_constant_arrays.front()) + start_index,
						argument.data.value.bool_constant_array.get_count());
					break;
				}

				case e_task_primitive_type::k_string:
				{
					size_t start_index =
						extract_index_from_pointer(argument.data.value.string_constant_array.get_pointer());
					argument.data.value.string_constant_array = c_string_constant_array(
						&m_string_constant_arrays.front() + start_index,
						argument.data.value.string_constant_array.get_count());
					break;
				}

				default:
					wl_unreachable();
				}
			} else {
				size_t start_index =
					extract_index_from_pointer(argument.data.value.buffer_array.get_pointer());
				argument.data.value.buffer_array = c_buffer_array(
					&m_buffer_arrays.front() + start_index,
					argument.data.value.buffer_array.get_count());
			}
		}
	}
}

void c_task_graph::rebase_buffers() {
	for (s_task_function_argument &argument : m_task_function_arguments) {
		if (!argument.data.type.get_data_type().is_array()
			&& argument.data.type.get_qualifier() != e_task_qualifier::k_constant) {
			size_t buffer_index = extract_index_from_pointer(argument.data.value.buffer);
			argument.data.value.buffer = &m_buffers[buffer_index];
		}
	}

	for (c_buffer *&buffer : m_buffer_arrays) {
		size_t buffer_index = extract_index_from_pointer(buffer);
		buffer = &m_buffers[buffer_index];
	}

	for (c_buffer *&buffer : m_input_buffers) {
		size_t buffer_index = extract_index_from_pointer(buffer);
		buffer = &m_buffers[buffer_index];
	}

	for (c_buffer *&buffer : m_output_buffers) {
		size_t buffer_index = extract_index_from_pointer(buffer);
		buffer = &m_buffers[buffer_index];
	}

	size_t buffer_index = extract_index_from_pointer(m_remain_active_output_buffer);
	m_remain_active_output_buffer = &m_buffers[buffer_index];
}

const char *c_task_graph::add_string(const char *string) {
	size_t offset = m_string_table.add_string(string);
	return store_index_in_pointer<const char>(offset);
}

const char *c_task_graph::get_pre_rebased_string(const char *string) const {
	size_t offset = extract_index_from_pointer(string);
	return m_string_table.get_string(offset);
}

void c_task_graph::rebase_string(const char *&string) {
	string = get_pre_rebased_string(string);
}

void c_task_graph::rebase_strings() {
	for (s_task_function_argument &argument : m_task_function_arguments) {
		if (argument.data.type.get_data_type().get_primitive_type() == e_task_primitive_type::k_string) {
			if (argument.data.type.get_data_type().is_array()) {
				// We will rebase all string array contents right after this
			} else {
				rebase_string(argument.data.value.string_constant);
			}
		}
	}

	for (const char *&string : m_string_constant_arrays) {
		rebase_string(string);
	}
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

				// We only care about native module and array inputs
				if (execution_graph.get_node_type(dest_node_reference) !=
					e_execution_graph_node_type::k_indexed_input) {
					continue;
				}

				c_node_reference input_node_reference =
					execution_graph.get_node_outgoing_edge_reference(dest_node_reference, 0);

				e_execution_graph_node_type input_node_type = execution_graph.get_node_type(input_node_reference);
				if (input_node_type == e_execution_graph_node_type::k_native_module_call) {
					// This output is used as a native module input
					// Find the task belonging to this node
					uint32 successor_task_index = nodes_to_tasks.at(input_node_reference);
					wl_assert(successor_task_index != k_invalid_task);
					add_task_successor(task_index, successor_task_index);
				} else if (input_node_type == e_execution_graph_node_type::k_constant) {
					// This output is used as an array input
					wl_assert(execution_graph.get_constant_node_data_type(input_node_reference).is_array());

					// Find all native modules which use this array
					for (size_t array_dest = 0;
						array_dest < execution_graph.get_node_outgoing_edge_count(input_node_reference);
						array_dest++) {
						c_node_reference array_dest_node_reference =
							execution_graph.get_node_outgoing_edge_reference(input_node_reference, array_dest);

						// We only care about native module inputs
						if (execution_graph.get_node_type(array_dest_node_reference) !=
							e_execution_graph_node_type::k_indexed_input) {
							continue;
						}

						c_node_reference array_input_node_reference =
							execution_graph.get_node_outgoing_edge_reference(array_dest_node_reference, 0);

						if (execution_graph.get_node_type(array_input_node_reference) ==
							e_execution_graph_node_type::k_native_module_call) {
							// This output is used as a native module input
							// Find the task belonging to this node
							uint32 successor_task_index = nodes_to_tasks.at(array_input_node_reference);
							wl_assert(successor_task_index != k_invalid_task);
							add_task_successor(task_index, successor_task_index);
						}
					}
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

void c_task_graph::add_task_successor(uint32 predecessor_task_index, uint32 successor_task_index) {
	s_task &predecessor_task = m_tasks[predecessor_task_index];

	// Avoid adding duplicates
	bool duplicate = false;
	for (size_t successor = 0; !duplicate && successor < predecessor_task.successors_count; successor++) {
		uint32 existing_successor_index = m_task_lists[predecessor_task.successors_start + successor];
		duplicate = (successor_task_index == existing_successor_index);
	}

	if (!duplicate) {
		m_task_lists.push_back(successor_task_index);
		predecessor_task.successors_count++;

		// Add this task as a predecessor to the successor
		m_tasks[successor_task_index].predecessor_count++;
	}
}

void c_task_graph::calculate_max_concurrency() {
	// In this function we calculate the worst case for the number of buffers which must be in memory concurrently. This
	// number may be much less than the number of buffers because in many cases, buffer B can only ever be created once
	// buffer A is no longer needed.

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

	for (const c_buffer &buffer : m_buffers) {
		add_usage_info_for_buffer_type(predecessor_resolver, buffer.get_data_type());
	}
}

void c_task_graph::add_usage_info_for_buffer_type(
	const c_predecessor_resolver &predecessor_resolver,
	c_task_data_type data_type) {
	// Skip if we've already calculated for this type
	size_t found = false;
	for (size_t info_index = 0; info_index < m_buffer_usage_info.size(); info_index++) {
		if (m_buffer_usage_info[info_index].type == data_type) {
			found = true;
			break;
		}
	}

	if (!found) {
		m_buffer_usage_info.push_back(s_buffer_usage_info());
		s_buffer_usage_info &info = m_buffer_usage_info.back();
		info.type = data_type;
		info.max_concurrency = calculate_max_buffer_concurrency(predecessor_resolver, data_type);
	}
}

uint32 c_task_graph::calculate_max_buffer_concurrency(
	const c_predecessor_resolver &task_predecessor_resolver,
	c_task_data_type data_type) const {
	// To determine whether two tasks (a,b) can be parallel, we just ask whether (a !precedes b) and (b !precedes a).
	// The predecessor resolver provides this data.

	// We're looking at individual buffers, so the array data type flag shouldn't make a difference
	wl_assert(!data_type.is_array());

	// The information we actually want to know is whether we can merge buffers, so we actually need to know which
	// buffers can exist in parallel. We can convert task concurrency data to buffer concurrency data.
	size_t buffer_count = m_buffers.size();
	std::vector<bool> buffer_concurrency(buffer_count * buffer_count, false);

	// Not all buffers are of the correct type - this bitvector masks out invalid buffers
	std::vector<bool> buffer_type_mask(buffer_count, false);
	uint32 valid_buffer_count = 0;

	// For any task, all buffer pairs of the same type are concurrent
	for (uint32 task_index = 0; task_index < m_tasks.size(); task_index++) {
		for (c_task_buffer_iterator it_a(get_task_arguments(task_index)); it_a.is_valid(); it_a.next()) {
			if (it_a.get_buffer()->get_data_type() != data_type) {
				continue;
			}

			size_t buffer_a_index = get_buffer_index(it_a.get_buffer());

			if (!buffer_type_mask[buffer_a_index]) {
				buffer_type_mask[buffer_a_index] = true;
				valid_buffer_count++;
			}

			for (c_task_buffer_iterator it_b(get_task_arguments(task_index)); it_b.is_valid(); it_b.next()) {
				if (it_b.get_buffer()->get_data_type() != data_type) {
					continue;
				}

				size_t buffer_b_index = get_buffer_index(it_b.get_buffer());
				buffer_concurrency[buffer_a_index * buffer_count + buffer_b_index] = true;
				buffer_concurrency[buffer_b_index * buffer_count + buffer_a_index] = true;
			}
		}
	}

	// For any pair of concurrent tasks, all buffer pairs between the two tasks are concurrent
	for (uint32 task_a_index = 0; task_a_index < m_tasks.size(); task_a_index++) {
		for (uint32 task_b_index = task_a_index + 1; task_b_index < m_tasks.size(); task_b_index++) {
			if (!task_predecessor_resolver.are_a_b_concurrent(task_a_index, task_b_index)) {
				continue;
			}

			for (c_task_buffer_iterator it_a(get_task_arguments(task_a_index)); it_a.is_valid(); it_a.next()) {
				if (it_a.get_buffer()->get_data_type() != data_type) {
					continue;
				}

				size_t buffer_a_index = get_buffer_index(it_a.get_buffer());
				for (c_task_buffer_iterator it_b(get_task_arguments(task_b_index)); it_b.is_valid(); it_b.next()) {
					if (it_b.get_buffer()->get_data_type() != data_type) {
						continue;
					}

					size_t buffer_b_index = get_buffer_index(it_b.get_buffer());
					buffer_concurrency[buffer_a_index * buffer_count + buffer_b_index] = true;
					buffer_concurrency[buffer_b_index * buffer_count + buffer_a_index] = true;
				}
			}
		}
	}

	// All input buffers are concurrent
	for (size_t input_a = 0; input_a < m_input_buffers.size(); input_a++) {
		c_buffer *input_a_buffer = m_input_buffers[input_a];
		wl_assert(!input_a_buffer->is_compile_time_constant());
		size_t input_a_buffer_index = get_buffer_index(input_a_buffer);
		c_task_data_type input_a_buffer_type = m_buffers[input_a_buffer_index].get_data_type();
		if (input_a_buffer_type != data_type) {
			continue;
		}

		if (!buffer_type_mask[input_a_buffer_index]) {
			buffer_type_mask[input_a_buffer_index] = true;
			valid_buffer_count++;
		}

		for (size_t input_b = input_a + 1; input_b < m_input_buffers.size(); input_b++) {
			c_buffer *input_b_buffer = m_input_buffers[input_b];
			wl_assert(!input_b_buffer->is_compile_time_constant());
			size_t input_b_buffer_index = get_buffer_index(input_b_buffer);
			c_task_data_type input_b_buffer_type = m_buffers[input_b_buffer_index].get_data_type();
			if (input_b_buffer_type != data_type) {
				continue;
			}

			size_t index_ab = input_a_buffer_index * buffer_count + input_b_buffer_index;
			size_t index_ba = input_b_buffer_index * buffer_count + input_a_buffer_index;
			buffer_concurrency[index_ab] = true;
			buffer_concurrency[index_ba] = true;
		}
	}

	// All output buffers are concurrent
	for (size_t output_a = 0; output_a < m_output_buffers.size(); output_a++) {
		c_buffer *output_a_buffer = m_output_buffers[output_a];
		if (output_a_buffer->is_compile_time_constant()) {
			continue;
		}

		size_t output_a_buffer_index = get_buffer_index(output_a_buffer);
		c_task_data_type output_a_buffer_type = m_buffers[output_a_buffer_index].get_data_type();
		if (output_a_buffer_type != data_type) {
			continue;
		}

		for (size_t output_b = output_a + 1; output_b < m_output_buffers.size(); output_b++) {
			c_buffer *output_b_buffer = m_output_buffers[output_b];
			if (output_b_buffer->is_compile_time_constant()) {
				continue;
			}

			size_t output_b_buffer_index = get_buffer_index(output_b_buffer);
			c_task_data_type output_b_buffer_type = m_buffers[output_b_buffer_index].get_data_type();
			if (output_b_buffer_type != data_type) {
				continue;
			}

			size_t index_ab = output_a_buffer_index * buffer_count + output_b_buffer_index;
			size_t index_ba = output_b_buffer_index * buffer_count + output_a_buffer_index;
			buffer_concurrency[index_ab] = true;
			buffer_concurrency[index_ba] = true;
		}
	}

	// The output buffers are also concurrent with the remain-active output buffer, but they are different types (real
	// and bool) so we don't need to bother marking them as concurrent since they will never be combined anyway. Assert
	// this here to be sure.
#if IS_TRUE(ASSERTS_ENABLED)
	size_t remain_active_output_buffer_index = get_buffer_index(m_remain_active_output_buffer);
	for (c_buffer *output_buffer : m_output_buffers) {
		size_t output_buffer_index = get_buffer_index(output_buffer);
		wl_assert(m_buffers[remain_active_output_buffer_index].get_data_type()
			!= m_buffers[output_buffer_index].get_data_type());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Reduce the matrix down to only valid buffers. The actual indices no longer matter at this point.
	std::vector<bool> reduced_buffer_concurrency;
	reduced_buffer_concurrency.reserve(valid_buffer_count * valid_buffer_count);

	for (uint32 buffer_a_index = 0; buffer_a_index < buffer_count; buffer_a_index++) {
		if (!buffer_type_mask[buffer_a_index]) {
			continue;
		}

		for (uint32 buffer_b_index = 0; buffer_b_index < buffer_count; buffer_b_index++) {
			if (!buffer_type_mask[buffer_b_index]) {
				continue;
			}

			reduced_buffer_concurrency.push_back(buffer_concurrency[buffer_a_index * buffer_count + buffer_b_index]);
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

		c_task_function_arguments arguments = task_graph.get_task_arguments(index);
		for (size_t arg = 0; arg < arguments.get_count(); arg++) {
			out << ",";
			if (arguments[arg].data.type.get_data_type().is_array()) {
				// $TODO support this case
				out << "array";
			} else {
				if (arguments[arg].data.type.get_qualifier() == e_task_qualifier::k_constant) {
					switch (arguments[arg].data.type.get_data_type().get_primitive_type()) {
					case e_task_primitive_type::k_real:
						out << arguments[arg].data.value.real_constant;
						break;

					case e_task_primitive_type::k_bool:
						out << (arguments[arg].data.value.bool_constant ? "true" : "false");
						break;

					case e_task_primitive_type::k_string:
						out << arguments[arg].data.value.string_constant;
						break;

					default:
						wl_unreachable();
					}
				} else {
					// $TODO can we output something better than a pointer?
					out << "[0x" << arguments[arg].data.value.buffer << "]";
				}
			}
		}
		out << "\n";

		// $TODO output task dependency information
	}

	return !out.fail();
}
#endif // IS_TRUE(OUTPUT_TASK_GRAPH_BUILD_RESULT)
