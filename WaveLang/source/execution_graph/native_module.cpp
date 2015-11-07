#include "execution_graph/native_module.h"

const s_native_module_uid s_native_module_uid::k_invalid = s_native_module_uid::build(0xffffffff, 0xffffffff);

const s_native_module_argument s_native_module_argument_list::k_argument_none = {
	k_native_module_argument_qualifier_count, k_native_module_argument_type_count
};

bool is_native_module_argument_type_array(e_native_module_argument_type type) {
	switch (type) {
	case k_native_module_argument_type_real:
	case k_native_module_argument_type_bool:
	case k_native_module_argument_type_string:
		return false;

	case k_native_module_argument_type_real_array:
	case k_native_module_argument_type_bool_array:
	case k_native_module_argument_type_string_array:
		return true;

	default:
		wl_unreachable();
		return false;
	}
}

e_native_module_argument_type get_element_from_array_native_module_argument_type(
	e_native_module_argument_type array_type) {
	switch (array_type) {
	case k_native_module_argument_type_real_array:
		return k_native_module_argument_type_real;

	case k_native_module_argument_type_bool_array:
		return k_native_module_argument_type_bool;

	case k_native_module_argument_type_string_array:
		return k_native_module_argument_type_string;

	default:
		wl_unreachable();
		return k_native_module_argument_type_count;
	}
}

e_native_module_argument_type get_array_from_element_native_module_argument_type(
	e_native_module_argument_type element_type) {
	switch (element_type) {
	case k_native_module_argument_type_real:
		return k_native_module_argument_type_real_array;

	case k_native_module_argument_type_bool:
		return k_native_module_argument_type_bool_array;

	case k_native_module_argument_type_string:
		return k_native_module_argument_type_string_array;

	default:
		wl_unreachable();
		return k_native_module_argument_type_count;
	}
}

s_native_module_argument_list s_native_module_argument_list::build(
	s_native_module_argument arg_0,
	s_native_module_argument arg_1,
	s_native_module_argument arg_2,
	s_native_module_argument arg_3,
	s_native_module_argument arg_4,
	s_native_module_argument arg_5,
	s_native_module_argument arg_6,
	s_native_module_argument arg_7,
	s_native_module_argument arg_8,
	s_native_module_argument arg_9) {
	static_assert(k_max_native_module_arguments == 10, "Max native module arguments changed");
	s_native_module_argument_list result = {
		arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9
	};
	return result;
}

s_native_module s_native_module::build(
	s_native_module_uid uid,
	const char *name,
	bool first_output_is_return,
	const s_native_module_argument_list &arguments,
	f_native_module_compile_time_call compile_time_call) {
	// First, zero-initialize
	s_native_module native_module;
	ZERO_STRUCT(&native_module);

	native_module.uid = uid;

	// Copy the name with size assert validation
	native_module.name.set_verify(name);

	native_module.first_output_is_return = first_output_is_return;
	native_module.in_argument_count = 0;
	native_module.out_argument_count = 0;
	for (native_module.argument_count = 0;
		 native_module.argument_count < k_max_native_module_arguments;
		 native_module.argument_count++) {
		s_native_module_argument arg = arguments.arguments[native_module.argument_count];
		// Loop until we find an k_native_module_argument_qualifier_count
		if (arg.qualifier != k_native_module_argument_qualifier_count) {
			wl_assert(VALID_INDEX(arg.qualifier, k_native_module_argument_qualifier_count));
			wl_assert(VALID_INDEX(arg.type, k_native_module_argument_type_count));
			native_module.arguments[native_module.argument_count] = arg;

			if (arg.qualifier == k_native_module_argument_qualifier_in ||
				arg.qualifier == k_native_module_argument_qualifier_constant) {
				native_module.in_argument_count++;
			} else {
				native_module.out_argument_count++;
			}
		} else {
			break;
		}
	}

	// If the first output is the return value, we must have at least one output
	wl_assert(!first_output_is_return || native_module.out_argument_count > 0);

	native_module.compile_time_call = compile_time_call;

	return native_module;
}
