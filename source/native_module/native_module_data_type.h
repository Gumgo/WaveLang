#pragma once

#include "common/common.h"

// $TODO constexpr all this as well as AST and task data types

enum class e_native_module_primitive_type {
	k_invalid = -1,

	k_real,
	k_bool,
	k_string,

	k_count
};

struct s_native_module_primitive_type_traits {
	const char *name;	// User-facing primitive type name
	bool constant_only;	// Whether this type must always be constant
};

// These are ordered such that if x >= y then x is assignable to y (i.e. with increasing const-ness)
enum class e_native_module_data_mutability {
	k_invalid = -1,

	k_variable,
	k_dependent_constant,
	k_constant,

	k_count
};

enum class e_native_module_argument_direction {
	k_invalid = -1,

	k_in,
	k_out,

	k_count
};

enum class e_native_module_data_access {
	k_invalid = -1,

	k_value,		// The value itself requires access
	k_reference,	// A reference to the value requires access, but not the value itself

	k_count
};

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type);

class c_native_module_data_type {
public:
	c_native_module_data_type() = default;
	c_native_module_data_type(e_native_module_primitive_type primitive_type, bool is_array, uint32 upsample_factor);

	static c_native_module_data_type invalid();
	static c_native_module_data_type empty_array();

	bool is_valid() const;
	bool is_empty_array() const;
	bool is_legal() const;

	e_native_module_primitive_type get_primitive_type() const;
	const s_native_module_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	uint32 get_upsample_factor() const;
	c_native_module_data_type get_element_type() const;
	c_native_module_data_type get_array_type() const;
	c_native_module_data_type get_upsampled_type(uint32 upsample_factor) const;

	bool operator==(const c_native_module_data_type &other) const;
	bool operator!=(const c_native_module_data_type &other) const;

	std::string to_string() const;

private:
	e_native_module_primitive_type m_primitive_type = e_native_module_primitive_type::k_invalid;
	bool m_is_array = false;
	uint32 m_upsample_factor = 1;
};

class c_native_module_qualified_data_type {
public:
	c_native_module_qualified_data_type() = default;
	c_native_module_qualified_data_type(
		c_native_module_data_type data_type,
		e_native_module_data_mutability data_mutability);

	static c_native_module_qualified_data_type invalid();
	static c_native_module_qualified_data_type empty_array();

	bool is_valid() const;
	bool is_empty_array() const;
	bool is_legal() const;

	e_native_module_primitive_type get_primitive_type() const;
	const s_native_module_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	uint32 get_upsample_factor() const;
	const c_native_module_data_type &get_data_type() const;
	e_native_module_data_mutability get_data_mutability() const;
	c_native_module_qualified_data_type get_element_type() const;
	c_native_module_qualified_data_type get_array_type() const;
	c_native_module_qualified_data_type get_upsampled_type(uint32 upsample_factor) const;
	c_native_module_qualified_data_type change_data_mutability(e_native_module_data_mutability data_mutability) const;

	bool operator==(const c_native_module_qualified_data_type &other) const;
	bool operator!=(const c_native_module_qualified_data_type &other) const;

	std::string to_string() const;

private:
	c_native_module_data_type m_data_type = c_native_module_data_type();
	e_native_module_data_mutability m_data_mutability = e_native_module_data_mutability::k_invalid;
};

bool is_native_module_data_type_assignable(
	const c_native_module_qualified_data_type &from_data_type,
	const c_native_module_qualified_data_type &to_data_type);
