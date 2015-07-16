#ifndef WAVELANG_NATIVE_MODULES_H__
#define WAVELANG_NATIVE_MODULES_H__

#include "common/common.h"
#include <vector>

// Defines the registry of native modules
// For the functions which can be resolved at compile-time with constant inputs, a function to do this is provided

enum e_native_module_argument_type {
	k_native_module_argument_type_in,
	k_native_module_argument_type_out,

	k_native_module_argument_type_count
};

// Return value for a compile-time native module call
struct s_native_module_compile_time_argument {
#if PREDEFINED(ASSERTS_ENABLED)
	bool is_input;
	bool assigned;
#endif // PREDEFINED(ASSERTS_ENABLED)

	real32 value;

	// These functions are used to enforce correct usage of input or output

	operator real32() const {
		wl_assert(is_input);
		return value;
	}

	real32 operator=(real32 value) {
		wl_assert(!is_input);
		this->value = value;
		IF_ASSERTS_ENABLED(assigned = true);
		return value;
	}

private:
	// Hide this operator so it is never accidentally used
	s_native_module_compile_time_argument operator=(s_native_module_compile_time_argument other);
};

// A function which can be called at compile-time to resolve the value(s) of native module calls with constant inputs
typedef std::vector<s_native_module_compile_time_argument> c_native_module_compile_time_argument_list;
typedef void (*f_native_module_compile_time_call)(c_native_module_compile_time_argument_list &arguments);

const size_t k_max_native_module_arguments = 10;

// List of native modules available at compile time
enum e_native_module {
	k_native_module_noop,
	k_native_module_negation,
	k_native_module_addition,
	k_native_module_subtraction,
	k_native_module_multiplication,
	k_native_module_division,
	k_native_module_modulo,

	// $TODO temporary for testing
	k_native_module_test,

	k_native_module_count
};

struct s_native_module {
	// Name of the native module
	const char *name;

	// If true, the first output argument routes to the return value when called from code
	bool first_output_is_return;

	// Number of arguments
	size_t argument_count;
	size_t in_argument_count;
	size_t out_argument_count;

	// Type of each argument (in or out)
	e_native_module_argument_type argument_types[k_max_native_module_arguments];

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