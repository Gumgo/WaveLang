#ifndef WAVELANG_TASK_FUNCTIONS_H__
#define WAVELANG_TASK_FUNCTIONS_H__

#include "common/common.h"

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

	// $TODO temporary
	k_task_function_test,

	k_task_function_count
};

struct s_task_function_description {
	void *function; // $TODO make this a fn pointer

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