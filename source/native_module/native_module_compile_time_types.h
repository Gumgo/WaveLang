#pragma once

#include "common/common.h"

#include "native_module/native_module_data_type.h"

#include <string>
#include <vector>

//  Type table:
//  script type  | value type                           | reference type
// ------------------------------------------------------------------------------------------------------
//  in real      | real32                               | c_native_module_real_reference
//  out real     | real32 &                             | c_native_module_real_reference &
//  in bool      | bool                                 | c_native_module_bool_reference
//  out bool     | bool &                               | c_native_module_bool_reference &
//  in string    | const c_native_module_string &       | c_native_module_string_reference
//  out string   | c_native_module_string &             | c_native_module_string_reference &
//  in real[]    | const c_native_module_real_array &   | const c_native_module_real_reference_array &
//  out real[]   | c_native_module_real_array &         | c_native_module_real_reference_array &
//  in bool[]    | const c_native_module_bool_array &   | const c_native_module_bool_reference_array &
//  out bool[]   | c_native_module_bool_array &         | c_native_module_bool_reference_array &
//  in string[]  | const c_native_module_string_array & | const c_native_module_string_reference_array &
//  out string[] | c_native_module_string_array &       | c_native_module_string_reference_array &

// $TODO make a real interface for strings and arrays which don't use raw std::string and std::vector

// String type used in native module compile-time functions
class c_native_module_string {
public:
	std::string &get_string();
	const std::string &get_string() const;
private:
	std::string m_string;
};

template<typename t_identifier>
class c_native_module_value_reference {
public:
#if IS_TRUE(ASSERTS_ENABLED)
	using t_opaque_data = uint64;
#else // IS_TRUE(ASSERTS_ENABLED)
	using t_opaque_data = uint32;
#endif // IS_TRUE(ASSERTS_ENABLED)

	c_native_module_value_reference()
		: m_opaque_data(static_cast<t_opaque_data>(-1)) {}

	c_native_module_value_reference(t_opaque_data opaque_data)
		: m_opaque_data(opaque_data) {}

	c_native_module_value_reference(const c_native_module_value_reference &) = default;
	c_native_module_value_reference(c_native_module_value_reference &&) = default;
	c_native_module_value_reference &operator=(const c_native_module_value_reference &) = default;
	c_native_module_value_reference &operator=(c_native_module_value_reference &&) = default;

	t_opaque_data get_opaque_data() const {
		return m_opaque_data;
	}

private:
	t_opaque_data m_opaque_data;
};

struct s_native_module_real_reference_identifier {};
using c_native_module_real_reference = c_native_module_value_reference<s_native_module_real_reference_identifier>;

struct s_native_module_bool_reference_identifier {};
using c_native_module_bool_reference = c_native_module_value_reference<s_native_module_bool_reference_identifier>;

struct s_native_module_string_reference_identifier {};
using c_native_module_string_reference = c_native_module_value_reference<s_native_module_string_reference_identifier>;

template<typename t_type>
class c_native_module_array {
public:
	std::vector<t_type> &get_array() {
		return m_array;
	}

	const std::vector<t_type> &get_array() const {
		return m_array;
	}

private:
	std::vector<t_type> m_array;
};

class c_native_module_real_array : public c_native_module_array<real32> {};
class c_native_module_bool_array : public c_native_module_array<bool> {};
class c_native_module_string_array : public c_native_module_array<std::string> {};

class c_native_module_real_reference_array : public c_native_module_array<c_native_module_real_reference> {};
class c_native_module_bool_reference_array : public c_native_module_array<c_native_module_bool_reference> {};
class c_native_module_string_reference_array : public c_native_module_array<c_native_module_string_reference> {};

// Used to convert a real into an array index
bool get_and_validate_native_module_array_index(real32 index_real, size_t array_count, size_t &index_out);
