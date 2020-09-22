#include "engine/task_data_type.h"

static constexpr s_task_primitive_type_traits k_task_primitive_type_traits[] = {
	//	is_dynamic
	{ true },	// real
	{ true },	// bool
	{ false }	// string
};

static_assert(array_count(k_task_primitive_type_traits) == enum_count<e_task_primitive_type>(),
	"Primitive type traits mismatch");

c_task_data_type::c_task_data_type(e_task_primitive_type primitive_type, bool is_array) {
	wl_assert(valid_enum_index(primitive_type));
	m_primitive_type = primitive_type;
	m_flags = 0;

	set_bit(m_flags, e_flag::k_is_array, is_array);
}

c_task_data_type c_task_data_type::invalid() {
	return c_task_data_type();
}

bool c_task_data_type::is_valid() const {
	return m_primitive_type != e_task_primitive_type::k_invalid;
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
	return test_bit(m_flags, e_flag::k_is_array);
}

c_task_data_type c_task_data_type::get_element_type() const {
	wl_assert(is_valid());
	wl_assert(is_array());
	return c_task_data_type(m_primitive_type);
}

c_task_data_type c_task_data_type::get_array_type() const {
	wl_assert(is_valid());
	wl_assert(!is_array());
	return c_task_data_type(m_primitive_type, true);
}

bool c_task_data_type::operator==(const c_task_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type)
		&& (m_flags == other.m_flags);
}

bool c_task_data_type::operator!=(const c_task_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type)
		|| (m_flags != other.m_flags);
}

c_task_qualified_data_type::c_task_qualified_data_type(c_task_data_type data_type, e_task_qualifier qualifier)
	: m_data_type(data_type)
	, m_qualifier(qualifier) {
	wl_assert(data_type.is_valid() == valid_enum_index(qualifier));
}

c_task_qualified_data_type c_task_qualified_data_type::invalid() {
	return c_task_qualified_data_type();
}

bool c_task_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

c_task_data_type c_task_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_task_qualifier c_task_qualified_data_type::get_qualifier() const {
	return m_qualifier;
}

bool c_task_qualified_data_type::operator==(const c_task_qualified_data_type &other) const {
	return (m_data_type == other.m_data_type) && (m_qualifier == other.m_qualifier);
}

bool c_task_qualified_data_type::operator!=(const c_task_qualified_data_type &other) const {
	return (m_data_type != other.m_data_type) || (m_qualifier != other.m_qualifier);
}

bool c_task_qualified_data_type::is_legal() const {
	wl_assert(is_valid());

	if (!m_data_type.get_primitive_type_traits().is_dynamic && m_qualifier != e_task_qualifier::k_constant) {
		// Static-only types must only be used as constants, e.g. a task cannot output a new string
		return false;
	}

	if (m_data_type.is_array() && m_qualifier == e_task_qualifier::k_out) {
		// Arrays cannot be outputs
		return false;
	}

	return true;
}

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type) {
	wl_assert(valid_enum_index(primitive_type));
	return k_task_primitive_type_traits[enum_index(primitive_type)];
}
