#pragma once

#include "common/common.h"

enum class e_ast_primitive_type {
	// This data type has not yet been set
	k_invalid,

	// An error occurred and has already been reported. We should silently ignore this data type when we encounter it so
	// that we don't get a huge sequence of error messages from an earlier error.
	k_error,

	// This is only valid as a return type
	k_void,

	// Primitive types with backing data
	k_real,
	k_bool,
	k_string,

	k_count
};

struct s_ast_primitive_type_traits {
	const char *name;	// User-facing primitive type name
	bool allows_const;	// Whether this type can have the const qualifier
	bool allows_array;	// Whether this type can be an array
	bool allows_values;	// Whether a value of this type is legal
};

enum class e_ast_data_mutability {
	k_invalid,
	k_variable,				// This data changes at runtime
	k_constant,				// This data type must be constant after compilation
	k_dependent_constant,	// Only allowed on outputs and return values - constant if all inputs are constant

	k_count
};

enum class e_ast_argument_direction {
	k_invalid,
	k_in,
	k_out,

	k_count
};

const s_ast_primitive_type_traits &get_ast_primitive_type_traits(e_ast_primitive_type primitive_type);

class c_ast_data_type {
public:
	c_ast_data_type() = default;
	c_ast_data_type(e_ast_primitive_type primitive_type, bool is_array = false);

	static c_ast_data_type error();
	static c_ast_data_type empty_array();

	bool is_valid() const;
	bool is_error() const;
	bool is_empty_array() const;
	bool is_legal_type_declaration() const;
	bool is_void() const;

	e_ast_primitive_type get_primitive_type() const;
	const s_ast_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_ast_data_type get_element_type() const;
	c_ast_data_type get_array_type() const;

	bool operator==(const c_ast_data_type &other) const;
	bool operator!=(const c_ast_data_type &other) const;

	std::string to_string() const;

private:
	e_ast_primitive_type m_primitive_type = e_ast_primitive_type::k_invalid;
	bool m_is_array = false;
};

class c_ast_qualified_data_type {
public:
	c_ast_qualified_data_type() = default;
	c_ast_qualified_data_type(c_ast_data_type data_type, e_ast_data_mutability data_mutability);

	static c_ast_qualified_data_type error();
	static c_ast_qualified_data_type empty_array();

	bool is_valid() const;
	bool is_error() const;
	bool is_empty_array() const;
	bool is_legal_type_declaration() const;
	bool is_void() const;

	e_ast_primitive_type get_primitive_type() const;
	const s_ast_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	const c_ast_data_type &get_data_type() const;
	e_ast_data_mutability get_data_mutability() const;
	c_ast_qualified_data_type get_element_type() const;
	c_ast_qualified_data_type get_array_type() const;

	bool operator==(const c_ast_qualified_data_type &other) const;
	bool operator!=(const c_ast_qualified_data_type &other) const;

	std::string to_string() const;

public:
	c_ast_data_type m_data_type;
	e_ast_data_mutability m_data_mutability = e_ast_data_mutability::k_invalid;
};

bool is_ast_data_type_assignable(
	const c_ast_qualified_data_type &from_data_type,
	const c_ast_qualified_data_type &to_data_type);
