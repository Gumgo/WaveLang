#pragma once

#include "common/common.h"

enum class e_task_qualifier {
	k_invalid = -1,

	k_in,
	k_out,

	// An input which is a compile-time constant
	// For arrays, the array itself is always constant so if this is true, it means all elements are constants
	k_constant,

	k_count
};

enum class e_task_primitive_type {
	k_invalid = -1,

	k_real,
	k_bool,
	k_string,

	k_count
};

struct s_task_primitive_type_traits {
	bool is_dynamic;	// Whether this is a runtime-dynamic type
};

class c_task_data_type {
public:
	c_task_data_type() = default;
	explicit c_task_data_type(e_task_primitive_type primitive_type, bool is_array = false);
	static c_task_data_type invalid();

	bool is_valid() const;

	e_task_primitive_type get_primitive_type() const;
	const s_task_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_task_data_type get_element_type() const;
	c_task_data_type get_array_type() const;

	bool operator==(const c_task_data_type &other) const;
	bool operator!=(const c_task_data_type &other) const;

private:
	enum class e_flag {
		k_is_array,

		k_count
	};

	e_task_primitive_type m_primitive_type = e_task_primitive_type::k_invalid;
	uint32 m_flags = 0;
};

class c_task_qualified_data_type {
public:
	c_task_qualified_data_type() = default;
	c_task_qualified_data_type(c_task_data_type data_type, e_task_qualifier qualifier);
	static c_task_qualified_data_type invalid();

	bool is_valid() const;
	c_task_data_type get_data_type() const;
	e_task_qualifier get_qualifier() const;

	bool operator==(const c_task_qualified_data_type &other) const;
	bool operator!=(const c_task_qualified_data_type &other) const;

	bool is_legal() const;

private:
	c_task_data_type m_data_type = c_task_data_type();
	e_task_qualifier m_qualifier = e_task_qualifier::k_invalid;
};

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type);
