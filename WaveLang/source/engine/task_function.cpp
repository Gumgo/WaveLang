#include "engine/task_function.h"

const s_task_function_uid s_task_function_uid::k_invalid = s_task_function_uid::build(0xffffffff, 0xffffffff);

s_task_function_argument_list s_task_function_argument_list::build(
	e_task_data_type arg_0,
	e_task_data_type arg_1,
	e_task_data_type arg_2,
	e_task_data_type arg_3,
	e_task_data_type arg_4,
	e_task_data_type arg_5,
	e_task_data_type arg_6,
	e_task_data_type arg_7,
	e_task_data_type arg_8,
	e_task_data_type arg_9) {
	static_assert(k_max_task_function_arguments == 10, "Max task function arguments changed");
	s_task_function_argument_list result = {
		arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9
	};
	return result;
}

s_task_function s_task_function::build(
	s_task_function_uid uid,
	f_task_memory_query memory_query,
	f_task_initializer initializer,
	f_task_function function,
	const s_task_function_argument_list &arguments) {
	// First, zero-initialize
	s_task_function task_function;
	ZERO_STRUCT(&task_function);

	task_function.uid = uid;

	wl_assert(function);

	task_function.memory_query = memory_query;
	task_function.initializer = initializer;
	task_function.function = function;

	for (task_function.argument_count = 0;
		 task_function.argument_count < k_max_task_function_arguments;
		 task_function.argument_count++) {
		e_task_data_type arg = arguments.arguments[task_function.argument_count];
		// Loop until we find an k_task_data_type_count
		if (arg != k_task_data_type_count) {
			wl_assert(VALID_INDEX(arg, k_task_data_type_count));
			task_function.argument_types[task_function.argument_count] = arg;
		} else {
			break;
		}
	}

	return task_function;
}

s_task_function_native_module_argument_mapping s_task_function_native_module_argument_mapping::build(
	uint32 arg_0,
	uint32 arg_1,
	uint32 arg_2,
	uint32 arg_3,
	uint32 arg_4,
	uint32 arg_5,
	uint32 arg_6,
	uint32 arg_7,
	uint32 arg_8,
	uint32 arg_9) {
	static_assert(k_max_native_module_arguments == 10, "Max native module arguments changed");
	s_task_function_native_module_argument_mapping result = {
		arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9
	};
	return result;
}

s_task_function_mapping s_task_function_mapping::build(
	s_task_function_uid task_function_uid,
	const char *input_pattern,
	const s_task_function_native_module_argument_mapping &mapping) {
	static_assert(k_max_native_module_arguments == 10, "Max native module arguments changed");

	// First, zero-initialize
	s_task_function_mapping task_function_mapping;
	ZERO_STRUCT(&task_function_mapping);

	task_function_mapping.task_function_uid = task_function_uid;

	wl_assert(strlen(input_pattern) <= k_max_native_module_arguments);
	uint32 index = 0;
	for (const char *c = input_pattern; *c != '\0'; c++, index++) {
		char ch = *c;

		// Convert characters
		switch (ch) {
		case 'v':
			task_function_mapping.native_module_input_types[index] =
				k_task_function_mapping_native_module_input_type_variable;
			break;

		case 'b':
			task_function_mapping.native_module_input_types[index] =
				k_task_function_mapping_native_module_input_type_branchless_variable;
			break;

		case '.':
			task_function_mapping.native_module_input_types[index] =
				k_task_function_mapping_native_module_input_type_none;
			break;

		default:
			// Invalid character
			wl_unreachable();
		}
	}

	// Validate that the two provided arrays have the same amount of data
#if PREDEFINED(ASSERTS_ENABLED)
	for (uint32 verify_index = 0; verify_index < k_max_native_module_arguments; verify_index++) {
		// Arguments must not be "none" before index, and must be "none" after it
		wl_assert(
			(verify_index < index) ==
			(mapping.mapping[verify_index] != s_task_function_native_module_argument_mapping::k_argument_none));
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Fill the remaining pattern with an invalid value
	for (; index < k_max_native_module_arguments; index++) {
		task_function_mapping.native_module_input_types[index] = k_task_function_mapping_native_module_input_type_count;
	}

	task_function_mapping.native_module_argument_mapping = mapping;

	return task_function_mapping;
}