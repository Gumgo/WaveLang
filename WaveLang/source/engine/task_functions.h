#ifndef WAVELANG_TASK_FUNCTIONS_H__
#define WAVELANG_TASK_FUNCTIONS_H__

#include "common/common.h"

class c_buffer;
class c_sample_library_accessor;
class c_sample_library_requester;

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

	k_task_function_sampler_buffer,
	k_task_function_sampler_bufferio,
	k_task_function_sampler_constant,

	k_task_function_sampler_loop_buffer,
	k_task_function_sampler_loop_bufferio,
	k_task_function_sampler_loop_constant,

	k_task_function_sampler_loop_phase_shift_buffer_buffer,
	k_task_function_sampler_loop_phase_shift_bufferio_buffer,
	k_task_function_sampler_loop_phase_shift_buffer_bufferio,
	k_task_function_sampler_loop_phase_shift_buffer_constant,
	k_task_function_sampler_loop_phase_shift_bufferio_constant,
	k_task_function_sampler_loop_phase_shift_constant_buffer,
	k_task_function_sampler_loop_phase_shift_constant_bufferio,
	k_task_function_sampler_loop_phase_shift_constant_constant,

	k_task_function_count
};

enum e_task_data_type {
	k_task_data_type_real_buffer_in,
	k_task_data_type_real_buffer_out,
	k_task_data_type_real_buffer_inout,
	k_task_data_type_real_constant_in,
	k_task_data_type_bool_constant_in,
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
		bool bool_constant_in;
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

	bool get_bool_constant_in() const {
		wl_assert(type == k_task_data_type_bool_constant_in);
		return data.bool_constant_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(type == k_task_data_type_string_constant_in);
		return data.string_constant_in;
	}
};

typedef c_wrapped_array_const<s_task_function_argument> c_task_function_arguments;

struct s_task_function_context {
	c_sample_library_accessor *sample_accessor;
	c_sample_library_requester *sample_requester;

	uint32 sample_rate;
	uint32 buffer_size;
	void *task_memory;

	// $TODO more things like timing

	c_task_function_arguments arguments;
};

// This function takes a partially-filled-in context and returns the amount of memory the task requires
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
typedef size_t (*f_task_memory_query)(const s_task_function_context &context);

// Called after task memory has been allocated - should be used to initialize this memory
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
// In addition, this is the time where samples should be requested, as that interface is provided on the context
typedef void (*f_task_initializer)(const s_task_function_context &context);

// Function executed for the task
typedef void (*f_task_function)(const s_task_function_context &context);

static const size_t k_max_task_function_arguments = 10;

struct s_task_function_description {
	// Memory query function, or null
	f_task_memory_query memory_query;

	// Function for initializing task memory
	f_task_initializer initializer;

	// Function to execute
	f_task_function function;

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