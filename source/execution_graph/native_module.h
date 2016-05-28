#ifndef WAVELANG_NATIVE_MODULE_H__
#define WAVELANG_NATIVE_MODULE_H__

#include "common/common.h"
#include <vector>

static const size_t k_max_native_module_library_name_length = 64;
static const size_t k_max_native_module_name_length = 64;
static const size_t k_max_native_module_arguments = 10;
static const size_t k_max_native_module_argument_name_length = 16;

#define NATIVE_PREFIX "__native_"

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

const s_native_module_primitive_type_traits &get_native_module_primitive_type_traits(
	e_native_module_primitive_type primitive_type);

struct s_native_module_argument {
	e_native_module_qualifier qualifier;
	c_native_module_data_type type;

	static s_native_module_argument build(
		e_native_module_qualifier qualifier,
		c_native_module_data_type type) {
		s_native_module_argument result = { qualifier, type };
		return result;
	}
 };

struct s_native_module_named_argument {
	s_native_module_argument argument;
	c_static_string<k_max_native_module_argument_name_length> name;

	static s_native_module_named_argument build(
		e_native_module_qualifier qualifier,
		c_native_module_data_type type,
		const char *name) {
		s_native_module_named_argument result = {
			s_native_module_argument::build(qualifier, type),
			c_static_string<k_max_native_module_argument_name_length>::construct_verify(name)
		};
		return result;
	}
};

// Return value for a compile-time native module call
// $TODO do we want to expose std::vector for arrays? Consider making a wrapper around it
struct s_native_module_compile_time_argument {
#if PREDEFINED(ASSERTS_ENABLED)
	s_native_module_argument argument;
	bool assigned;
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Don't use these directly
	union {
		real32 real_value;
		bool bool_value;
	};
	std::string string_value;
	std::vector<uint32> array_value; // Array of value node indices

	// These functions are used to enforce correct usage of input or output

	real32 get_real() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_real));
		return real_value;
	}

	real32 operator=(real32 value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_real));
		this->real_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return value;
	}

	bool get_bool() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_bool));
		return bool_value;
	}

	bool operator=(bool value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_bool));
		this->bool_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return value;
	}

	const std::string &get_string() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_string));
		return string_value;
	}

	const std::string &operator=(const std::string &value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_real));
		this->string_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return string_value;
	}

	const std::vector<uint32> &get_real_array() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_real, true));
		return array_value;
	}

	void set_real_array(const std::vector<uint32> &value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_real, true));
		this->array_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
	}

	const std::vector<uint32> &get_bool_array() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_bool, true));
		return array_value;
	}

	void set_bool_array(const std::vector<uint32> &value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_bool, true));
		this->array_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
	}

	const std::vector<uint32> &get_string_array() const {
		wl_assert(native_module_qualifier_is_input(argument.qualifier));
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_string, true));
		return array_value;
	}

	void set_string_array(const std::vector<uint32> &value) {
		wl_assert(argument.qualifier == k_native_module_qualifier_out);
		wl_assert(argument.type == c_native_module_data_type(k_native_module_primitive_type_string, true));
		this->array_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
	}

private:
	// Hide this operator so it is never accidentally used
	s_native_module_compile_time_argument operator=(s_native_module_compile_time_argument other);
};

// A function which can be called at compile-time to resolve the value(s) of native module calls with constant inputs
typedef c_wrapped_array<s_native_module_compile_time_argument> c_native_module_compile_time_argument_list;
typedef void (*f_native_module_compile_time_call)(c_native_module_compile_time_argument_list arguments);

// Shorthand for argument specification
#define NMA(qualifier, type, name) s_native_module_named_argument::build(	\
	k_native_module_qualifier_ ## qualifier,								\
	c_native_module_data_type(k_native_module_primitive_type_ ## type),		\
	name)
#define NMA_ARRAY(qualifier, type, name) s_native_module_named_argument::build(	\
	k_native_module_qualifier_ ## qualifier,									\
	c_native_module_data_type(k_native_module_primitive_type_ ## type, true),	\
	name)

struct s_native_module_argument_list {
	static const s_native_module_named_argument k_argument_none;
	s_static_array<s_native_module_named_argument, k_max_native_module_arguments> arguments;

	static s_native_module_argument_list build(
		s_native_module_named_argument arg_0 = k_argument_none,
		s_native_module_named_argument arg_1 = k_argument_none,
		s_native_module_named_argument arg_2 = k_argument_none,
		s_native_module_named_argument arg_3 = k_argument_none,
		s_native_module_named_argument arg_4 = k_argument_none,
		s_native_module_named_argument arg_5 = k_argument_none,
		s_native_module_named_argument arg_6 = k_argument_none,
		s_native_module_named_argument arg_7 = k_argument_none,
		s_native_module_named_argument arg_8 = k_argument_none,
		s_native_module_named_argument arg_9 = k_argument_none);
};

struct s_native_module {
	// Unique identifier for this native module
	s_native_module_uid uid;

	// Name of the native module
	c_static_string<k_max_native_module_name_length> name;

	// If true, the first output argument routes to the return value when called from code
	bool first_output_is_return;

	// Number of arguments
	size_t argument_count;
	size_t in_argument_count;
	size_t out_argument_count;

	// List of arguments
	s_static_array<s_native_module_argument, k_max_native_module_arguments> arguments;
	s_static_array<c_static_string<k_max_native_module_argument_name_length>, k_max_native_module_arguments>
		argument_names;

	// Function to resolve the module at compile time if all inputs are constant, or null if this is not possible
	f_native_module_compile_time_call compile_time_call;

	static s_native_module build(
		s_native_module_uid uid,
		const char *name,
		bool first_output_is_return,
		const s_native_module_argument_list &arguments,
		f_native_module_compile_time_call compile_time_call);
};

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
		// Currently no string value (likely will never be needed)
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

	static s_native_module_optimization_pattern build(
		s_native_module_optimization_symbol sym_0 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_1 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_2 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_3 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_4 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_5 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_6 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_7 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_8 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_9 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_10 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_11 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_12 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_13 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_14 = s_native_module_optimization_symbol::invalid(),
		s_native_module_optimization_symbol sym_15 = s_native_module_optimization_symbol::invalid()) {
		static_assert(k_max_native_module_optimization_pattern_length == 16, "Max optimization pattern length changed");
		s_native_module_optimization_pattern result;
		result.symbols[0] = sym_0;
		result.symbols[1] = sym_1;
		result.symbols[2] = sym_2;
		result.symbols[3] = sym_3;
		result.symbols[4] = sym_4;
		result.symbols[5] = sym_5;
		result.symbols[6] = sym_6;
		result.symbols[7] = sym_7;
		result.symbols[8] = sym_8;
		result.symbols[9] = sym_9;
		result.symbols[10] = sym_10;
		result.symbols[11] = sym_11;
		result.symbols[12] = sym_12;
		result.symbols[13] = sym_13;
		result.symbols[14] = sym_14;
		result.symbols[15] = sym_15;
		return result;
	}
};

struct s_native_module_optimization_rule {
	s_native_module_optimization_pattern source;
	s_native_module_optimization_pattern target;

	static s_native_module_optimization_rule build(
		const s_native_module_optimization_pattern &source,
		const s_native_module_optimization_pattern &target) {
		s_native_module_optimization_rule result;
		memcpy(&result.source, &source, sizeof(result.source));
		memcpy(&result.target, &target, sizeof(result.target));
		return result;
	}
};

// Macros for building optimization rules
#define NMO_BEGIN(uid)		s_native_module_optimization_symbol::build_native_module(uid)
#define NMO_END				s_native_module_optimization_symbol::build_native_module_end()
#define NMO_NM(uid, ...)	NMO_BEGIN(uid), ##__VA_ARGS__, NMO_END
#define NMO_DRF(arr, idx)	s_native_module_optimization_symbol::build_array_dereference(), arr, idx
#define NMO_V(index)		s_native_module_optimization_symbol::build_variable(index)
#define NMO_C(index)		s_native_module_optimization_symbol::build_constant(index)
#define NMO_RV(value)		s_native_module_optimization_symbol::build_real_value(value)
#define NMO_BV(value)		s_native_module_optimization_symbol::build_bool_value(value)

#endif // WAVELANG_NATIVE_MODULE_H__