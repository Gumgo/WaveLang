#include "engine/task_function.h"

const s_task_function_uid s_task_function_uid::k_invalid = s_task_function_uid::build(0xffffffff, 0xffffffff);

// The invalid qualifier value signifies "none"
const c_task_data_type s_task_function_argument_list::k_argument_none = c_task_data_type::invalid();

static const s_task_primitive_type_traits k_task_primitive_type_traits[] = {
	//	is_dynamic
	{ true },	// real
	{ true },	// bool
	{ false }	// string
};

static_assert(NUMBEROF(k_task_primitive_type_traits) == k_task_primitive_type_count,
	"Primitive type traits mismatch");

c_task_data_type::c_task_data_type() {
	m_primitive_type = k_task_primitive_type_real;
	m_qualifier = k_task_qualifier_in;
	m_flags = 0;
}

c_task_data_type::c_task_data_type(e_task_primitive_type primitive_type, e_task_qualifier qualifier, bool is_array) {
	wl_assert(VALID_INDEX(primitive_type, k_task_primitive_type_count));
	wl_assert(VALID_INDEX(qualifier, k_task_qualifier_count));
	m_primitive_type = primitive_type;
	m_qualifier = qualifier;
	m_flags = 0;

	set_bit<k_flag_is_array>(m_flags, is_array);
}

c_task_data_type c_task_data_type::invalid() {
	// Set all to 0xffffffff
	c_task_data_type result;
	result.m_primitive_type = static_cast<e_task_primitive_type>(-1);
	result.m_qualifier = static_cast<e_task_qualifier>(-1);
	result.m_flags = static_cast<uint32>(-1);
	return result;
}

bool c_task_data_type::is_valid() const {
	return (m_primitive_type != static_cast<e_task_primitive_type>(-1));
}

e_task_primitive_type c_task_data_type::get_primitive_type() const {
	wl_assert(is_valid());
	return m_primitive_type;
}

e_task_qualifier c_task_data_type::get_qualifier() const {
	wl_assert(is_valid());
	return m_qualifier;
}

const s_task_primitive_type_traits &c_task_data_type::get_primitive_type_traits() const {
	wl_assert(is_valid());
	return get_task_primitive_type_traits(m_primitive_type);
}

bool c_task_data_type::is_array() const {
	wl_assert(is_valid());
	return test_bit<k_flag_is_array>(m_flags);
}

c_task_data_type c_task_data_type::get_element_type() const {
	wl_assert(is_valid());
	wl_assert(is_array());
	return c_task_data_type(m_primitive_type, m_qualifier);
}

c_task_data_type c_task_data_type::get_array_type() const {
	wl_assert(is_valid());
	wl_assert(!is_array());
	return c_task_data_type(m_primitive_type, m_qualifier, true);
}

c_task_data_type c_task_data_type::get_with_qualifier(e_task_qualifier qualifier) const {
	wl_assert(is_valid());
	return c_task_data_type(m_primitive_type, qualifier, is_array());
}

bool c_task_data_type::operator==(const c_task_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type) &&
		(m_qualifier == other.m_qualifier) &&
		(m_flags == other.m_flags);
}

bool c_task_data_type::operator!=(const c_task_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type) ||
		(m_qualifier != other.m_qualifier) ||
		(m_flags != other.m_flags);
}

bool c_task_data_type::is_legal() const {
	wl_assert(is_valid());

	if (!get_primitive_type_traits().is_dynamic && m_qualifier != k_task_qualifier_in) {
		// Static-only types must only be used as inputs, e.g. a task cannot output a new string
		return false;
	}

	if (is_array() && m_qualifier != k_task_qualifier_in) {
		// Arrays cannot be outputs
		return false;
	}

	return true;
}

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type) {
	wl_assert(VALID_INDEX(primitive_type, k_task_primitive_type_count));
	return k_task_primitive_type_traits[primitive_type];
}

s_task_function_argument_list s_task_function_argument_list::build(
	c_task_data_type arg_0,
	c_task_data_type arg_1,
	c_task_data_type arg_2,
	c_task_data_type arg_3,
	c_task_data_type arg_4,
	c_task_data_type arg_5,
	c_task_data_type arg_6,
	c_task_data_type arg_7,
	c_task_data_type arg_8,
	c_task_data_type arg_9) {
	static_assert(k_max_task_function_arguments == 10, "Max task function arguments changed");
	s_task_function_argument_list result;
	result.arguments[0] = arg_0;
	result.arguments[1] = arg_1;
	result.arguments[2] = arg_2;
	result.arguments[3] = arg_3;
	result.arguments[4] = arg_4;
	result.arguments[5] = arg_5;
	result.arguments[6] = arg_6;
	result.arguments[7] = arg_7;
	result.arguments[8] = arg_8;
	result.arguments[9] = arg_9;
	return result;
}

s_task_function s_task_function::build(
	s_task_function_uid uid,
	const char *name,
	f_task_memory_query memory_query,
	f_task_initializer initializer,
	f_task_voice_initializer voice_initializer,
	f_task_function function,
	const s_task_function_argument_list &arguments) {
	// First, zero-initialize
	s_task_function task_function;
	ZERO_STRUCT(&task_function);

	task_function.uid = uid;

	// Copy the name with size assert validation
	task_function.name.set_verify(name);

	wl_assert(function);

	task_function.memory_query = memory_query;
	task_function.initializer = initializer;
	task_function.voice_initializer = voice_initializer;
	task_function.function = function;

	for (task_function.argument_count = 0;
		 task_function.argument_count < k_max_task_function_arguments;
		 task_function.argument_count++) {
		c_task_data_type arg = arguments.arguments[task_function.argument_count];
		// Loop until we find an invalid argument
		if (arg.is_valid()) {
			wl_assert(arg.is_legal());
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
	s_task_function_native_module_argument_mapping result;
	result.mapping[0] = arg_0;
	result.mapping[1] = arg_1;
	result.mapping[2] = arg_2;
	result.mapping[3] = arg_3;
	result.mapping[4] = arg_4;
	result.mapping[5] = arg_5;
	result.mapping[6] = arg_6;
	result.mapping[7] = arg_7;
	result.mapping[8] = arg_8;
	result.mapping[9] = arg_9;
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
#if IS_TRUE(ASSERTS_ENABLED)
	for (uint32 verify_index = 0; verify_index < k_max_native_module_arguments; verify_index++) {
		// Arguments must not be "none" before index, and must be "none" after it
		wl_assert(
			(verify_index < index) ==
			(mapping.mapping[verify_index] != s_task_function_native_module_argument_mapping::k_argument_none));
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Fill the remaining pattern with an invalid value
	for (; index < k_max_native_module_arguments; index++) {
		task_function_mapping.native_module_input_types[index] = k_task_function_mapping_native_module_input_type_count;
	}

	task_function_mapping.native_module_argument_mapping = mapping;

	return task_function_mapping;
}
