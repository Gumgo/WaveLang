#include "compiler/try_call_native_module.h"

#include "instrument/instrument_globals.h"

template<typename t_native_module_native_type>
struct s_native_module_native_type_mapping {};

template<>
struct s_native_module_native_type_mapping<c_native_module_real_reference> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_real;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_bool_reference> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_bool;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_string_reference> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_string;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_real_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_real;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_bool_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_bool;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_string_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_string;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_real_reference_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_real;
	using t_element = c_native_module_real_reference;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_bool_reference_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_bool;
	using t_element = c_native_module_bool_reference;
};

template<>
struct s_native_module_native_type_mapping<c_native_module_string_reference_array> {
	static constexpr e_native_module_primitive_type k_primitive_type = e_native_module_primitive_type::k_string;
	using t_element = c_native_module_string_reference;
};

template<typename t_argument_reference>
static t_argument_reference argument_reference_from_node_handle(h_graph_node node_handle);

template<typename t_argument_reference>
static h_graph_node node_handle_from_argument_reference(t_argument_reference argument_reference);

class c_native_module_caller : public c_native_module_diagnostic_interface, public c_native_module_reference_interface {
public:
	c_native_module_caller(
		c_compiler_context &context,
		c_graph_trimmer &graph_trimmer,
		const s_instrument_globals &instrument_globals,
		h_graph_node native_module_call_node_handle,
		int32 input_latency,
		const s_compiler_source_location &call_source_location);

	bool try_call(bool &did_call_out, int32 &output_latency_out, std::vector<h_graph_node> *output_node_handles_out);

	void message(const char *format, ...) override;
	void warning(const char *format, ...) override;
	void error(const char *format, ...) override;

	// Note: these are somewhat broken. If a constant is produced and then assigned to a non-constant output, it will
	// inherit the output latency of the module call but will not get properly delayed. It is really only safe to use
	// these in modules which cannot have any input or output latency (e.g. modules with no input arguments, such as
	// get_sample_rate(), are safe).
	c_native_module_real_reference create_constant_reference(real32 value) override;
	c_native_module_bool_reference create_constant_reference(bool value) override;
	c_native_module_string_reference create_constant_reference(const char *value) override;

private:
	// Converts the node handle into a c_native_module_value_reference and stores whether the referenced node is
	// constant in is_constant_out
	template<typename t_reference>
	t_reference build_reference_value(h_graph_node node_handle, bool &is_constant_out);

	// Validates that the node referenced by argument_reference is of the correct data type and is const if
	// should_be_constant is true. If these conditions are met, the node handle is returned. Otherwise, an error is
	// raised and an invalid node handle is returned - this indicates that the implementation of the native module which
	// provided this node handle is invalid.
	template<typename t_argument_reference>
	h_graph_node validate_and_get_node_handle(
		t_argument_reference argument_reference,
		uint32 upsample_factor,
		bool should_be_constant);

	// Builds an array of constant values and stores the result in array_value_out
	template<typename t_array>
	void build_constant_array_value(h_graph_node array_node_handle, t_array &array_value_out);

	// Builds an array of node handles of the given reference array type.
	template<typename t_reference_array>
	void build_reference_array_value(
		h_graph_node array_node_handle,
		t_reference_array &reference_array_value_out,
		bool &all_elements_constant_out);

	// Builds an array in the native module graph using the provided constant values and returns the resulting node
	// handle
	template<typename t_array>
	h_graph_node build_constant_array_node(const t_array &array_value);

	// Attempts to build an array in the native module graph using the provided node handles. The node handles must be
	// of the correct type and must all point to constant values if should_be_constant is true. If either of these
	// conditions is voilated, an error is raised and an invalid node handle is returned - this means the native
	// module's implementation is invalid. Otherwise, the resulting node handle is returned.
	template<typename t_reference_array>
	h_graph_node try_build_reference_array_node(
		const t_reference_array &reference_array_value,
		uint32 upsample_factor,
		bool should_be_constant);

	const char *get_native_module_name() const;

	// Native module intrinsic implementations:
	void get_latency(const s_native_module_context &context) const;

	c_compiler_context &m_context;
	c_graph_trimmer &m_graph_trimmer;
	const s_instrument_globals &m_instrument_globals;
	h_graph_node m_native_module_call_node_handle;
	int32 m_input_latency;
	const s_compiler_source_location &m_call_source_location;

	bool m_error_issued = false;
	std::vector<h_graph_node> m_created_node_handles;
};

bool try_call_native_module(
	c_compiler_context &context,
	c_graph_trimmer &graph_trimmer,
	const s_instrument_globals &instrument_globals,
	h_graph_node native_module_call_node_handle,
	int32 input_latency,
	const s_compiler_source_location &call_source_location,
	bool &did_call_out,
	int32 &output_latency_out,
	std::vector<h_graph_node> *output_node_handles_out) {
	c_native_module_caller native_module_caller(
		context,
		graph_trimmer,
		instrument_globals,
		native_module_call_node_handle,
		input_latency,
		call_source_location);
	return native_module_caller.try_call(did_call_out, output_latency_out, output_node_handles_out);
}

template<typename t_argument_reference>
static t_argument_reference argument_reference_from_node_handle(h_graph_node node_handle) {
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return t_argument_reference(
		static_cast<uint64>(node_handle.get_data().index | (static_cast<uint64>(node_handle.get_data().salt) << 32)));
#else // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return t_argument_reference(node_handle.get_data().index);
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
}

template<typename t_argument_reference>
static h_graph_node node_handle_from_argument_reference(t_argument_reference argument_reference) {
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return h_graph_node::construct(
		{
			static_cast<uint32>(argument_reference.get_opaque_data()),
			static_cast<uint32>(argument_reference.get_opaque_data() >> 32)
		});
#else // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	return h_graph_node::construct({ argument_reference.get_opaque_data() });
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
}

c_native_module_caller::c_native_module_caller(
	c_compiler_context &context,
	c_graph_trimmer &graph_trimmer,
	const s_instrument_globals &instrument_globals,
	h_graph_node native_module_call_node_handle,
	int32 input_latency,
	const s_compiler_source_location &call_source_location)
	: m_context(context)
	, m_graph_trimmer(graph_trimmer)
	, m_instrument_globals(instrument_globals)
	, m_native_module_call_node_handle(native_module_call_node_handle)
	, m_input_latency(input_latency)
	, m_call_source_location(call_source_location) {}

bool c_native_module_caller::try_call(
	bool &did_call_out,
	int32 &output_latency_out,
	std::vector<h_graph_node> *output_node_handles_out) {
	did_call_out = false;
	output_latency_out = m_input_latency;

	c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	h_native_module native_module_handle =
		native_module_graph.get_native_module_call_node_native_module_handle(m_native_module_call_node_handle);
	const s_native_module &native_module =
		c_native_module_registry::get_native_module(native_module_handle);

	// Early-out
	if (!native_module.compile_time_call
		&& !native_module.validate_arguments
		&& !native_module.get_latency) {
		return true;
	}

	uint32 native_module_upsample_factor =
		native_module_graph.get_native_module_call_node_upsample_factor(m_native_module_call_node_handle);

	// Set up the arguments
	std::vector<s_native_module_compile_time_argument> compile_time_arguments;
	compile_time_arguments.reserve(
		native_module_graph.get_node_incoming_edge_count(m_native_module_call_node_handle)
		+ native_module_graph.get_node_outgoing_edge_count(m_native_module_call_node_handle));

#if IS_TRUE(ASSERTS_ENABLED)
	size_t in_argument_count = 0;
	size_t out_argument_count = 0;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		e_native_module_argument_direction argument_direction =
			native_module.arguments[argument_index].argument_direction;
		if (argument_direction == e_native_module_argument_direction::k_in) {
			in_argument_count++;
		} else {
			wl_assert(argument_direction == e_native_module_argument_direction::k_out);
			out_argument_count++;
		}
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	bool can_call = native_module.compile_time_call != nullptr;
	size_t next_input = 0;
	size_t next_output = 0;
	bool all_dependent_constant_inputs_are_constant = true;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		s_native_module_argument argument = native_module.arguments[argument_index];
		compile_time_arguments.push_back({});
		s_native_module_compile_time_argument &compile_time_argument = compile_time_arguments.back();
#if IS_TRUE(ASSERTS_ENABLED)
		compile_time_argument.argument_direction = argument.argument_direction;
		compile_time_argument.type = argument.type;
		compile_time_argument.data_access = argument.data_access;
#endif // IS_TRUE(ASSERTS_ENABLED)

		if (argument.argument_direction == e_native_module_argument_direction::k_in) {
			h_graph_node source_node_handle = native_module_graph.get_node_indexed_input_incoming_edge_handle(
				m_native_module_call_node_handle,
				next_input,
				0);

			c_native_module_qualified_data_type node_data_type =
				native_module_graph.get_node_data_type(source_node_handle);

			// Ideally we could call is_native_module_data_type_assignable() here, but we haven't determined the
			// dependent-constant data mutability yet, so we have to do the comparison manually to deal with upsample
			// factor correctly.
			wl_assert(node_data_type.get_primitive_type() == argument.type.get_primitive_type());
			wl_assert(node_data_type.is_array() == argument.type.is_array());
			wl_assert(node_data_type.get_data_mutability() == e_native_module_data_mutability::k_constant
				|| node_data_type.get_upsample_factor() ==
				argument.type.get_upsampled_type(native_module_upsample_factor).get_upsample_factor());

			// If the input node is referenced by value but isn't a constant, we can't call this native module
			if (argument.data_access == e_native_module_data_access::k_value
				&& node_data_type.get_data_mutability() != e_native_module_data_mutability::k_constant) {
				can_call = false;
				next_input++;
				continue;
			}

			bool is_constant = true;
			switch (argument.type.get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						c_native_module_real_array &real_array =
							compile_time_argument.value.emplace<c_native_module_real_array>();
						build_constant_array_value(source_node_handle, real_array);
					} else {
						compile_time_argument.value.emplace<real32>(
							native_module_graph.get_constant_node_real_value(source_node_handle));
					}
				} else {
					wl_assert(argument.data_access == e_native_module_data_access::k_reference);
					if (argument.type.is_array()) {
						c_native_module_real_reference_array &real_reference_array =
							compile_time_argument.value.emplace<c_native_module_real_reference_array>();
						build_reference_array_value(source_node_handle, real_reference_array, is_constant);
					} else {
						compile_time_argument.value.emplace<c_native_module_real_reference>(
							build_reference_value<c_native_module_real_reference>(source_node_handle, is_constant));
					}
				}
				break;

			case e_native_module_primitive_type::k_bool:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						c_native_module_bool_array &bool_array =
							compile_time_argument.value.emplace<c_native_module_bool_array>();
						build_constant_array_value(source_node_handle, bool_array);
					} else {
						compile_time_argument.value.emplace<bool>(
							native_module_graph.get_constant_node_bool_value(source_node_handle));
					}
				} else {
					wl_assert(argument.data_access == e_native_module_data_access::k_reference);
					if (argument.type.is_array()) {
						c_native_module_bool_reference_array &bool_reference_array =
							compile_time_argument.value.emplace<c_native_module_bool_reference_array>();
						build_reference_array_value(source_node_handle, bool_reference_array, is_constant);
					} else {
						compile_time_argument.value.emplace<c_native_module_bool_reference>(
							build_reference_value<c_native_module_bool_reference>(source_node_handle, is_constant));
					}
				}
				break;

			case e_native_module_primitive_type::k_string:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						c_native_module_string_array &string_array =
							compile_time_argument.value.emplace<c_native_module_string_array>();
						build_constant_array_value(source_node_handle, string_array);
					} else {
						compile_time_argument.value.emplace<c_native_module_string>().get_string() =
							native_module_graph.get_constant_node_string_value(source_node_handle);
					}
				} else {
					wl_assert(argument.data_access == e_native_module_data_access::k_reference);
					if (argument.type.is_array()) {
						c_native_module_string_reference_array &string_reference_array =
							compile_time_argument.value.emplace<c_native_module_string_reference_array>();
						build_reference_array_value(source_node_handle, string_reference_array, is_constant);
					} else {
						compile_time_argument.value.emplace<c_native_module_string_reference>(
							build_reference_value<c_native_module_string_reference>(source_node_handle, is_constant));
					}
				}
				break;

			default:
				wl_unreachable();
			}

			if (argument.type.get_data_mutability() == e_native_module_data_mutability::k_constant) {
				// Constant arguments should only ever get constant nodes as input (including node handle inputs)
				wl_assert(is_constant);
			} else if (argument.type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant
				&& !is_constant) {
				// If any dependent-constant inputs aren't constant, the output dependent-constants aren't constant
				all_dependent_constant_inputs_are_constant = false;
			}

			next_input++;
		} else {
			wl_assert(argument.argument_direction == e_native_module_argument_direction::k_out);

			switch (argument.type.get_primitive_type()) {
			case e_native_module_primitive_type::k_real:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_real_array>();
					} else {
						compile_time_argument.value.emplace<real32>();
					}
				} else {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_real_reference_array>();
					} else {
						compile_time_argument.value.emplace<c_native_module_real_reference>(
							argument_reference_from_node_handle<c_native_module_real_reference>(
								h_graph_node::invalid()));
					}
				}
				break;

			case e_native_module_primitive_type::k_bool:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_bool_array>();
					} else {
						compile_time_argument.value.emplace<bool>();
					}
				} else {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_bool_reference_array>();
					} else {
						compile_time_argument.value.emplace<c_native_module_bool_reference>(
							argument_reference_from_node_handle<c_native_module_bool_reference>(
								h_graph_node::invalid()));
					}
				}
				break;

			case e_native_module_primitive_type::k_string:
				if (argument.data_access == e_native_module_data_access::k_value) {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_string_array>();
					} else {
						compile_time_argument.value.emplace<c_native_module_string>();
					}
				} else {
					if (argument.type.is_array()) {
						compile_time_argument.value.emplace<c_native_module_string_reference_array>();
					} else {
						compile_time_argument.value.emplace<c_native_module_string_reference>(
							argument_reference_from_node_handle<c_native_module_string_reference>(
								h_graph_node::invalid()));
					}
				}
				break;

			default:
				wl_unreachable();
			}

			next_output++;
		}
	}

	wl_assert(next_input == in_argument_count);
	wl_assert(next_output == out_argument_count);
	wl_assert(next_input + next_output == native_module.argument_count);

	// Make the compile time call to resolve the outputs
	s_native_module_context native_module_context;
	zero_type(&native_module_context);

	native_module_context.diagnostic_interface = this;
	native_module_context.reference_interface = this;
	native_module_context.instrument_globals = &m_instrument_globals;

	native_module_context.upsample_factor = native_module_upsample_factor;
	native_module_context.sample_rate = m_instrument_globals.sample_rate * native_module_upsample_factor;

	h_native_module_library library_handle =
		c_native_module_registry::get_native_module_library_handle(native_module.uid.get_library_id());
	native_module_context.library_context = m_context.get_native_module_library_context(library_handle);

	native_module_context.arguments = c_native_module_compile_time_argument_list(compile_time_arguments);

	// Detect intrinsics with custom implementations
	s_native_module_uid get_latency_real_uid = 
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_real);
	s_native_module_uid get_latency_bool_uid =
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_bool);
	s_native_module_uid get_latency_string_uid =
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_string);
	s_native_module_uid get_latency_real_array_uid =
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_real_array);
	s_native_module_uid get_latency_bool_array_uid =
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_bool_array);
	s_native_module_uid get_latency_string_array_uid =
		c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic::k_get_latency_string_array);

	// Issue the call
	int32 latency = 0;
	if (native_module.uid == get_latency_real_uid
		|| native_module.uid == get_latency_bool_uid
		|| native_module.uid == get_latency_string_uid
		|| native_module.uid == get_latency_real_array_uid
		|| native_module.uid == get_latency_bool_array_uid
		|| native_module.uid == get_latency_string_array_uid) {
		get_latency(native_module_context);
		wl_assert(!m_error_issued);
	} else {
		if (native_module.validate_arguments) {
			native_module.validate_arguments(native_module_context);
			if (m_error_issued) {
				return false;
			}
		}

		if (native_module.get_latency) {
			latency = native_module.get_latency(native_module_context);
			if (m_error_issued) {
				return false;
			}

			if (latency < 0) {
				m_context.error(
					e_compiler_error::k_invalid_native_module_implementation,
					m_call_source_location,
					"Native module '%s' produced negative latency; "
					"the implementation of '%s' is broken, this is not a script error",
					get_native_module_name(),
					get_native_module_name());
				return false;
			}

			output_latency_out = m_input_latency + latency;
		}

		if (!can_call) {
			return true;
		}

		native_module.compile_time_call(native_module_context);
		if (m_error_issued) {
			return false;
		}
	}

	// Now that we've successfully issued the call, assign all the outputs and try trimming the native module call node.
	next_output = 0;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		const s_native_module_argument &argument = native_module.arguments[argument_index];
		if (argument.argument_direction != e_native_module_argument_direction::k_out) {
			continue;
		}

		s_native_module_compile_time_argument &compile_time_argument = compile_time_arguments[argument_index];
		h_graph_node target_node_handle = h_graph_node::invalid();

		// We need to make sure that native modules which output node references (as opposed to values) output
		// references to constant nodes in the required situations
		bool should_be_constant =
			argument.type.get_data_mutability() == e_native_module_data_mutability::k_constant
			|| (argument.type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant
				&& all_dependent_constant_inputs_are_constant);
		uint32 upsample_factor = should_be_constant ? 1 : argument.type.get_upsample_factor();

		switch (argument.type.get_primitive_type()) {
		case e_native_module_primitive_type::k_real:
			if (argument.data_access == e_native_module_data_access::k_value) {
				if (argument.type.is_array()) {
					target_node_handle =
						build_constant_array_node(compile_time_argument.get_real_array_out());
				} else {
					target_node_handle = native_module_graph.add_constant_node(compile_time_argument.get_real_out());
				}
			} else {
				wl_assert(argument.data_access == e_native_module_data_access::k_reference);
				if (argument.type.is_array()) {
					target_node_handle = try_build_reference_array_node(
						compile_time_argument.get_real_reference_array_out(),
						upsample_factor,
						should_be_constant);
				} else {
					target_node_handle = validate_and_get_node_handle(
						compile_time_argument.get_real_reference_out(),
						upsample_factor,
						should_be_constant);
				}

			}
			break;

		case e_native_module_primitive_type::k_bool:
			if (argument.data_access == e_native_module_data_access::k_value) {
				if (argument.type.is_array()) {
					target_node_handle =
						build_constant_array_node(compile_time_argument.get_bool_array_out());
				} else {
					target_node_handle = native_module_graph.add_constant_node(compile_time_argument.get_bool_out());
				}
			} else {
				wl_assert(argument.data_access == e_native_module_data_access::k_reference);
				if (argument.type.is_array()) {
					target_node_handle = try_build_reference_array_node(
						compile_time_argument.get_bool_reference_array_out(),
						upsample_factor,
						should_be_constant);
				} else {
					target_node_handle = validate_and_get_node_handle(
						compile_time_argument.get_bool_reference_out(),
						upsample_factor,
						should_be_constant);
				}
			}
			break;

		case e_native_module_primitive_type::k_string:
			if (argument.data_access == e_native_module_data_access::k_value) {
				if (argument.type.is_array()) {
					target_node_handle =
						build_constant_array_node(compile_time_argument.get_string_array_out());
				} else {
					target_node_handle = native_module_graph.add_constant_node(
						compile_time_argument.get_string_out().get_string().c_str());
				}
			} else {
				wl_assert(argument.data_access == e_native_module_data_access::k_reference);
				if (argument.type.is_array()) {
					target_node_handle = try_build_reference_array_node(
						compile_time_argument.get_string_reference_array_out(),
						upsample_factor,
						should_be_constant);
				} else {
					target_node_handle = validate_and_get_node_handle(
						compile_time_argument.get_string_reference_out(),
						upsample_factor,
						should_be_constant);
				}
			}
			break;

		default:
			wl_unreachable();
		}

		if (!target_node_handle.is_valid()) {
			// An error occurred
			return false;
		}

		// Hook up all outputs with this node instead
		h_graph_node output_node_handle =
			native_module_graph.get_node_outgoing_edge_handle(m_native_module_call_node_handle, next_output);
		while (native_module_graph.get_node_outgoing_edge_count(output_node_handle) > 0) {
			h_graph_node to_node_handle =
				native_module_graph.get_node_outgoing_edge_handle(output_node_handle, 0);
			native_module_graph.remove_edge(output_node_handle, to_node_handle);
			native_module_graph.add_edge(target_node_handle, to_node_handle);
		}

		// Store off the resulting output node
		if (output_node_handles_out) {
			output_node_handles_out->push_back(target_node_handle);
		}

		next_output++;
	}

	wl_assert(next_output == out_argument_count);

	// Remove any newly created but unused constant nodes
	for (h_graph_node constant_node_handle : m_created_node_handles) {
		if (native_module_graph.get_node_outgoing_edge_count(constant_node_handle) == 0) {
			native_module_graph.remove_node(constant_node_handle);
		}
	}

	m_created_node_handles.clear();

	// Finally, remove the native module call entirely
	m_graph_trimmer.try_trim_node(m_native_module_call_node_handle);
	did_call_out = true;
	return true;
}

void c_native_module_caller::message(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = str_vformat(format, args);
	va_end(args);

	m_context.message(
		m_call_source_location,
		"Native module '%s': %s",
		get_native_module_name(),
		message.c_str());
}

void c_native_module_caller::warning(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = str_vformat(format, args);
	va_end(args);

	m_context.warning(
		e_compiler_warning::k_native_module_warning,
		m_call_source_location,
		"Native module '%s': %s",
		get_native_module_name(),
		message.c_str());
}

void c_native_module_caller::error(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = str_vformat(format, args);
	va_end(args);

	m_context.error(
		e_compiler_error::k_native_module_error,
		m_call_source_location,
		"Native module '%s': %s",
		get_native_module_name(),
		message.c_str());
	m_error_issued = true;
}

c_native_module_real_reference c_native_module_caller::create_constant_reference(real32 value) {
	h_graph_node node_handle = m_graph_trimmer.get_native_module_graph().add_constant_node(value);
	m_created_node_handles.push_back(node_handle);
	return argument_reference_from_node_handle<c_native_module_real_reference>(node_handle);
}

c_native_module_bool_reference c_native_module_caller::create_constant_reference(bool value) {
	h_graph_node node_handle = m_graph_trimmer.get_native_module_graph().add_constant_node(value);
	m_created_node_handles.push_back(node_handle);
	return argument_reference_from_node_handle<c_native_module_bool_reference>(node_handle);
}

c_native_module_string_reference c_native_module_caller::create_constant_reference(const char *value) {
	h_graph_node node_handle = m_graph_trimmer.get_native_module_graph().add_constant_node(value);
	m_created_node_handles.push_back(node_handle);
	return argument_reference_from_node_handle<c_native_module_string_reference>(node_handle);
}

template<typename t_reference>
t_reference c_native_module_caller::build_reference_value(
	h_graph_node node_handle,
	bool &is_constant_out) {
	const c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	is_constant_out = native_module_graph.get_node_type(node_handle) == e_native_module_graph_node_type::k_constant;

#if IS_TRUE(ASSERTS_ENABLED)
	static constexpr e_native_module_primitive_type k_primitive_type =
		s_native_module_native_type_mapping<t_reference>::k_primitive_type;
	c_native_module_data_type data_type = native_module_graph.get_node_data_type(node_handle).get_data_type();
	wl_assert(data_type == c_native_module_data_type(k_primitive_type, false, data_type.get_upsample_factor()));
#endif // IS_TRUE(ASSERTS_ENABLED)

	return argument_reference_from_node_handle<t_reference>(node_handle);
}

template<typename t_argument_reference>
h_graph_node c_native_module_caller::validate_and_get_node_handle(
	t_argument_reference argument_reference,
	uint32 upsample_factor,
	bool should_be_constant) {
	wl_assert(upsample_factor == 1 || !should_be_constant);
	h_graph_node node_handle = node_handle_from_argument_reference(argument_reference);
	if (!node_handle.is_valid()) {
		m_context.error(
			e_compiler_error::k_invalid_native_module_implementation,
			m_call_source_location,
			"Native module '%s' produced an invalid reference; "
			"the implementation of '%s' is broken, this is not a script error",
			get_native_module_name(),
			get_native_module_name());
		return h_graph_node();
	}

	static constexpr e_native_module_primitive_type k_primitive_type =
		s_native_module_native_type_mapping<t_argument_reference>::k_primitive_type;

	c_native_module_qualified_data_type expected_data_type(
		c_native_module_data_type(k_primitive_type, false, upsample_factor),
		should_be_constant ? e_native_module_data_mutability::k_constant : e_native_module_data_mutability::k_variable);

	const c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	c_native_module_qualified_data_type node_data_type = native_module_graph.get_node_data_type(node_handle);
	if (!is_native_module_data_type_assignable(node_data_type, expected_data_type)) {
		m_context.error(
			e_compiler_error::k_invalid_native_module_implementation,
			m_call_source_location,
			"Native module '%s' produced a reference of type '%s' but the expected type is '%s'; "
			"the implementation of '%s' is broken, this is not a script error",
			get_native_module_name(),
			node_data_type.to_string().c_str(),
			expected_data_type.to_string().c_str(),
			get_native_module_name());
		return h_graph_node();
	}

	return node_handle;
}

template<typename t_array>
void c_native_module_caller::build_constant_array_value(
	h_graph_node array_node_handle,
	t_array &array_value_out) {
	const c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	size_t element_count = native_module_graph.get_node_outgoing_edge_count(array_node_handle);
	array_value_out.get_array().resize(element_count);
	for (size_t element_index = 0; element_index < element_count; element_index++) {
		h_graph_node element_node_handle = native_module_graph.get_node_indexed_input_incoming_edge_handle(
			array_node_handle,
			element_index,
			0);

		if constexpr (std::is_same_v<t_array, c_native_module_real_array>) {
			array_value_out.get_array()[element_index] =
				native_module_graph.get_constant_node_real_value(element_node_handle);
		} else if constexpr (std::is_same_v<t_array, c_native_module_bool_array>) {
			array_value_out.get_array()[element_index] =
				native_module_graph.get_constant_node_bool_value(element_node_handle);
		} else if constexpr (std::is_same_v<t_array, c_native_module_string_array>) {
			array_value_out.get_array()[element_index] =
				native_module_graph.get_constant_node_string_value(element_node_handle);
		} else {
			STATIC_UNREACHABLE();
		}
	}
}

template<typename t_reference_array>
void c_native_module_caller::build_reference_array_value(
	h_graph_node array_node_handle,
	t_reference_array &reference_array_value_out,
	bool &all_elements_constant_out) {
	const c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	size_t element_count = native_module_graph.get_node_incoming_edge_count(array_node_handle);
	reference_array_value_out.get_array().resize(element_count);
	all_elements_constant_out = true;
	for (size_t element_index = 0; element_index < element_count; element_index++) {
		h_graph_node element_node_handle = native_module_graph.get_node_indexed_input_incoming_edge_handle(
			array_node_handle,
			element_index,
			0);

		using t_reference = typename s_native_module_native_type_mapping<t_reference_array>::t_element;

		bool element_is_constant;
		reference_array_value_out.get_array()[element_index] =
			build_reference_value<t_reference>(element_node_handle, element_is_constant);

		all_elements_constant_out &= element_is_constant;
	}
}

template<typename t_array>
h_graph_node c_native_module_caller::build_constant_array_node(const t_array &array_value) {
	c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	static constexpr e_native_module_primitive_type k_primitive_type =
		s_native_module_native_type_mapping<t_array>::k_primitive_type;

	h_graph_node array_node_handle = native_module_graph.add_array_node(k_primitive_type);
	for (size_t index = 0; index < array_value.get_array().size(); index++) {
		if constexpr (std::is_same_v<t_array, c_native_module_string_array>) {
			native_module_graph.add_array_value(
				array_node_handle,
				native_module_graph.add_constant_node(array_value.get_array()[index].c_str()));
		} else {
			native_module_graph.add_array_value(
				array_node_handle,
				native_module_graph.add_constant_node(array_value.get_array()[index]));
		}
	}

	return array_node_handle;
}

template<typename t_reference_array>
h_graph_node c_native_module_caller::try_build_reference_array_node(
	const t_reference_array &reference_array_value,
	uint32 upsample_factor,
	bool should_be_constant) {
	wl_assert(upsample_factor == 1 || !should_be_constant);
	c_native_module_graph &native_module_graph = m_graph_trimmer.get_native_module_graph();
	static constexpr e_native_module_primitive_type k_primitive_type =
		s_native_module_native_type_mapping<t_reference_array>::k_primitive_type;

	h_graph_node array_node_handle = native_module_graph.add_array_node(k_primitive_type);
	for (size_t index = 0; index < reference_array_value.get_array().size(); index++) {
		h_graph_node node_handle =
			validate_and_get_node_handle(reference_array_value.get_array()[index], upsample_factor, should_be_constant);
		if (!node_handle.is_valid()) {
			// No need to report an error, it was already raised in validate_and_get_node_handle()
			return h_graph_node();
		}

		native_module_graph.add_array_value(array_node_handle, node_handle);
	}

	return array_node_handle;
}

const char *c_native_module_caller::get_native_module_name() const {
	h_native_module native_module_handle =
		m_graph_trimmer.get_native_module_graph().get_native_module_call_node_native_module_handle(
			m_native_module_call_node_handle);
	const s_native_module &native_module =
		c_native_module_registry::get_native_module(native_module_handle);
	return native_module.name.get_string();
}

void c_native_module_caller::get_latency(const s_native_module_context &context) const {
	wl_assert(context.arguments.get_count() == 2);
	context.arguments[1].get_real_out() = static_cast<real32>(m_input_latency);
}
