#ifndef WAVELANG_TASK_FUNCTIONS_H__
#define WAVELANG_TASK_FUNCTIONS_H__

#include "common/common.h"

class c_buffer;

// List of all task functions
// buffer = a buffer input
// bufferio = a buffer inout
// constant = a constant input
enum e_task_function {
	k_task_function_negation_buffer,
	k_task_function_negation_bufferio,

	k_task_function_addition_buffer_buffer,
	k_task_function_addition_bufferio_buffer,
	k_task_function_addition_buffer_constant,
	k_task_function_addition_bufferio_constant,

	k_task_function_subtraction_buffer_buffer,
	k_task_function_subtraction_bufferio_buffer,
	k_task_function_subtraction_buffer_bufferio,
	k_task_function_subtraction_buffer_constant,
	k_task_function_subtraction_bufferio_constant,
	k_task_function_subtraction_constant_buffer,
	k_task_function_subtraction_constant_bufferio,

	k_task_function_multiplication_buffer_buffer,
	k_task_function_multiplication_bufferio_buffer,
	k_task_function_multiplication_buffer_constant,
	k_task_function_multiplication_bufferio_constant,

	k_task_function_division_buffer_buffer,
	k_task_function_division_bufferio_buffer,
	k_task_function_division_buffer_bufferio,
	k_task_function_division_buffer_constant,
	k_task_function_division_bufferio_constant,
	k_task_function_division_constant_buffer,
	k_task_function_division_constant_bufferio,

	k_task_function_modulo_buffer_buffer,
	k_task_function_modulo_bufferio_buffer,
	k_task_function_modulo_buffer_bufferio,
	k_task_function_modulo_buffer_constant,
	k_task_function_modulo_bufferio_constant,
	k_task_function_modulo_constant_buffer,
	k_task_function_modulo_constant_bufferio,

	k_task_function_abs_buffer,
	k_task_function_abs_bufferio,

	k_task_function_floor_buffer,
	k_task_function_floor_bufferio,

	k_task_function_ceil_buffer,
	k_task_function_ceil_bufferio,

	k_task_function_round_buffer,
	k_task_function_round_bufferio,

	k_task_function_min_buffer_buffer,
	k_task_function_min_bufferio_buffer,
	k_task_function_min_buffer_constant,
	k_task_function_min_bufferio_constant,

	k_task_function_max_buffer_buffer,
	k_task_function_max_bufferio_buffer,
	k_task_function_max_buffer_constant,
	k_task_function_max_bufferio_constant,

	k_task_function_exp_buffer,
	k_task_function_exp_bufferio,

	k_task_function_log_buffer,
	k_task_function_log_bufferio,

	k_task_function_sqrt_buffer,
	k_task_function_sqrt_bufferio,

	k_task_function_pow_buffer_buffer,
	k_task_function_pow_bufferio_buffer,
	k_task_function_pow_buffer_bufferio,
	k_task_function_pow_buffer_constant,
	k_task_function_pow_bufferio_constant,
	k_task_function_pow_constant_buffer,
	k_task_function_pow_constant_bufferio,

	// $TODO temporary
	k_task_function_test,
	k_task_function_test_c,
	k_task_function_test_delay,

	k_task_function_count
};

enum e_task_data_type {
	k_task_data_type_real_buffer_in,
	k_task_data_type_real_buffer_out,
	k_task_data_type_real_buffer_inout,
	k_task_data_type_real_constant_in,
	k_task_data_type_string_constant_in,

	k_task_data_type_count
};

struct s_task_function_argument {
#if PREDEFINED(ASSERTS_ENABLED)
	e_task_data_type type;
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Do not access these directly
	union {
		const c_buffer *real_buffer_in;
		c_buffer *real_buffer_out;
		c_buffer *real_buffer_inout;
		real32 real_constant_in;
		const char *string_constant_in;
	} data;

	const c_buffer *get_real_buffer_in() const {
		wl_assert(type == k_task_data_type_real_buffer_in);
		return data.real_buffer_in;
	}

	c_buffer *get_real_buffer_out() const {
		wl_assert(type == k_task_data_type_real_buffer_out);
		return data.real_buffer_out;
	}

	c_buffer *get_real_buffer_inout() const {
		wl_assert(type == k_task_data_type_real_buffer_inout);
		return data.real_buffer_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(type == k_task_data_type_real_constant_in);
		return data.real_constant_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(type == k_task_data_type_string_constant_in);
		return data.string_constant_in;
	}
};

typedef c_wrapped_array_const<s_task_function_argument> c_task_function_arguments;

struct s_task_function_context {
	uint32 buffer_size;
	void *task_memory;
	// $TODO more things like timing

	c_task_function_arguments arguments;
};

typedef void (*f_task_function)(const s_task_function_context &context);

// This function takes the list of arguments and returns the amount of memory the task requires
// Only the constant arguments are available - any buffer arguments are null
typedef size_t (*f_task_memory_query)(c_task_function_arguments arguments);

static const size_t k_max_task_function_arguments = 10;

struct s_task_function_description {
	// Function to execute
	f_task_function function;

	// Memory query function, or null
	f_task_memory_query memory_query;

	// Number of arguments
	uint32 argument_count;

	// Type of each argument
	e_task_data_type argument_types[k_max_task_function_arguments];

	// $TODO more data about how this task is allowed to be optimized (e.g. accumulator)
};

class c_task_function_registry {
public:
	static uint32 get_task_function_count();
	static const s_task_function_description &get_task_function_description(uint32 index);
	static const s_task_function_description &get_task_function_description(e_task_function task_function);
};

#endif // WAVELANG_TASK_FUNCTIONS_H__