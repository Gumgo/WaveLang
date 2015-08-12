#ifndef WAVELANG_NATIVE_MODULES_H__
#define WAVELANG_NATIVE_MODULES_H__

#include "common/common.h"
#include <vector>

// Defines the registry of native modules
// For the functions which can be resolved at compile-time with constant inputs, a function to do this is provided

// List of native modules available at compile time
// INSTRUCTIONS FOR ADDING A NEW NATIVE MODULE:
// 1) Add the native module to this enum and create a definition for it in native_modules.cpp.
// 2) If the function can be resolved at compile time given constant inputs, write a compile_time_call function.
// 3) If the native module has optimizations (e.g. x + 0 => x), add these to execution_graph_optimization_rules.txt.
// 4) Add a set of tasks for the native module in task_functions.h/cpp.
// 5) Modify the task mapping function in native_module_task_mapping.cpp to include the new native module and tasks.
enum e_native_module {
	k_native_module_noop,

	// Basic operators/arithmetic
	k_native_module_negation,
	k_native_module_addition,
	k_native_module_subtraction,
	k_native_module_multiplication,
	k_native_module_division,
	k_native_module_modulo,

	// Utility functions
	k_native_module_abs,
	k_native_module_floor,
	k_native_module_ceil,
	k_native_module_round,
	k_native_module_min,
	k_native_module_max,
	k_native_module_exp,
	k_native_module_log,
	k_native_module_sqrt,
	k_native_module_pow,

	// $TODO trig functions

	// $TODO temporary for testing
	k_native_module_test,
	k_native_module_delay_test,

	k_native_module_count
};

enum e_native_module_argument_qualifier {
	k_native_module_argument_qualifier_in,
	k_native_module_argument_qualifier_out,

	// Same as input argument, except the value must resolve to a compile-time constant
	k_native_module_argument_qualifier_constant,

	k_native_module_argument_qualifier_count
};

enum e_native_module_argument_type {
	k_native_module_argument_type_real,
	k_native_module_argument_type_string,

	k_native_module_argument_type_count
};

struct s_native_module_argument {
	e_native_module_argument_qualifier qualifier;
	e_native_module_argument_type type;
};

// Return value for a compile-time native module call
struct s_native_module_compile_time_argument {
#if PREDEFINED(ASSERTS_ENABLED)
	s_native_module_argument argument;
	bool assigned;
#endif // PREDEFINED(ASSERTS_ENABLED)

	real32 real_value;
	std::string string_value;

	// These functions are used to enforce correct usage of input or output

	operator real32() const {
		wl_assert(
			argument.qualifier == k_native_module_argument_qualifier_in ||
			argument.qualifier == k_native_module_argument_qualifier_constant);
		wl_assert(argument.type == k_native_module_argument_type_real);
		return real_value;
	}

	real32 operator=(real32 value) {
		wl_assert(argument.qualifier == k_native_module_argument_qualifier_out);
		wl_assert(argument.type == k_native_module_argument_type_real);
		this->real_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return value;
	}

	operator std::string() const {
		wl_assert(
			argument.qualifier == k_native_module_argument_qualifier_in ||
			argument.qualifier == k_native_module_argument_qualifier_constant);
		wl_assert(argument.type == k_native_module_argument_type_string);
		return string_value;
	}

	const std::string &operator=(const std::string &value) {
		wl_assert(argument.qualifier == k_native_module_argument_qualifier_out);
		wl_assert(argument.type == k_native_module_argument_type_string);
		this->string_value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return string_value;
	}

private:
	// Hide this operator so it is never accidentally used
	s_native_module_compile_time_argument operator=(s_native_module_compile_time_argument other);
};

// A function which can be called at compile-time to resolve the value(s) of native module calls with constant inputs
typedef std::vector<s_native_module_compile_time_argument> c_native_module_compile_time_argument_list;
typedef void (*f_native_module_compile_time_call)(c_native_module_compile_time_argument_list &arguments);

static const size_t k_max_native_module_arguments = 10;

struct s_native_module {
	// Name of the native module
	const char *name;

	// If true, the first output argument routes to the return value when called from code
	bool first_output_is_return;

	// Number of arguments
	size_t argument_count;
	size_t in_argument_count;
	size_t out_argument_count;

	// List of arguments
	s_native_module_argument arguments[k_max_native_module_arguments];

	// Function to resolve the module at compile time if all inputs are constant, or null if this is not possible
	f_native_module_compile_time_call compile_time_call;
};

class c_native_module_registry {
public:
	static uint32 get_native_module_count();
	static const s_native_module &get_native_module(uint32 index);
	static uint32 get_native_module_index(const char *name);

	// Operator names
	static const char *k_operator_noop_name;
	static const char *k_operator_negation_name;
	static const char *k_operator_addition_operator_name;
	static const char *k_operator_subtraction_operator_name;
	static const char *k_operator_multiplication_operator_name;
	static const char *k_operator_division_name;
	static const char *k_operator_modulo_name;
};

#endif // WAVELANG_NATIVE_MODULES_H__