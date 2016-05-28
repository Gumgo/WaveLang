#include "execution_graph/native_module.h"

const s_native_module_uid s_native_module_uid::k_invalid = s_native_module_uid::build(0xffffffff, 0xffffffff);

// The invalid qualifier value signifies "none"
const s_native_module_named_argument s_native_module_argument_list::k_argument_none = {
	s_native_module_argument::build(k_native_module_qualifier_count, c_native_module_data_type::invalid()),
	c_static_string<k_max_native_module_argument_name_length>::construct_empty()
};

static const s_native_module_primitive_type_traits k_native_module_primitive_type_traits[] = {
	//	is_dynamic
	{	true	},	// real
	{	false	},	// bool
	{	false	}	// string
};

static_assert(NUMBEROF(k_native_module_primitive_type_traits) == k_native_module_primitive_type_count,
	"Primitive type traits mismatch");

struct s_native_module_data_type_format {
	union {
		uint32 data;

		struct {
			uint16 primitive_type;
			uint16 flags;
		};
	};
};

c_native_module_data_type::c_native_module_data_type() {
	m_primitive_type = k_native_module_primitive_type_real;
	m_flags = 0;
}

c_native_module_data_type::c_native_module_data_type(e_native_module_primitive_type primitive_type, bool is_array) {
	wl_assert(VALID_INDEX(primitive_type, k_native_module_primitive_type_count));
	m_primitive_type = primitive_type;
	m_flags = 0;

	set_bit<k_flag_is_array>(m_flags, is_array);
}

c_native_module_data_type c_native_module_data_type::invalid() {
	// Set all to 0xffffffff
	c_native_module_data_type result;
	result.m_primitive_type = static_cast<e_native_module_primitive_type>(-1);
	result.m_flags = static_cast<uint32>(-1);
	return result;
}

bool c_native_module_data_type::is_valid() const {
	return (m_primitive_type != static_cast<e_native_module_primitive_type>(-1));
}

e_native_module_primitive_type c_native_module_data_type::get_primitive_type() const {
	wl_assert(is_valid());
	return m_primitive_type;
}

const s_native_module_primitive_type_traits &c_native_module_data_type::get_primitive_type_traits() const {
	wl_assert(is_valid());
	return get_native_module_primitive_type_traits(m_primitive_type);
}

bool c_native_module_data_type::is_array() const {
	wl_assert(is_valid());
	return test_bit<k_flag_is_array>(m_flags);
}

c_native_module_data_type c_native_module_data_type::get_element_type() const {
	wl_assert(is_valid());
	wl_assert(is_array());
	return c_native_module_data_type(m_primitive_type);
}

c_native_module_data_type c_native_module_data_type::get_array_type() const {
	wl_assert(is_valid());
	wl_assert(!is_array());
	return c_native_module_data_type(m_primitive_type, true);
}

bool c_native_module_data_type::operator==(const c_native_module_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type) && (m_flags == other.m_flags);
}

bool c_native_module_data_type::operator!=(const c_native_module_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type) || (m_flags != other.m_flags);
}

uint32 c_native_module_data_type::write() const {
	// $TODO endianness?
	wl_assert(is_valid()); // Invalid type is only used as a sentinel in a few spots, should never write it
	s_native_module_data_type_format format;
	format.primitive_type = static_cast<uint16>(m_primitive_type);
	format.flags = static_cast<uint16>(m_flags);
	return format.data;
}

bool c_native_module_data_type::read(uint32 data) {
	// $TODO endianness?
	s_native_module_data_type_format format;
	format.data = data;

	if (!VALID_INDEX(format.primitive_type, k_native_module_primitive_type_count)) {
		return false;
	}

	// This is checking for out-of-range flags
	if (format.flags >= (1 << k_flag_count)) {
		return false;
	}

	m_primitive_type = static_cast<e_native_module_primitive_type>(format.primitive_type);
	m_flags = format.flags;
	return true;
}

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type) {
	wl_assert(VALID_INDEX(primitive_type, k_native_module_primitive_type_count));
	return k_native_module_primitive_type_traits[primitive_type];
}

s_native_module_argument_list s_native_module_argument_list::build(
	s_native_module_named_argument arg_0,
	s_native_module_named_argument arg_1,
	s_native_module_named_argument arg_2,
	s_native_module_named_argument arg_3,
	s_native_module_named_argument arg_4,
	s_native_module_named_argument arg_5,
	s_native_module_named_argument arg_6,
	s_native_module_named_argument arg_7,
	s_native_module_named_argument arg_8,
	s_native_module_named_argument arg_9) {
	static_assert(k_max_native_module_arguments == 10, "Max native module arguments changed");
	s_native_module_argument_list result;
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
		const s_native_module_named_argument &arg = arguments.arguments[native_module.argument_count];
		// Loop until we find an k_native_module_qualifier_count
		if (arg.argument.qualifier != k_native_module_qualifier_count) {
			wl_assert(VALID_INDEX(arg.argument.qualifier, k_native_module_qualifier_count));
			wl_assert(arg.argument.type.is_valid());
			native_module.arguments[native_module.argument_count] = arg.argument;
			arg.name.validate();
			native_module.argument_names[native_module.argument_count].set_verify(arg.name.get_string());

			if (native_module_qualifier_is_input(arg.argument.qualifier)) {
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
