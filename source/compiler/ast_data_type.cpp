#include "compiler/ast_data_type.h"

static const s_ast_primitive_type_traits k_ast_primitive_type_traits[] = {
	//	name		is_data		is_dynamic	is_returnable
	{	"void",		false,		false,		true	},
	{	"module",	false,		false,		false	},
	{	"real",		true,		true,		true	},
	{	"bool",		true,		true,		true	},
	{	"string",	true,		false,		true	}
};

static_assert(NUMBEROF(k_ast_primitive_type_traits) == enum_count<e_ast_primitive_type>(),
	"Primitive type traits mismatch");

c_ast_data_type::c_ast_data_type() {
	m_primitive_type = e_ast_primitive_type::k_void;
	m_flags = 0;
}

c_ast_data_type::c_ast_data_type(e_ast_primitive_type primitive_type, bool is_array) {
	wl_assert(valid_enum_index(primitive_type));
	m_primitive_type = primitive_type;
	m_flags = 0;

	set_bit(m_flags, e_flag::k_is_array, is_array);
}

e_ast_primitive_type c_ast_data_type::get_primitive_type() const {
	return m_primitive_type;
}

const s_ast_primitive_type_traits &c_ast_data_type::get_primitive_type_traits() const {
	return get_ast_primitive_type_traits(m_primitive_type);
}

bool c_ast_data_type::is_array() const {
	return test_bit(m_flags, e_flag::k_is_array);
}

c_ast_data_type c_ast_data_type::get_element_type() const {
	wl_assert(is_array());
	return c_ast_data_type(m_primitive_type);
}

c_ast_data_type c_ast_data_type::get_array_type() const {
	wl_assert(!is_array());
	return c_ast_data_type(m_primitive_type, true);
}

std::string c_ast_data_type::get_string() const {
	const s_ast_primitive_type_traits &type_traits = get_ast_primitive_type_traits(m_primitive_type);
	std::string result = type_traits.name;

	if (is_array()) {
		result += "[]";
	}

	return result;
}

bool c_ast_data_type::operator==(const c_ast_data_type &other) const {
	return (m_primitive_type == other.m_primitive_type) && (m_flags == other.m_flags);
}

bool c_ast_data_type::operator!=(const c_ast_data_type &other) const {
	return (m_primitive_type != other.m_primitive_type) || (m_flags != other.m_flags);
}

const s_ast_primitive_type_traits &get_ast_primitive_type_traits(e_ast_primitive_type primitive_type) {
	wl_assert(valid_enum_index(primitive_type));
	return k_ast_primitive_type_traits[enum_index(primitive_type)];
}
