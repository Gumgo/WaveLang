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

struct s_task_function_context {
	uint32 buffer_size;
	void *task_memory;
	// $TODO more things like timing

	c_wrapped_array_const<real32> in_constants;
	c_wrapped_array_const<const c_buffer *> in_buffers;
	c_wrapped_array_const<c_buffer *> out_buffers;
	c_wrapped_array_const<c_buffer *> inout_buffers;
};

typedef void (*f_task_function)(const s_task_function_context &context);

// This function takes a list of constant inputs to the task and returns the amount of memory the task requires
typedef c_wrapped_array_const<real32> c_task_constant_inputs;
typedef size_t (*f_task_memory_query)(c_task_constant_inputs constant_inputs);

struct s_task_function_description {
	// Function to execute
	f_task_function function;

	// Memory query function, or null
	f_task_memory_query memory_query;

	// Number of constant inputs
	uint32 in_constant_count;

	// Number of buffers used as inputs to the function
	uint32 in_buffer_count;

	// Number of buffers used as outputs from the function
	uint32 out_buffer_count;

	// Number of buffers used as inouts for the function; these are buffers which start as an input and are overwritten
	// to be used as an output.
	uint32 inout_buffer_count;

	// $TODO more data about how this task is allowed to be optimized (e.g. accumulator)
};

class c_task_function_registry {
public:
	static uint32 get_task_function_count();
	static const s_task_function_description &get_task_function_description(uint32 index);
	static const s_task_function_description &get_task_function_description(e_task_function task_function);
};

#endif // WAVELANG_TASK_FUNCTIONS_H__