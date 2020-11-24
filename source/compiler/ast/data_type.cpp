#include "compiler/ast/data_type.h"

static constexpr const s_AST_primitive_type_traits k_primitive_type_traits[] = {
	// name				const	array	values
	{ "<invalid>",		false,	false,	false	},	// k_invalid
	{ "<error-type>",	true,	true,	true	},	// k_error
	{ "void",			false,	false,	false	},	// k_void
	{ "real",			true,	true,	true	},	// k_real
	{ "bool",			true,	true,	true	},	// k_bool
	{ "string",			true,	true,	true	}	// k_string
};
static_assert(array_count(k_primitive_type_traits) == enum_count<e_AST_primitive_type>());

// Note: for k_error, the traits indicate that everything is allowed. This is so that error types pass easily though
// test to avoid display of further errors (we only want to display an error once when an invalid type is detected, we
// don't want to keep displaying errors when the invalid type is used).

const s_AST_primitive_type_traits &get_AST_primitive_type_traits(e_AST_primitive_type primitive_type) {
	return k_primitive_type_traits[enum_index(primitive_type)];
}

c_AST_data_type::c_AST_data_type(e_AST_primitive_type primitive_type, bool is_array = false) {
	if (primitive_type == e_AST_primitive_type::k_error) {
		// There is only a single "error" type to avoid ambiguity
		wl_assert(!is_array);
	}

	m_primitive_type = primitive_type;
	m_is_array = is_array;
}

c_AST_data_type c_AST_data_type::error() {
	return c_AST_data_type(e_AST_primitive_type::k_error);
}

c_AST_data_type c_AST_data_type::empty_array() {
	return c_AST_data_type(e_AST_primitive_type::k_invalid, true);
}

bool c_AST_data_type::is_valid() const {
	return m_primitive_type != e_AST_primitive_type::k_invalid || m_is_array;
}

bool c_AST_data_type::is_error() const {
	return m_primitive_type == e_AST_primitive_type::k_error;
}

bool c_AST_data_type::is_empty_array() const {
	return m_primitive_type == e_AST_primitive_type::k_invalid && m_is_array;
}

bool c_AST_data_type::is_legal_type_declaration() const {
	const s_AST_primitive_type_traits &primitive_type_traits = get_primitive_type_traits();
	if (m_is_array) {
		return primitive_type_traits.allows_array;
	}

	return true;
}

bool c_AST_data_type::is_void() const {
	return m_primitive_type == e_AST_primitive_type::k_void;
}

e_AST_primitive_type c_AST_data_type::get_primitive_type() const {
	return m_primitive_type;
}

const s_AST_primitive_type_traits &c_AST_data_type::get_primitive_type_traits() const {
	return k_primitive_type_traits[enum_index(m_primitive_type)];
}

bool c_AST_data_type::is_array() const {
	return m_is_array;
}

c_AST_data_type c_AST_data_type::get_element_type() const {
	wl_assert(m_is_array);
	return c_AST_data_type(m_primitive_type, false);
}

c_AST_data_type c_AST_data_type::get_array_type() const {
	wl_assert(!m_is_array);
	return c_AST_data_type(m_primitive_type, true);
}

bool c_AST_data_type::operator==(const c_AST_data_type &other) const {
	return m_primitive_type == other.m_primitive_type && m_is_array == other.m_is_array;
}

bool c_AST_data_type::operator!=(const c_AST_data_type &other) const {
	return m_primitive_type != other.m_primitive_type || m_is_array != other.m_is_array;
}

std::string c_AST_data_type::to_string() const {
	if (is_empty_array()) {
		return "[]";
	}

	std::string result = get_primitive_type_traits().name;
	if (m_is_array) {
		result += "[]";
	}

	return result;
}

c_AST_qualified_data_type::c_AST_qualified_data_type(c_AST_data_type data_type, e_AST_data_mutability data_mutability) {
	if (data_type.is_error()) {
		// There is only a single "error" type to avoid ambiguity
		wl_assert(data_mutability == e_AST_data_mutability::k_invalid);
	}

	if (data_type.is_empty_array()) {
		// There is only a single "empty array" type to avoid ambiguity
		wl_assert(data_mutability == e_AST_data_mutability::k_constant);
	}

	m_data_type = data_type;
	m_data_mutability = data_mutability;
}

c_AST_qualified_data_type c_AST_qualified_data_type::error() {
	return c_AST_qualified_data_type(c_AST_data_type::error(), e_AST_data_mutability::k_invalid);
}

c_AST_qualified_data_type c_AST_qualified_data_type::empty_array() {
	return c_AST_qualified_data_type(c_AST_data_type::empty_array(), e_AST_data_mutability::k_constant);
}

bool c_AST_qualified_data_type::is_valid() const {
	return m_data_type.is_valid();
}

bool c_AST_qualified_data_type::is_error() const {
	return m_data_type.is_error();
}

bool c_AST_qualified_data_type::is_empty_array() const {
	return m_data_type.is_empty_array();
}

bool c_AST_qualified_data_type::is_legal_type_declaration() const {
	if (!m_data_type.is_legal_type_declaration() || m_data_mutability == e_AST_data_mutability::k_invalid) {
		return false;
	}

	const s_AST_primitive_type_traits &primitive_type_traits = m_data_type.get_primitive_type_traits();
	if (m_data_mutability == e_AST_data_mutability::k_constant
		|| m_data_mutability == e_AST_data_mutability::k_dependent_constant) {
		return primitive_type_traits.allows_const;
	}

	return true;
}

bool c_AST_qualified_data_type::is_void() const {
	return m_data_type.is_void();
}

e_AST_primitive_type c_AST_qualified_data_type::get_primitive_type() const {
	return m_data_type.get_primitive_type();
}

const s_AST_primitive_type_traits &c_AST_qualified_data_type::get_primitive_type_traits() const {
	return m_data_type.get_primitive_type_traits();
}

bool c_AST_qualified_data_type::is_array() const {
	return m_data_type.is_array();
}

const c_AST_data_type &c_AST_qualified_data_type::get_data_type() const {
	return m_data_type;
}

e_AST_data_mutability c_AST_qualified_data_type::get_data_mutability() const {
	return m_data_mutability;
}

c_AST_qualified_data_type c_AST_qualified_data_type::get_element_type() const {
	return c_AST_qualified_data_type(m_data_type.get_element_type(), m_data_mutability);
}

c_AST_qualified_data_type c_AST_qualified_data_type::get_array_type() const {
	return c_AST_qualified_data_type(m_data_type.get_array_type(), m_data_mutability);
}

bool c_AST_qualified_data_type::operator==(const c_AST_qualified_data_type &other) const {
	return m_data_type == other.m_data_type && m_data_mutability == other.m_data_mutability;
}

bool c_AST_qualified_data_type::operator!=(const c_AST_qualified_data_type &other) const {
	return m_data_type != other.m_data_type || m_data_mutability != other.m_data_mutability;
}

std::string c_AST_qualified_data_type::to_string() const {
	std::string result = m_data_type.to_string();
	if (m_data_mutability == e_AST_data_mutability::k_constant) {
		result = "const " + result;
	} else if (m_data_mutability == e_AST_data_mutability::k_dependent_constant) {
		result = "const? " + result;
	}

	return result;
}

bool is_ast_data_type_assignable(
	const c_AST_qualified_data_type &from_data_type,
	const c_AST_qualified_data_type &to_data_type) {
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

	//           |      FROM
	//           | var const? const
	//  _________|_________________
	//    var    |  Y     Y     Y
	// TO const? |  N     Y     Y
	//    const  |  N     N     Y
	return from_data_type.get_data_mutability() == e_AST_data_mutability::k_constant
		|| to_data_type.get_data_mutability() == e_AST_data_mutability::k_variable
		|| from_data_type.get_data_mutability() == to_data_type.get_data_mutability();
}
