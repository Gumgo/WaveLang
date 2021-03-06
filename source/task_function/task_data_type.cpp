#include "task_function/task_data_type.h"

static constexpr s_task_primitive_type_traits k_task_primitive_type_traits[] = {
	// name		constant_only
	{ "real",	false	},	// real
	{ "bool",	false	},	// bool
	{ "string",	true	}	// string
};

STATIC_ASSERT(is_enum_fully_mapped<e_task_primitive_type>(k_task_primitive_type_traits));

c_task_data_type::c_task_data_type(e_task_primitive_type primitive_type, bool is_array, uint32 upsample_factor) {
	wl_assert(valid_enum_index(primitive_type));
	wl_assert(upsample_factor > 0);
	m_primitive_type = primitive_type;
	m_is_array = is_array;
	m_upsample_factor = upsample_factor;
}

c_task_data_type c_task_data_type::invalid() {
	return c_task_data_type();
}

bool c_task_data_type::is_valid() const {
	return m_primitive_type != e_task_primitive_type::k_invalid;
}

bool c_task_data_type::is_legal() const {
	return is_valid();
}

e_task_primitive_type c_task_data_type::get_primitive_type() const {
	wl_assert(is_valid());
	return m_primitive_type;
}

const s_task_primitive_type_traits &c_task_data_type::get_primitive_type_traits() const {
	wl_assert(is_valid());
	return get_task_primitive_type_traits(m_primitive_type);
}

bool c_task_data_type::is_array() const {
	wl_assert(is_valid());
	return m_is_array;
}

uint32 c_task_data_type::get_upsample_factor() const {
	return m_upsample_factor;
}

c_task_data_type c_task_data_type::get_element_type() const {
	wl_assert(is_valid());
	wl_assert(is_array());
	return c_task_data_type(m_primitive_type, false, m_upsample_factor);
}

c_task_data_type c_task_data_type::get_array_type() const {
	wl_assert(is_valid());
	wl_assert(!is_array());
	return c_task_data_type(m_primitive_type, true, m_upsample_factor);
}

c_task_data_type c_task_data_type::get_upsampled_type(uint32 upsample_factor) const {
	wl_assert(is_valid());
	return c_task_data_type(m_primitive_type, m_is_array, m_upsample_factor * upsample_factor);
}

bool c_task_data_type::operator==(const c_task_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type)
		&& (m_is_array == other.m_is_array)
		&& (m_upsample_factor == other.m_upsample_factor);
}

bool c_task_data_type::operator!=(const c_task_data_type &other) const {
	return !(*this == other);
}

std::string c_task_data_type::to_string() const {
	std::string result = get_primitive_type_traits().name;

	if (m_upsample_factor > 1) {
		result.push_back('@');
		result += std::to_string(m_upsample_factor);
		result.push_back('x');
	}

	if (m_is_array) {
		result += "[]";
	}

	return result;
}

c_task_qualified_data_type::c_task_qualified_data_type(
	c_task_data_type data_type,
	e_task_data_mutability data_mutability)
	: m_data_type(data_type)
	, m_data_mutability(data_mutability) {
	wl_assert(data_type.is_valid() == valid_enum_index(data_mutability));
}

c_task_qualified_data_type c_task_qualified_data_type::invalid() {
	return c_task_qualified_data_type();
}

bool c_task_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

bool c_task_qualified_data_type::is_legal() const {
	if (!m_data_type.is_legal()) {
		return false;
	}

	if (get_primitive_type_traits().constant_only) {
		return m_data_mutability == e_task_data_mutability::k_constant;
	}

	return true;
}

e_task_primitive_type c_task_qualified_data_type::get_primitive_type() const {
	return m_data_type.get_primitive_type();
}

const s_task_primitive_type_traits &c_task_qualified_data_type::get_primitive_type_traits() const {
	return m_data_type.get_primitive_type_traits();
}

bool c_task_qualified_data_type::is_array() const {
	return m_data_type.is_array();
}

uint32 c_task_qualified_data_type::get_upsample_factor() const {
	return m_data_type.get_upsample_factor();
}

const c_task_data_type &c_task_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_task_data_mutability c_task_qualified_data_type::get_data_mutability() const {
	return m_data_mutability;
}

c_task_qualified_data_type c_task_qualified_data_type::get_element_type() const {
	return c_task_qualified_data_type(m_data_type.get_element_type(), m_data_mutability);
}

c_task_qualified_data_type c_task_qualified_data_type::get_array_type() const {
	return c_task_qualified_data_type(m_data_type.get_array_type(), m_data_mutability);
}

c_task_qualified_data_type c_task_qualified_data_type::get_upsampled_type(uint32 upsample_factor) const {
	if (m_data_mutability == e_task_data_mutability::k_constant) {
		wl_assert(get_upsample_factor() == 1);
		return *this;
	} else {
		return c_task_qualified_data_type(m_data_type.get_upsampled_type(upsample_factor), m_data_mutability);
	}
}


bool c_task_qualified_data_type::operator==(const c_task_qualified_data_type &other) const {
	return (m_data_type == other.m_data_type) && (m_data_mutability == other.m_data_mutability);
}

bool c_task_qualified_data_type::operator!=(const c_task_qualified_data_type &other) const {
	return (m_data_type != other.m_data_type) || (m_data_mutability != other.m_data_mutability);
}

std::string c_task_qualified_data_type::to_string() const {
	std::string result = m_data_type.to_string();
	if (m_data_mutability == e_task_data_mutability::k_constant) {
		result = "const " + result;
	}

	return result;
}

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type) {
	wl_assert(valid_enum_index(primitive_type));
	return k_task_primitive_type_traits[enum_index(primitive_type)];
}
