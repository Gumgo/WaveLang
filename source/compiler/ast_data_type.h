#pragma once

#include "common/common.h"

enum class e_ast_primitive_type {
	k_void,
	k_module,
	k_real,
	k_bool,
	k_string,

	k_count
};

enum class e_ast_qualifier {
	k_none,
	k_in,
	k_out,

	k_count
};

struct s_ast_primitive_type_traits {
	const char *name;	// User-facing primitive type name
	bool is_data;		// Whether this type can be used as variable and constant data
	bool is_dynamic;	// Whether this is a runtime-dynamic type
	bool is_returnable;	// Whether this is a valid return type for modules
};

class c_ast_data_type {
public:
	c_ast_data_type();
	c_ast_data_type(e_ast_primitive_type primitive_type, bool is_array = false);

	e_ast_primitive_type get_primitive_type() const;
	const s_ast_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_ast_data_type get_element_type() const;
	c_ast_data_type get_array_type() const;

	std::string get_string() const;

	bool operator==(const c_ast_data_type &other) const;
	bool operator!=(const c_ast_data_type &other) const;

private:
	enum class e_flag {
		k_is_array,

		k_count
	};

	e_ast_primitive_type m_primitive_type;
	uint32 m_flags;
};

const s_ast_primitive_type_traits &get_ast_primitive_type_traits(e_ast_primitive_type primitive_type);

