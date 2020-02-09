#include "execution_graph/native_module.h"

const s_native_module_uid s_native_module_uid::k_invalid = s_native_module_uid::build(0xffffffff, 0xffffffff);

static const s_native_module_primitive_type_traits k_native_module_primitive_type_traits[] = {
	//	is_dynamic
	{	true	},	// real
	{	true	},	// bool
	{	false	}	// string
};

static_assert(NUMBEROF(k_native_module_primitive_type_traits) == enum_count<e_native_module_primitive_type>(),
	"Primitive type traits mismatch");

static const char *k_native_operator_names[] = {
	"operator_u+",
	"operator_u-",
	"operator_+",
	"operator_-",
	"operator_*",
	"operator_/",
	"operator_%",
	"operator_!",
	"operator_==",
	"operator_!=",
	"operator_<",
	"operator_>",
	"operator_<=",
	"operator_>=",
	"operator_&&",
	"operator_||",
	"operator_[]"
};
static_assert(NUMBEROF(k_native_operator_names) == enum_count<e_native_operator>(), "Native operator count mismatch");

const char *get_native_operator_native_module_name(e_native_operator native_operator) {
	wl_assert(valid_enum_index(native_operator));
	return k_native_operator_names[enum_index(native_operator)];
}

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
	m_primitive_type = e_native_module_primitive_type::k_real;
	m_flags = 0;
}

c_native_module_data_type::c_native_module_data_type(e_native_module_primitive_type primitive_type, bool is_array) {
	wl_assert(valid_enum_index(primitive_type));
	m_primitive_type = primitive_type;
	m_flags = 0;

	set_bit(m_flags, e_flag::k_is_array, is_array);
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
	return test_bit(m_flags, e_flag::k_is_array);
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

c_native_module_qualified_data_type::c_native_module_qualified_data_type() {
	m_qualifier = e_native_module_qualifier::k_in;
}

c_native_module_qualified_data_type::c_native_module_qualified_data_type(
	c_native_module_data_type data_type, e_native_module_qualifier qualifier)
	: m_data_type(data_type)
	, m_qualifier(qualifier) {
	wl_assert(!data_type.is_valid() || valid_enum_index(qualifier));
}

c_native_module_qualified_data_type c_native_module_qualified_data_type::invalid() {
	return c_native_module_qualified_data_type(
		c_native_module_data_type::invalid(), e_native_module_qualifier::k_count);
}

bool c_native_module_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

c_native_module_data_type c_native_module_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_native_module_qualifier c_native_module_qualified_data_type::get_qualifier() const {
	return m_qualifier;
}

bool c_native_module_qualified_data_type::operator==(const c_native_module_qualified_data_type &other) const {
	return (m_data_type == other.m_data_type) && (m_qualifier == other.m_qualifier);
}

bool c_native_module_qualified_data_type::operator!=(const c_native_module_qualified_data_type &other) const {
	return (m_data_type != other.m_data_type) || (m_qualifier != other.m_qualifier);
}

uint32 c_native_module_data_type::write() const {
	wl_assert(is_valid()); // Invalid type is only used as a sentinel in a few spots, should never write it
	s_native_module_data_type_format format;
	format.primitive_type = static_cast<uint16>(m_primitive_type);
	format.flags = static_cast<uint16>(m_flags);
	return format.data;
}

bool c_native_module_data_type::read(uint32 data) {
	s_native_module_data_type_format format;
	format.data = data;

	if (!VALID_INDEX(format.primitive_type, enum_count<e_native_module_primitive_type>())) {
		return false;
	}

	// This is checking for out-of-range flags
	if (format.flags >= (1 << static_cast<uint32>(e_flag::k_count))) {
		return false;
	}

	m_primitive_type = static_cast<e_native_module_primitive_type>(format.primitive_type);
	m_flags = format.flags;
	return true;
}

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type) {
	wl_assert(valid_enum_index(primitive_type));
	return k_native_module_primitive_type_traits[enum_index(primitive_type)];
}

size_t get_native_module_input_index_for_argument_index(const s_native_module &native_module, size_t argument_index) {
	size_t input_index = 0;
	for (size_t arg = 0; arg < native_module.arguments.get_count(); arg++) {
		if (arg == argument_index) {
			wl_assert(native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier()));
			return input_index;
		}

		if (native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier())) {
			input_index++;
		}
	}

	wl_vhalt("Invalid argument index");
	return k_invalid_argument_index;
}

size_t get_native_module_output_index_for_argument_index(const s_native_module &native_module, size_t argument_index) {
	size_t output_index = 0;
	for (size_t arg = 0; arg < native_module.arguments.get_count(); arg++) {
		if (arg == argument_index) {
			wl_assert(native_module.arguments[arg].type.get_qualifier() == e_native_module_qualifier::k_out);
			return output_index;
		}

		if (native_module.arguments[arg].type.get_qualifier() == e_native_module_qualifier::k_out) {
			output_index++;
		}
	}

	wl_vhalt("Invalid argument index");
	return k_invalid_argument_index;
}
