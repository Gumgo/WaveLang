#pragma once

#include "common/common.h"

#include "native_module/native_module_compile_time_types.h"
#include "native_module/native_module_data_type.h"

#include <string>
#include <variant>
#include <vector>

struct s_instrument_globals;

static constexpr size_t k_max_native_module_library_name_length = 64;
static constexpr size_t k_max_native_module_name_length = 64;
static constexpr size_t k_max_native_module_arguments = 10;
static constexpr size_t k_max_native_module_argument_name_length = 20;
static constexpr size_t k_invalid_native_module_argument_index = static_cast<size_t>(-1);

using f_library_compiler_initializer = void *(*)();
using f_library_compiler_deinitializer = void (*)(void *library_context);

struct s_native_module_library {
	uint32 id = 0;
	c_static_string<k_max_native_module_library_name_length> name =
		c_static_string<k_max_native_module_library_name_length>::construct_empty();
	uint32 version = 0;

	f_library_compiler_initializer compiler_initializer = nullptr;
	f_library_compiler_deinitializer compiler_deinitializer = nullptr;
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

	constexpr s_native_module_uid()
		: data_uint64(0xffffffffffffffffull) {}

	constexpr bool operator==(const s_native_module_uid &other) const {
		return data_uint64 == other.data_uint64;
	}

	constexpr bool operator!=(const s_native_module_uid &other) const {
		return data_uint64 != other.data_uint64;
	}

	constexpr uint32 get_library_id() const {
		return big_to_native_endian(library_id);
	}

	constexpr uint32 get_module_id() const {
		return big_to_native_endian(module_id);
	}

	constexpr bool is_valid() const {
		return *this != invalid();
	}

	static constexpr s_native_module_uid build(uint32 library_id, uint32 module_id) {
		s_native_module_uid result;
		result.library_id = native_to_big_endian(library_id);
		result.module_id = native_to_big_endian(module_id);
		return result;
	}

	static constexpr s_native_module_uid invalid() {
		return {};
	}
};

// Certain native modules either have custom implementations or are used in a custom way in specific contexts
enum class e_native_module_intrinsic {
	k_invalid = -1,

	k_get_latency_real,
	k_get_latency_bool,
	k_get_latency_string,
	k_get_latency_real_array,
	k_get_latency_bool_array,
	k_get_latency_string_array,
	k_delay_samples_real,
	k_delay_samples_bool,

	k_count
};

// Fixed list of operators which are to be associated with native modules
enum class e_native_operator {
	k_invalid = -1,

	k_noop, // Special case "no-op" operator
	k_negation,
	k_addition,
	k_subtraction,
	k_multiplication,
	k_division,
	k_modulo,
	k_not,
	k_equal,
	k_not_equal,
	k_less,
	k_greater,
	k_less_equal,
	k_greater_equal,
	k_and,
	k_or,
	// $TODO add k_xor
	k_subscript,

	k_count
};

// Returns the native module name associated with the native operator. These are special pre-defined names which take
// the form "operator_<symbol>" and cannot be called directly from script since they are not valid identifiers but are
// used for module identification and in output.
const char *get_native_operator_native_module_name(e_native_operator native_operator);

// Argument for a compile-time native module call
struct s_native_module_compile_time_argument {
#if IS_TRUE(ASSERTS_ENABLED)
	e_native_module_argument_direction argument_direction;
	c_native_module_qualified_data_type type;
	e_native_module_data_access data_access;
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Don't use this directly
	std::variant<
		real32,
		bool,
		c_native_module_string,
		c_native_module_real_reference,
		c_native_module_bool_reference,
		c_native_module_string_reference,
		c_native_module_real_array,
		c_native_module_bool_array,
		c_native_module_string_array,
		c_native_module_real_reference_array,
		c_native_module_bool_reference_array,
		c_native_module_string_reference_array> value;

	// These functions are used to enforce correct usage of input or output

	real32 get_real_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<real32>(value);
	}

	real32 &get_real_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<real32>(value);
	}

	bool get_bool_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<bool>(value);
	}

	bool &get_bool_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<bool>(value);
	}

	const c_native_module_string &get_string_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_string>(value);
	}

	c_native_module_string &get_string_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_string>(value);
	}

	c_native_module_real_reference get_real_reference_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_real_reference>(value);
	}

	c_native_module_real_reference &get_real_reference_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_real_reference>(value);
	}

	c_native_module_bool_reference get_bool_reference_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_bool_reference>(value);
	}

	c_native_module_bool_reference &get_bool_reference_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_bool_reference>(value);
	}

	c_native_module_string_reference get_string_reference_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_string_reference>(value);
	}

	c_native_module_string_reference &get_string_reference_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(!type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_string_reference>(value);
	}

	const c_native_module_real_array &get_real_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_real_array>(value);
	}

	c_native_module_real_array &get_real_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_real_array>(value);
	}

	const c_native_module_bool_array &get_bool_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_bool_array>(value);
	}

	c_native_module_bool_array &get_bool_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_bool_array>(value);
	}

	const c_native_module_string_array &get_string_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_string_array>(value);
	}

	c_native_module_string_array &get_string_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_value);
		return std::get<c_native_module_string_array>(value);
	}

	const c_native_module_real_reference_array &get_real_reference_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_real_reference_array>(value);
	}

	c_native_module_real_reference_array &get_real_reference_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_real);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_real_reference_array>(value);
	}

	const c_native_module_bool_reference_array &get_bool_reference_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_bool_reference_array>(value);
	}

	c_native_module_bool_reference_array &get_bool_reference_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_bool);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_bool_reference_array>(value);
	}

	const c_native_module_string_reference_array &get_string_reference_array_in() const {
		wl_assert(argument_direction == e_native_module_argument_direction::k_in);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_string_reference_array>(value);
	}

	c_native_module_string_reference_array &get_string_reference_array_out() {
		wl_assert(argument_direction == e_native_module_argument_direction::k_out);
		wl_assert(type.get_primitive_type() == e_native_module_primitive_type::k_string);
		wl_assert(type.is_array());
		wl_assert(data_access == e_native_module_data_access::k_reference);
		return std::get<c_native_module_string_reference_array>(value);
	}
};

// A function which can be called at compile-time to resolve the value(s) of native module calls with constant inputs
using c_native_module_compile_time_argument_list = c_wrapped_array<s_native_module_compile_time_argument>;

class c_native_module_diagnostic_interface {
public:
	virtual void message(const char *format, ...) = 0;
	virtual void warning(const char *format, ...) = 0;
	virtual void error(const char *format, ...) = 0;
};

class c_native_module_reference_interface {
public:
	virtual c_native_module_real_reference create_constant_reference(real32 value) = 0;
	virtual c_native_module_bool_reference create_constant_reference(bool value) = 0;
	virtual c_native_module_string_reference create_constant_reference(const char *value) = 0;
};

struct s_native_module_context {
	c_native_module_diagnostic_interface *diagnostic_interface;
	c_native_module_reference_interface *reference_interface;
	const s_instrument_globals *instrument_globals;

	uint32 upsample_factor;	// Upsample factor this module is being run at
	uint32 sample_rate;		// Sample rate (after upsampling), or 0 if sample rate isn't available at compile-time
	void *library_context;

	c_native_module_compile_time_argument_list arguments;
};

// Function executed at compile-time to evaluate the native module
using f_native_module_compile_time_call = void (*)(const s_native_module_context &context);

// Function called to raise errors if any arguments to the native module are invalid
using f_native_module_validate_arguments = void (*)(const s_native_module_context &context);

// Returns the amount of latency caused by this native module
using f_native_module_get_latency = int32 (*)(const s_native_module_context &context);

struct s_native_module_argument {
	c_static_string<k_max_native_module_argument_name_length> name =
		c_static_string< k_max_native_module_argument_name_length>::construct_empty();
	e_native_module_argument_direction argument_direction = e_native_module_argument_direction::k_invalid;
	c_native_module_qualified_data_type type = c_native_module_qualified_data_type::invalid();
	e_native_module_data_access data_access = e_native_module_data_access::k_invalid;
};

struct s_native_module {
	// Unique identifier for this native module
	s_native_module_uid uid = s_native_module_uid::invalid();

	// Name of the native module
	c_static_string<k_max_native_module_name_length> name =
		c_static_string< k_max_native_module_name_length>::construct_empty();
	
	// Number of arguments
	size_t argument_count = 0;

	// Index of the argument representing the return value when called from script, or
	// k_invalid_native_module_argument_index
	size_t return_argument_index = k_invalid_native_module_argument_index;

	// List of arguments
	s_static_array<s_native_module_argument, k_max_native_module_arguments> arguments{};

	// Function to resolve the module at compile time if all inputs are available, or null if this is not possible
	f_native_module_compile_time_call compile_time_call = nullptr;

	// Function to validate arguments
	f_native_module_validate_arguments validate_arguments = nullptr;

	// Returns the amount of latency caused by this native module
	f_native_module_get_latency get_latency = nullptr;
};

bool is_native_module_argument_always_available_at_compile_time(const s_native_module_argument &native_module_argument);

void get_native_module_compile_time_properties(
	const s_native_module &native_module,
	bool &always_runs_at_compile_time_out,
	bool &always_runs_at_compile_time_if_dependent_constants_are_constant_out);

bool validate_native_module(const s_native_module &native_module);

// Optimization rules

// An optimization rule consists of two "patterns", a source and target. Each pattern represents a graph of native
// module operations. If the source pattern is identified in the graph, it is replaced with the target pattern.

enum class e_native_module_optimization_symbol_type {
	k_invalid,

	k_native_module,
	k_native_module_end,
	k_variable,
	k_constant,
	k_real_value,
	k_bool_value,
	//k_string_value, // Probably never needed, since strings are always constant

	k_count
};

static constexpr size_t k_max_native_module_optimization_pattern_length = 16;

struct s_native_module_optimization_symbol {
	static constexpr uint32 k_max_matched_symbols = 4;

	union u_data {
		u_data() {}

		s_native_module_uid native_module_uid;
		uint32 index; // Symbol index if an variable or constant
		real32 real_value;
		bool bool_value;
		// Currently no string value (likely will never be needed because all strings are compile-time resolved)
	};

	e_native_module_optimization_symbol_type type;
	u_data data;

	static s_native_module_optimization_symbol invalid() {
		s_native_module_optimization_symbol result;
		zero_type(&result);
		return result;
	}

	bool is_valid() const {
		return type != e_native_module_optimization_symbol_type::k_invalid;
	}

	static s_native_module_optimization_symbol build_native_module(s_native_module_uid native_module_uid) {
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_native_module;
		result.data.native_module_uid = native_module_uid;
		return result;
	}

	static s_native_module_optimization_symbol build_native_module_end() {
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_native_module_end;
		return result;
	}

	static s_native_module_optimization_symbol build_variable(uint32 index) {
		wl_assert(valid_index(index, k_max_matched_symbols));
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_variable;
		result.data.index = index;
		return result;
	}

	static s_native_module_optimization_symbol build_constant(uint32 index) {
		wl_assert(valid_index(index, k_max_matched_symbols));
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_constant;
		result.data.index = index;
		return result;
	}

	static s_native_module_optimization_symbol build_real_value(real32 real_value) {
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_real_value;
		result.data.real_value = real_value;
		return result;
	}

	static s_native_module_optimization_symbol build_bool_value(bool bool_value) {
		s_native_module_optimization_symbol result;
		zero_type(&result);
		result.type = e_native_module_optimization_symbol_type::k_bool_value;
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
