#ifndef WAVELANG_AST_DATA_TYPE_H__
#define WAVELANG_AST_DATA_TYPE_H__

#include "common/common.h"

enum e_ast_primitive_type {
	k_ast_primitive_type_void,
	k_ast_primitive_type_module,
	k_ast_primitive_type_real,
	k_ast_primitive_type_bool,
	k_ast_primitive_type_string,

	k_ast_primitive_type_count
};

enum e_ast_qualifier {
	k_ast_qualifier_none,
	k_ast_qualifier_in,
	k_ast_qualifier_out,

	k_ast_qualifier_count
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
	enum e_flag {
		k_flag_is_array,

		k_flag_count
	};

	e_ast_primitive_type m_primitive_type;
	uint32 m_flags;
};

const s_ast_primitive_type_traits &get_ast_primitive_type_traits(e_ast_primitive_type primitive_type);

#endif // WAVELANG_AST_DATA_TYPE_H__