#include "compiler/ast/data_type.h"

static constexpr const s_ast_primitive_type_traits k_primitive_type_traits[] = {
	//					allows	allows	allows	only	allows
	// name				const	array	values	const	upsample
	{ "<invalid>",		false,	false,	false,	false,	false },	// k_invalid
	{ "<error-type>",	true,	true,	true,	false,	false },	// k_error
	{ "void",			false,	false,	false,	false,	false },	// k_void
	{ "real",			true,	true,	true,	false,	true },		// k_real
	{ "bool",			true,	true,	true,	false,	true },		// k_bool
	{ "string",			true,	true,	true,	true,	false }		// k_string
};
STATIC_ASSERT(is_enum_fully_mapped<e_ast_primitive_type>(k_primitive_type_traits));

// Note: for k_error, the traits indicate that everything is allowed. This is so that error types pass easily though
// test to avoid display of further errors (we only want to display an error once when an invalid type is detected, we
// don't want to keep displaying errors when the invalid type is used).

const s_ast_primitive_type_traits &get_ast_primitive_type_traits(e_ast_primitive_type primitive_type) {
	return k_primitive_type_traits[enum_index(primitive_type)];
}

c_ast_data_type::c_ast_data_type(e_ast_primitive_type primitive_type, bool is_array, uint32 upsample_factor) {
	wl_assert(upsample_factor > 0);

	if (primitive_type == e_ast_primitive_type::k_error) {
		// There is only a single "error" type to avoid ambiguity
		wl_assert(!is_array);
		wl_assert(upsample_factor == 1);
	}

	m_primitive_type = primitive_type;
	m_is_array = is_array;
	m_upsample_factor = upsample_factor;
}

c_ast_data_type c_ast_data_type::error() {
	return c_ast_data_type(e_ast_primitive_type::k_error, false, 1);
}

c_ast_data_type c_ast_data_type::empty_array() {
	return c_ast_data_type(e_ast_primitive_type::k_invalid, true, 1);
}

c_ast_data_type c_ast_data_type::void_() {
	return c_ast_data_type(e_ast_primitive_type::k_void, false, 1);
}

bool c_ast_data_type::is_valid() const {
	return m_primitive_type != e_ast_primitive_type::k_invalid || m_is_array;
}

bool c_ast_data_type::is_error() const {
	return m_primitive_type == e_ast_primitive_type::k_error;
}

bool c_ast_data_type::is_empty_array() const {
	return m_primitive_type == e_ast_primitive_type::k_invalid && m_is_array;
}

bool c_ast_data_type::is_legal_type_declaration(bool was_upsample_factor_provided) const {
	const s_ast_primitive_type_traits &primitive_type_traits = get_primitive_type_traits();

	if (m_is_array && !primitive_type_traits.allows_array) {
		return false;
	}

	if ((was_upsample_factor_provided || m_upsample_factor > 1) && !primitive_type_traits.allows_upsampling) {
		return false;
	}

	return true;
}

bool c_ast_data_type::is_void() const {
	return m_primitive_type == e_ast_primitive_type::k_void;
}

e_ast_primitive_type c_ast_data_type::get_primitive_type() const {
	return m_primitive_type;
}

const s_ast_primitive_type_traits &c_ast_data_type::get_primitive_type_traits() const {
	return k_primitive_type_traits[enum_index(m_primitive_type)];
}

bool c_ast_data_type::is_array() const {
	return m_is_array;
}

uint32 c_ast_data_type::get_upsample_factor() const {
	return m_upsample_factor;
}

c_ast_data_type c_ast_data_type::get_element_type() const {
	if (is_error()) {
		return error();
	}

	wl_assert(m_is_array);
	return c_ast_data_type(m_primitive_type, false, m_upsample_factor);
}

c_ast_data_type c_ast_data_type::get_array_type() const {
	if (is_error()) {
		return error();
	}

	wl_assert(!m_is_array);
	return c_ast_data_type(m_primitive_type, true, m_upsample_factor);
}

c_ast_data_type c_ast_data_type::get_upsampled_type(uint32 upsample_factor) const {
	if (is_error()) {
		return error();
	}

	return c_ast_data_type(m_primitive_type, m_is_array, m_upsample_factor * upsample_factor);
}

bool c_ast_data_type::operator==(const c_ast_data_type &other) const {
	return m_primitive_type == other.m_primitive_type
		&& m_is_array == other.m_is_array
		&& m_upsample_factor == other.m_upsample_factor;
}

bool c_ast_data_type::operator!=(const c_ast_data_type &other) const {
	return !(*this == other);
}

std::string c_ast_data_type::to_string() const {
	if (is_empty_array()) {
		return "[]";
	}

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

c_ast_qualified_data_type::c_ast_qualified_data_type(c_ast_data_type data_type, e_ast_data_mutability data_mutability) {
	if (data_type.is_error()) {
		// There is only a single "error" type to avoid ambiguity
		wl_assert(data_mutability == e_ast_data_mutability::k_invalid);
	}

	if (data_type.is_empty_array()) {
		// There is only a single "empty array" type to avoid ambiguity
		wl_assert(data_mutability == e_ast_data_mutability::k_constant);
	}

	m_data_type = data_type;
	m_data_mutability = data_mutability;
}

c_ast_qualified_data_type c_ast_qualified_data_type::error() {
	return c_ast_qualified_data_type(c_ast_data_type::error(), e_ast_data_mutability::k_invalid);
}

c_ast_qualified_data_type c_ast_qualified_data_type::empty_array() {
	return c_ast_qualified_data_type(c_ast_data_type::empty_array(), e_ast_data_mutability::k_constant);
}

c_ast_qualified_data_type c_ast_qualified_data_type::void_() {
	return c_ast_qualified_data_type(c_ast_data_type::void_(), e_ast_data_mutability::k_variable);
}

bool c_ast_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

bool c_ast_qualified_data_type::is_error() const {
	return m_data_type.is_error();
}

bool c_ast_qualified_data_type::is_empty_array() const {
	return m_data_type.is_empty_array();
}

bool c_ast_qualified_data_type::is_legal_type_declaration(bool was_upsample_factor_provided) const {
	if (!m_data_type.is_legal_type_declaration(was_upsample_factor_provided)
		|| m_data_mutability == e_ast_data_mutability::k_invalid) {
		return false;
	}

	const s_ast_primitive_type_traits &primitive_type_traits = m_data_type.get_primitive_type_traits();

	if (m_data_mutability != e_ast_data_mutability::k_variable && !primitive_type_traits.allows_const) {
		return false;
	}

	if (m_data_mutability != e_ast_data_mutability::k_constant && primitive_type_traits.constant_only) {
		return false;
	}

	if (m_data_mutability == e_ast_data_mutability::k_constant
		&& (was_upsample_factor_provided || get_upsample_factor() > 1)) {
		return false;
	}

	return true;
}

bool c_ast_qualified_data_type::is_void() const {
	return m_data_type.is_void();
}

e_ast_primitive_type c_ast_qualified_data_type::get_primitive_type() const {
	return m_data_type.get_primitive_type();
}

const s_ast_primitive_type_traits &c_ast_qualified_data_type::get_primitive_type_traits() const {
	return m_data_type.get_primitive_type_traits();
}

bool c_ast_qualified_data_type::is_array() const {
	return m_data_type.is_array();
}

uint32 c_ast_qualified_data_type::get_upsample_factor() const {
	return m_data_type.get_upsample_factor();
}

const c_ast_data_type &c_ast_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_ast_data_mutability c_ast_qualified_data_type::get_data_mutability() const {
	return m_data_mutability;
}

c_ast_qualified_data_type c_ast_qualified_data_type::get_element_type() const {
	if (is_error()) {
		return error();
	}

	return c_ast_qualified_data_type(m_data_type.get_element_type(), m_data_mutability);
}

c_ast_qualified_data_type c_ast_qualified_data_type::get_array_type() const {
	if (is_error()) {
		return error();
	}

	return c_ast_qualified_data_type(m_data_type.get_array_type(), m_data_mutability);
}

c_ast_qualified_data_type c_ast_qualified_data_type::get_upsampled_type(uint32 upsample_factor) const {
	if (is_error()) {
		return error();
	}

	if (m_data_mutability == e_ast_data_mutability::k_constant) {
		wl_assert(get_upsample_factor() == 1);
		return *this;
	} else {
		return c_ast_qualified_data_type(m_data_type.get_upsampled_type(upsample_factor), m_data_mutability);
	}
}

c_ast_qualified_data_type c_ast_qualified_data_type::change_data_mutability(
	e_ast_data_mutability data_mutability) const {
	if (is_error()) {
		return error();
	}

	// Set upsample factor to 1 for constants
	return c_ast_qualified_data_type(
		c_ast_data_type(
			get_primitive_type(),
			is_array(),
			data_mutability == e_ast_data_mutability::k_constant ? 1 : get_upsample_factor()),
		data_mutability);
}

bool c_ast_qualified_data_type::operator==(const c_ast_qualified_data_type &other) const {
	return m_data_type == other.m_data_type && m_data_mutability == other.m_data_mutability;
}

bool c_ast_qualified_data_type::operator!=(const c_ast_qualified_data_type &other) const {
	return !(*this == other);
}

std::string c_ast_qualified_data_type::to_string() const {
	std::string result = m_data_type.to_string();
	if (m_data_mutability == e_ast_data_mutability::k_constant) {
		result = "const " + result;
	} else if (m_data_mutability == e_ast_data_mutability::k_dependent_constant) {
		result = "const? " + result;
	}

	return result;
}

bool is_ast_data_type_assignable(
	const c_ast_qualified_data_type &from_data_type,
	const c_ast_qualified_data_type &to_data_type) {
	if (from_data_type.is_error() || to_data_type.is_error()) {
		// Always allow assigning error types - if we encounter an error type we should have already reported the error
		// and we don't want to keep raising additional errors about the usage of a type which we know is invalid.
		return true;
	}

	if (from_data_type.is_empty_array() && to_data_type.is_array()) {
		// Empty arrays can be assigned to any array type
		return true;
	}

	if (from_data_type.get_primitive_type() != to_data_type.get_primitive_type()
		|| from_data_type.is_array() != to_data_type.is_array()) {
		return false;
	}

	if (from_data_type.get_upsample_factor() != to_data_type.get_upsample_factor()
		&& from_data_type.get_data_mutability() != e_ast_data_mutability::k_constant) {
		// The upsample factor must match unless we're assigning a constant
		return false;
	}

	//           |      FROM
	//           | var const? const
	//  _________|_________________
	//    var    |  Y     Y     Y
	// TO const? |  N     Y     Y
	//    const  |  N     N     Y
	return to_data_type.get_data_mutability() <= from_data_type.get_data_mutability();
}
