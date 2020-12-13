#pragma once

#include "common/common.h"

enum class e_task_primitive_type {
	k_invalid = -1,

	k_real,
	k_bool,
	k_string,

	k_count
};

struct s_task_primitive_type_traits {
	const char *name;	// User-facing primitive type name
	bool constant_only;	// Whether this type must always be constant
};

enum class e_task_data_mutability {
	k_invalid = -1,

	k_variable,
	k_constant,
	// Task data mutability is fully resolved - they don't have dependent-constant types

	k_count
};

enum class e_task_argument_direction {
	k_invalid = -1,

	k_in,
	k_out,

	k_count
};

class c_task_data_type {
public:
	c_task_data_type() = default;
	c_task_data_type(e_task_primitive_type primitive_type, bool is_array = false);

	static c_task_data_type invalid();

	bool is_valid() const;

	e_task_primitive_type get_primitive_type() const;
	const s_task_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_task_data_type get_element_type() const;
	c_task_data_type get_array_type() const;

	bool operator==(const c_task_data_type & other) const;
	bool operator!=(const c_task_data_type & other) const;

	std::string to_string() const;

private:
	e_task_primitive_type m_primitive_type = e_task_primitive_type::k_invalid;
	bool m_is_array = false;
};

class c_task_qualified_data_type {
public:
	c_task_qualified_data_type() = default;
	c_task_qualified_data_type(
		c_task_data_type data_type,
		e_task_data_mutability data_mutability);

	static c_task_qualified_data_type invalid();

	bool is_valid() const;

	e_task_primitive_type get_primitive_type() const;
	const s_task_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	const c_task_data_type &get_data_type() const;
	e_task_data_mutability get_data_mutability() const;
	c_task_qualified_data_type get_element_type() const;
	c_task_qualified_data_type get_array_type() const;

	bool operator==(const c_task_qualified_data_type & other) const;
	bool operator!=(const c_task_qualified_data_type & other) const;

	std::string to_string() const;

private:
	c_task_data_type m_data_type = c_task_data_type();
	e_task_data_mutability m_data_mutability = e_task_data_mutability::k_invalid;
};

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type);
