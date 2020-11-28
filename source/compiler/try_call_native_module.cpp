#include "compiler/try_call_native_module.h"

#include "execution_graph/diagnostic/diagnostic.h"
#include "execution_graph/node_interface/node_interface.h"

#include <vector>

class c_diagnostic_context {
public:
	c_diagnostic_context(
		c_compiler_context &context,
		const char *native_module_name,
		const s_compiler_source_location &source_location)
		: m_context(context)
		, m_native_module_name(native_module_name)
		, m_source_location(source_location) {}

	bool was_error_issued() const {
		return m_error_issued;
	}

	static void issue_diagnostic(void *context, e_diagnostic_level diagnostic_level, const char *message) {
		static_cast<c_diagnostic_context *>(context)->issue_diagnostic(diagnostic_level, message);
	}

private:
	void issue_diagnostic(e_diagnostic_level diagnostic_level, const char *message) {
		switch (diagnostic_level) {
		case e_diagnostic_level::k_message:
			m_context.message(
				m_source_location,
				"Native module '%s': %s",
				m_native_module_name,
				message);
			break;

		case e_diagnostic_level::k_warning:
			m_context.warning(
				e_compiler_warning::k_native_module_warning,
				m_source_location,
				"Native module '%s': %s",
				m_native_module_name,
				message);
			break;

		case e_diagnostic_level::k_error:
			m_error_issued = true;
			m_context.error(
				e_compiler_error::k_native_module_error,
				m_source_location,
				"Native module '%s': %s",
				m_native_module_name,
				message);
			break;

		default:
			wl_unreachable();
		}
	}

	c_compiler_context &m_context;
	const char *m_native_module_name;
	const s_compiler_source_location &m_source_location;
	bool m_error_issued = false;
};

static void build_array_value(
	const c_execution_graph *execution_graph,
	c_node_reference array_node_reference,
	c_native_module_array &array_value_out);
static bool perform_array_subscript(
	c_compiler_context &context,
	s_native_module_context &native_module_context,
	const s_compiler_source_location &source_location,
	c_node_reference &element_node_reference_out);

bool try_call_native_module(
	c_compiler_context &context,
	c_graph_trimmer &graph_trimmer,
	const s_instrument_globals &instrument_globals,
	c_node_reference native_module_call_node_reference,
	const s_compiler_source_location &call_source_location,
	bool &did_call_out) {
	did_call_out = false;

	c_execution_graph &execution_graph = graph_trimmer.get_execution_graph();
	h_native_module native_module_handle =
		execution_graph.get_native_module_call_native_module_handle(native_module_call_node_reference);
	const s_native_module &native_module =
		c_native_module_registry::get_native_module(native_module_handle);

	if (!native_module.compile_time_call) {
		return true;
	}

	size_t input_count = execution_graph.get_node_incoming_edge_count(native_module_call_node_reference);
	for (size_t input_index = 0; input_index < input_count; input_index++) {
		c_node_reference source_node_reference = execution_graph.get_node_indexed_input_incoming_edge_reference(
			native_module_call_node_reference,
			input_index,
			0);

		// $TODO $COMPILER Check for arrays explicitly here too when we have a k_array type
		if (execution_graph.get_node_type(source_node_reference) != e_execution_graph_node_type::k_constant) {
			return true;
		}
	}

	// Set up the arguments
	std::vector<s_native_module_compile_time_argument> compile_time_arguments;
	compile_time_arguments.reserve(
		execution_graph.get_node_incoming_edge_count(native_module_call_node_reference)
		+ execution_graph.get_node_outgoing_edge_count(native_module_call_node_reference));

	size_t next_input = 0;
	size_t next_output = 0;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		s_native_module_argument argument = native_module.arguments[argument_index];
		compile_time_arguments.push_back({});
		s_native_module_compile_time_argument &compile_time_argument = compile_time_arguments.back();
#if IS_TRUE(ASSERTS_ENABLED)
		compile_time_argument.type = argument.type;
#endif // IS_TRUE(ASSERTS_ENABLED)

		if (native_module_qualifier_is_input(argument.type.get_qualifier())) {
			c_node_reference source_node_reference = execution_graph.get_node_indexed_input_incoming_edge_reference(
				native_module_call_node_reference,
				next_input,
				0);
			c_native_module_data_type type = execution_graph.get_constant_node_data_type(source_node_reference);

			if (type.is_array()) {
				build_array_value(execution_graph, source_node_reference, compile_time_argument.array_value);
			} else {
				switch (type.get_primitive_type()) {
				case e_native_module_primitive_type::k_real:
					compile_time_argument.real_value =
						execution_graph.get_constant_node_real_value(source_node_reference);
					break;

				case e_native_module_primitive_type::k_bool:
					compile_time_argument.bool_value =
						execution_graph.get_constant_node_bool_value(source_node_reference);
					break;

				case e_native_module_primitive_type::k_string:
					compile_time_argument.string_value.get_string() =
						execution_graph.get_constant_node_string_value(source_node_reference);
					break;

				default:
					wl_unreachable();
				}
			}

			next_input++;
		} else {
			wl_assert(argument.type.get_qualifier() == e_native_module_qualifier::k_out);
			compile_time_argument.real_value = 0.0f;
			next_output++;
		}
	}

	wl_assert(next_input == native_module.in_argument_count);
	wl_assert(next_output == native_module.out_argument_count);

	// Make the compile time call to resolve the outputs
	s_native_module_context native_module_context;
	zero_type(&native_module_context);

	c_diagnostic_context diagnostic_context(context, native_module.name.get_string(), call_source_location);
	c_diagnostic diagnostic(c_diagnostic_context::issue_diagnostic, &diagnostic_context);
	native_module_context.diagnostic = &diagnostic;

	c_node_interface node_interface(&execution_graph);
	native_module_context.node_interface = &node_interface;

	native_module_context.instrument_globals = &instrument_globals;

	h_native_module_library library_handle =
		c_native_module_registry::get_native_module_library_handle(native_module.uid.get_library_id());
	native_module_context.library_context = context.get_native_module_library_context(library_handle);

	c_native_module_compile_time_argument_list argument_list(compile_time_arguments);
	native_module_context.arguments = &argument_list;

	// Hack: currently, array subscript cannot be implemented in a native module because if the array is non-const, then
	// the result should be a non-const node reference. We currently have the ability to construct arrays of non-const
	// nodes inside of native modules but not individual values. To work around this, detect the array subscript case
	// and handle it manually.
	c_node_reference element_node_reference;
	if (c_native_module_registry::get_native_module_operator(native_module.uid) == e_native_operator::k_subscript) {
		if (!perform_array_subscript(context, native_module_context, call_source_location, element_node_reference)) {
			return false;
		}
	} else {
		native_module.compile_time_call(native_module_context);
		if (diagnostic_context.was_error_issued()) {
			return false;
		}
	}

	// Now that we've successfully (or not) issued the call, assign all the outputs and try trimming the native module
	// call node.
	next_output = 0;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		const s_native_module_argument &argument = native_module.arguments[argument_index];
		if (argument.type.get_qualifier() != e_native_module_qualifier::k_out) {
			continue;
		}

		const s_native_module_compile_time_argument &compile_time_argument = compile_time_arguments[argument_index];

		// Create a constant node for this output
		c_node_reference target_node_reference = c_node_reference();
		const c_native_module_array *argument_array = nullptr;

		if (element_node_reference.is_valid()) {
			// Hack: handle array subscript specifically
			wl_assert(argument_index == 2);
			target_node_reference = element_node_reference;
		} else if (argument.type.get_data_type().is_array()) {
			target_node_reference =
				execution_graph.add_constant_array_node(argument.type.get_data_type().get_element_type());
			argument_array = &compile_time_argument.array_value;
		} else {
			switch (argument.type.get_data_type().get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				target_node_reference =
					execution_graph.add_constant_node(compile_time_argument.real_value);
				break;

			case e_native_module_primitive_type::k_bool:
				target_node_reference =
					execution_graph.add_constant_node(compile_time_argument.bool_value);
				break;

			case e_native_module_primitive_type::k_string:
				target_node_reference =
					execution_graph.add_constant_node(compile_time_argument.string_value.get_string());
				break;

			default:
				wl_unreachable();
			}
		}

		if (argument_array) {
			// Hook up array inputs
			for (size_t index = 0; index < argument_array->get_array().size(); index++) {
				execution_graph.add_constant_array_value(target_node_reference, argument_array->get_array()[index]);
			}
		}

		// Hook up all outputs with this node instead
		c_node_reference output_node_reference =
			execution_graph.get_node_outgoing_edge_reference(native_module_call_node_reference, next_output);
		while (execution_graph.get_node_outgoing_edge_count(output_node_reference) > 0) {
			c_node_reference to_node_reference =
				execution_graph.get_node_outgoing_edge_reference(output_node_reference, 0);
			execution_graph.remove_edge(output_node_reference, to_node_reference);
			execution_graph.add_edge(target_node_reference, to_node_reference);
		}

		next_output++;
	}

	wl_assert(next_output == native_module.out_argument_count);

	// Remove any newly created but unused constant nodes
	for (c_node_reference constant_node_reference : node_interface.get_created_node_references()) {
		if (execution_graph.get_node_outgoing_edge_count(constant_node_reference) == 0) {
			execution_graph.remove_node(constant_node_reference);
		}
	}

	// Finally, remove the native module call entirely
	graph_trimmer.try_trim_node(native_module_call_node_reference);
	return true;
}

static void build_array_value(
	const c_execution_graph &execution_graph,
	c_node_reference array_node_reference,
	c_native_module_array &array_value_out) {
	// $TODO $COMPILER Should be k_array maybe
	wl_assert(execution_graph.get_node_type(array_node_reference) == e_execution_graph_node_type::k_constant);
	wl_assert(execution_graph.get_constant_node_data_type(array_node_reference).is_array());
	wl_assert(array_value_out.get_array().empty());

	size_t array_count = execution_graph.get_node_incoming_edge_count(array_node_reference);
	array_value_out.get_array().reserve(array_count);

	for (size_t array_index = 0; array_index < array_count; array_index++) {
		// Jump past the indexed input node
		c_node_reference value_node_reference =
			execution_graph.get_node_indexed_input_incoming_edge_reference(array_node_reference, array_index, 0);
		array_value_out.get_array().push_back(value_node_reference);
	}
}

static bool perform_array_subscript(
	c_compiler_context &context,
	s_native_module_context &native_module_context,
	const s_compiler_source_location &source_location,
	c_node_reference &element_node_reference_out) {
	wl_assert(native_module_context.arguments->get_count() == 3);

	std::vector<c_node_reference> &array_node_references =
		(*native_module_context.arguments)[0].array_value.get_array();

	size_t array_count = array_node_references.size();
	real32 array_index_real = (*native_module_context.arguments)[1].get_real_in();
	size_t array_index;
	if (!get_and_validate_native_module_array_index(array_index_real, array_count, array_index)) {
		context.error(
			e_compiler_error::k_array_index_out_of_bounds,
			source_location,
			"Array index '%f' is out of bounds for array of size '%llu'",
			array_index_real,
			array_count);
		return false;
	}

	element_node_reference_out = array_node_references[array_index];
	return true;
}
