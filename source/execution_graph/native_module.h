#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module_compile_time_types.h"

#include <string>
#include <vector>

static const size_t k_max_native_module_library_name_length = 64;
static const size_t k_max_native_module_name_length = 64;
static const size_t k_max_native_module_arguments = 10;
static const size_t k_max_native_module_argument_name_length = 16;
static const size_t k_invalid_argument_index = static_cast<size_t>(-1);

struct s_native_module_library {
	uint32 id;
	c_static_string<k_max_native_module_library_name_length> name;
	uint32 version;
};

// Unique identifier for each native module
struct s_native_module_uid {
	// Unique identifier consists of 8 bytes
	// First 4 bytes is "library_id", stored in big endian order. IDs 0-255 are reserved for core libraries.
	// Last 4 bytes is "module_id", stored in big endian order. Used to identify a module within the library.
	union {
		uint8 data[8];
		uint64 data_uint64;
		struct {
			uint32 library_id;
			uint32 module_id;
		};
	};

	bool operator==(const s_native_module_uid &other) const {
		return data_uint64 == other.data_uint64;
	}

	bool operator!=(const s_native_module_uid &other) const {
		return data_uint64 != other.data_uint64;
	}

	uint32 get_library_id() const {
		return big_to_native_endian(library_id);
	}

	uint32 get_module_id() const {
		return big_to_native_endian(module_id);
	}

	static s_native_module_uid build(uint32 library_id, uint32 module_id) {
		s_native_module_uid result;
		result.library_id = native_to_big_endian(library_id);
		result.module_id = native_to_big_endian(module_id);
		return result;
	}

	static const s_native_module_uid k_invalid;
};

// Fixed list of operators which are to be associated with native modules
enum e_native_operator {
	k_native_operator_invalid = -1,

	k_native_operator_noop, // Special case "no-op" operator
	k_native_operator_negation,
	k_native_operator_addition,
	k_native_operator_subtraction,
	k_native_operator_multiplication,
	k_native_operator_division,
	k_native_operator_modulo,
	k_native_operator_not,
	k_native_operator_equal,
	k_native_operator_not_equal,
	k_native_operator_less,
	k_native_operator_greater,
	k_native_operator_less_equal,
	k_native_operator_greater_equal,
	k_native_operator_and,
	k_native_operator_or,
	k_native_operator_array_dereference,

	k_native_operator_count
};

// Returns the native module name associated with the native operator. These are special pre-defined names which take
// the form "operator_<symbol>" and cannot be called directly from script since they are not valid identifiers but are
// used for module identification and in output.
const char *get_native_operator_native_module_name(e_native_operator native_operator);

enum e_native_module_qualifier {
	k_native_module_qualifier_in,
	k_native_module_qualifier_out,

	// Same as input argument, except the value must resolve to a compile-time constant
	k_native_module_qualifier_constant,

	k_native_module_qualifier_count
};

// Many cases accept both "in" and "constant", so use this utility function for those
inline bool native_module_qualifier_is_input(e_native_module_qualifier qualifier) {
	return (qualifier == k_native_module_qualifier_in) || (qualifier == k_native_module_qualifier_constant);
}

enum e_native_module_primitive_type {
	k_native_module_primitive_type_real,
	k_native_module_primitive_type_bool,
	k_native_module_primitive_type_string,

	k_native_module_primitive_type_count
};

struct s_native_module_primitive_type_traits {
	bool is_dynamic;	// Whether this is a runtime-dynamic type
};

class c_native_module_data_type {
public:
	c_native_module_data_type();
	c_native_module_data_type(e_native_module_primitive_type primitive_type, bool is_array = false);
	static c_native_module_data_type invalid();

	bool is_valid() const;

	e_native_module_primitive_type get_primitive_type() const;
	const s_native_module_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_native_module_data_type get_element_type() const;
	c_native_module_data_type get_array_type() const;

	bool operator==(const c_native_module_data_type &other) const;
	bool operator!=(const c_native_module_data_type &other) const;

	uint32 write() const;
	bool read(uint32 data);

private:
	enum e_flag {
		k_flag_is_array,

		k_flag_count
	};

	e_native_module_primitive_type m_primitive_type;
	uint32 m_flags;
};

class c_native_module_qualified_data_type {
public:
	c_native_module_qualified_data_type();
	c_native_module_qualified_data_type(c_native_module_data_type data_type, e_native_module_qualifier qualifier);
	static c_native_module_qualified_data_type invalid();

	bool is_valid() const;

	c_native_module_data_type get_data_type() const;
	e_native_module_qualifier get_qualifier() const;

	bool operator==(const c_native_module_qualified_data_type &other) const;
	bool operator!=(const c_native_module_qualified_data_type &other) const;

private:
	c_native_module_data_type m_data_type;
	e_native_module_qualifier m_qualifier;
};

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type);

struct s_native_module_argument {
	c_static_string<k_max_native_module_argument_name_length> name;
	c_native_module_qualified_data_type type;
};

// Argument for a compile-time native module call
struct s_native_module_compile_time_argument {
#if IS_TRUE(ASSERTS_ENABLED)
	c_native_module_qualified_data_type type;
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Don't use these directly
	union {
		real32 real_value;
		bool bool_value;
	};
	c_native_module_string string_value;
	c_native_module_array array_value; // Array of value node indices for all types - use hacky, but safe, cast

	// These functions are used to enforce correct usage of input or output

	real32 get_real_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_real));
		return real_value;
	}

	real32 &get_real_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_real));
		return real_value;
	}

	bool get_bool_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_bool));
		return bool_value;
	}

	bool &get_bool_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_bool));
		return bool_value;
	}

	const c_native_module_string &get_string_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_string));
		return string_value;
	}

	c_native_module_string &get_string_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_string));
		return string_value;
	}

	const c_native_module_real_array &get_real_array_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_real, true));
		return *static_cast<const c_native_module_real_array *>(&array_value);
	}

	c_native_module_real_array &get_real_array_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_real, true));
		return *static_cast<c_native_module_real_array *>(&array_value);
	}

	const c_native_module_bool_array &get_bool_array_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_bool, true));
		return *static_cast<const c_native_module_bool_array *>(&array_value);
	}

	c_native_module_bool_array &get_bool_array_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_bool, true));
		return *static_cast<c_native_module_bool_array *>(&array_value);
	}

	const c_native_module_string_array &get_string_array_in() const {
		wl_assert(native_module_qualifier_is_input(type.get_qualifier()));
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_string, true));
		return *static_cast<const c_native_module_string_array *>(&array_value);
	}

	c_native_module_string_array &get_string_array_out() {
		wl_assert(type.get_qualifier() == k_native_module_qualifier_out);
		wl_assert(type.get_data_type() == c_native_module_data_type(k_native_module_primitive_type_string, true));
		return *static_cast<c_native_module_string_array *>(&array_value);
	}
};

// A function which can be called at compile-time to resolve the value(s) of native module calls with constant inputs
typedef c_wrapped_array<s_native_module_compile_time_argument> c_native_module_compile_time_argument_list;
typedef void (*f_native_module_compile_time_call)(c_native_module_compile_time_argument_list arguments);

struct s_native_module {
	// Unique identifier for this native module
	s_native_module_uid uid;

	// Name of the native module
	c_static_string<k_max_native_module_name_length> name;
	
	// Number of arguments
	size_t argument_count;
	size_t in_argument_count;
	size_t out_argument_count;

	// Index of the argument representing the return value when called from script, or k_invalid_argument_index
	size_t return_argument_index;

	// List of arguments
	s_static_array<s_native_module_argument, k_max_native_module_arguments> arguments;

	// Function to resolve the module at compile time if all inputs are constant, or null if this is not possible
	f_native_module_compile_time_call compile_time_call;
};

// Helpers to translate between nth argument and mth input/output
size_t get_native_module_input_index_for_argument_index(
	const s_native_module &native_module, size_t argument_index);
size_t get_native_module_output_index_for_argument_index(
	const s_native_module &native_module, size_t argument_index);

// Optimization rules

// An optimization rule consists of two "patterns", a source and target. Each pattern represents a graph of native
// module operations. If the source pattern is identified in the graph, it is replaced with the target pattern.

enum e_native_module_optimization_symbol_type {
	k_native_module_optimization_symbol_type_invalid,

	k_native_module_optimization_symbol_type_native_module,
	k_native_module_optimization_symbol_type_native_module_end,
	k_native_module_optimization_symbol_type_array_dereference,
	k_native_module_optimization_symbol_type_variable,
	k_native_module_optimization_symbol_type_constant,
	k_native_module_optimization_symbol_type_real_value,
	k_native_module_optimization_symbol_type_bool_value,
	//k_native_module_optimization_symbol_type_string_value, // Probably never needed, since strings are always constant

	k_native_module_optimization_symbol_type_count
};

static const size_t k_max_native_module_optimization_pattern_length = 16;

struct s_native_module_optimization_symbol {
	static const uint32 k_max_matched_symbols = 4;

	e_native_module_optimization_symbol_type type;
	union {
		s_native_module_uid native_module_uid;
		uint32 index; // Symbol index if an variable or constant
		real32 real_value;
		bool bool_value;
		// Currently no string value (likely will never be needed because all strings are compile-time resolved)
	} data;

	static s_native_module_optimization_symbol invalid() {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		return result;
	}

	bool is_valid() const {
		return type != k_native_module_optimization_symbol_type_invalid;
	}

	static s_native_module_optimization_symbol build_native_module(s_native_module_uid native_module_uid) {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_native_module;
		result.data.native_module_uid = native_module_uid;
		return result;
	}

	static s_native_module_optimization_symbol build_array_dereference() {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_array_dereference;
		return result;
	}

	static s_native_module_optimization_symbol build_native_module_end() {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_native_module_end;
		return result;
	}

	static s_native_module_optimization_symbol build_variable(uint32 index) {
		wl_assert(VALID_INDEX(index, k_max_matched_symbols));
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_variable;
		result.data.index = index;
		return result;
	}

	static s_native_module_optimization_symbol build_constant(uint32 index) {
		wl_assert(VALID_INDEX(index, k_max_matched_symbols));
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_constant;
		result.data.index = index;
		return result;
	}

	static s_native_module_optimization_symbol build_real_value(real32 real_value) {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_real_value;
		result.data.real_value = real_value;
		return result;
	}

	static s_native_module_optimization_symbol build_bool_value(bool bool_value) {
		s_native_module_optimization_symbol result;
		ZERO_STRUCT(&result);
		result.type = k_native_module_optimization_symbol_type_bool_value;
		result.data.bool_value = bool_value;
		return result;
	}
};

struct s_native_module_optimization_pattern {
	s_static_array<s_native_module_optimization_symbol, k_max_native_module_optimization_pattern_length> symbols;
};

struct s_native_module_optimization_rule {
	s_native_module_optimization_pattern source;
	s_native_module_optimization_pattern target;
};

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_H__
