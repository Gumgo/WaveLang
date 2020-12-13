#include "native_module/native_module_data_type.h"

static constexpr s_native_module_primitive_type_traits k_native_module_primitive_type_traits[] = {
	// name		constant_only
	{ "real",	false	},	// k_real
	{ "bool",	false	},	// k_bool
	{ "string",	true	}	// k_string
};

STATIC_ASSERT(is_enum_fully_mapped<e_native_module_primitive_type>(k_native_module_primitive_type_traits));

c_native_module_data_type::c_native_module_data_type(e_native_module_primitive_type primitive_type, bool is_array) {
	wl_assert(valid_enum_index(primitive_type));
	m_primitive_type = primitive_type;
	m_is_array = is_array;
}

c_native_module_data_type c_native_module_data_type::invalid() {
	return c_native_module_data_type();
}

bool c_native_module_data_type::is_valid() const {
	return m_primitive_type != e_native_module_primitive_type::k_invalid;
}

bool c_native_module_data_type::is_legal() const {
	return is_valid();
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
	return m_is_array;
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
	return (m_primitive_type == other.m_primitive_type) && (m_is_array == other.m_is_array);
}

bool c_native_module_data_type::operator!=(const c_native_module_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type) || (m_is_array != other.m_is_array);
}

std::string c_native_module_data_type::to_string() const {
	std::string result = get_primitive_type_traits().name;
	if (m_is_array) {
		result += "[]";
	}

	return result;
}

c_native_module_qualified_data_type::c_native_module_qualified_data_type(
	c_native_module_data_type data_type,
	e_native_module_data_mutability data_mutability)
	: m_data_type(data_type)
	, m_data_mutability(data_mutability) {
	wl_assert(data_type.is_valid() == valid_enum_index(data_mutability));
}

c_native_module_qualified_data_type c_native_module_qualified_data_type::invalid() {
	return c_native_module_qualified_data_type();
}

bool c_native_module_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

bool c_native_module_qualified_data_type::is_legal() const {
	if (!m_data_type.is_legal()) {
		return false;
	}

	if (get_primitive_type_traits().constant_only) {
		return m_data_mutability == e_native_module_data_mutability::k_constant;
	}

	return true;
}

e_native_module_primitive_type c_native_module_qualified_data_type::get_primitive_type() const {
	return m_data_type.get_primitive_type();
}

const s_native_module_primitive_type_traits &c_native_module_qualified_data_type::get_primitive_type_traits() const {
	return m_data_type.get_primitive_type_traits();
}

bool c_native_module_qualified_data_type::is_array() const {
	return m_data_type.is_array();
}

const c_native_module_data_type &c_native_module_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_native_module_data_mutability c_native_module_qualified_data_type::get_data_mutability() const {
	return m_data_mutability;
}

c_native_module_qualified_data_type c_native_module_qualified_data_type::get_element_type() const {
	c_native_module_qualified_data_type(m_data_type.get_element_type(), m_data_mutability);
}

c_native_module_qualified_data_type c_native_module_qualified_data_type::get_array_type() const {
	c_native_module_qualified_data_type(m_data_type.get_array_type(), m_data_mutability);
}

bool c_native_module_qualified_data_type::operator==(const c_native_module_qualified_data_type &other) const {
	return (m_data_type == other.m_data_type) && (m_data_mutability == other.m_data_mutability);
}

bool c_native_module_qualified_data_type::operator!=(const c_native_module_qualified_data_type &other) const {
	return (m_data_type != other.m_data_type) || (m_data_mutability != other.m_data_mutability);
}

std::string c_native_module_qualified_data_type::to_string() const {
	std::string result = m_data_type.to_string();
	if (m_data_mutability == e_native_module_data_mutability::k_constant) {
		result = "const " + result;
	} else if (m_data_mutability == e_native_module_data_mutability::k_dependent_constant) {
		result = "const? " + result;
	}

	return result;
}

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type) {
	wl_assert(valid_enum_index(primitive_type));
	return k_native_module_primitive_type_traits[enum_index(primitive_type)];
}