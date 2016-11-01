#include "engine/task_function.h"

const s_task_function_uid s_task_function_uid::k_invalid = s_task_function_uid::build(0xffffffff, 0xffffffff);

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
	m_flags = 0;
}

c_task_data_type::c_task_data_type(e_task_primitive_type primitive_type, bool is_array) {
	wl_assert(VALID_INDEX(primitive_type, k_task_primitive_type_count));
	m_primitive_type = primitive_type;
	m_flags = 0;

	set_bit<k_flag_is_array>(m_flags, is_array);
}

c_task_data_type c_task_data_type::invalid() {
	// Set all to 0xffffffff
	c_task_data_type result;
	result.m_primitive_type = static_cast<e_task_primitive_type>(-1);
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
	return c_task_data_type(m_primitive_type);
}

c_task_data_type c_task_data_type::get_array_type() const {
	wl_assert(is_valid());
	wl_assert(!is_array());
	return c_task_data_type(m_primitive_type, true);
}

bool c_task_data_type::operator==(const c_task_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type) &&
		(m_flags == other.m_flags);
}

bool c_task_data_type::operator!=(const c_task_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type) ||
		(m_flags != other.m_flags);
}

c_task_qualified_data_type::c_task_qualified_data_type() {
	m_qualifier = k_task_qualifier_in;
}

c_task_qualified_data_type::c_task_qualified_data_type(c_task_data_type data_type, e_task_qualifier qualifier)
	: m_data_type(data_type)
	, m_qualifier(qualifier) {
	wl_assert(!data_type.is_valid() || VALID_INDEX(qualifier, k_task_qualifier_count));
}

c_task_qualified_data_type c_task_qualified_data_type::invalid() {
	return c_task_qualified_data_type(c_task_data_type::invalid(), k_task_qualifier_count);
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

	if (!m_data_type.get_primitive_type_traits().is_dynamic && m_qualifier != k_task_qualifier_in) {
		// Static-only types must only be used as inputs, e.g. a task cannot output a new string
		return false;
	}

	if (m_data_type.is_array() && m_qualifier != k_task_qualifier_in) {
		// Arrays cannot be outputs
		return false;
	}

	return true;
}

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type) {
	wl_assert(VALID_INDEX(primitive_type, k_task_primitive_type_count));
	return k_task_primitive_type_traits[primitive_type];
}
