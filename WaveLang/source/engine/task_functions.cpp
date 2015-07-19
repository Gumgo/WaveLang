#include "engine/task_functions.h"
#include "engine/buffer_operations/buffer_operations_arithmetic.h"

// $TODO temp
#include <cmath>

static void task_function_negation_buffer(const s_task_function_context &context);
static void task_function_negation_bufferio(const s_task_function_context &context);

static void task_function_addition_buffer_buffer(const s_task_function_context &context);
static void task_function_addition_bufferio_buffer(const s_task_function_context &context);
static void task_function_addition_buffer_constant(const s_task_function_context &context);
static void task_function_addition_bufferio_constant(const s_task_function_context &context);

static void task_function_subtraction_buffer_buffer(const s_task_function_context &context);
static void task_function_subtraction_bufferio_buffer(const s_task_function_context &context);
static void task_function_subtraction_buffer_bufferio(const s_task_function_context &context);
static void task_function_subtraction_buffer_constant(const s_task_function_context &context);
static void task_function_subtraction_bufferio_constant(const s_task_function_context &context);
static void task_function_subtraction_constant_buffer(const s_task_function_context &context);
static void task_function_subtraction_constant_bufferio(const s_task_function_context &context);

static void task_function_multiplication_buffer_buffer(const s_task_function_context &context);
static void task_function_multiplication_bufferio_buffer(const s_task_function_context &context);
static void task_function_multiplication_buffer_constant(const s_task_function_context &context);
static void task_function_multiplication_bufferio_constant(const s_task_function_context &context);

static void task_function_division_buffer_buffer(const s_task_function_context &context);
static void task_function_division_bufferio_buffer(const s_task_function_context &context);
static void task_function_division_buffer_bufferio(const s_task_function_context &context);
static void task_function_division_buffer_constant(const s_task_function_context &context);
static void task_function_division_bufferio_constant(const s_task_function_context &context);
static void task_function_division_constant_buffer(const s_task_function_context &context);
static void task_function_division_constant_bufferio(const s_task_function_context &context);

static void task_function_modulo_buffer_buffer(const s_task_function_context &context);
static void task_function_modulo_bufferio_buffer(const s_task_function_context &context);
static void task_function_modulo_buffer_bufferio(const s_task_function_context &context);
static void task_function_modulo_buffer_constant(const s_task_function_context &context);
static void task_function_modulo_bufferio_constant(const s_task_function_context &context);
static void task_function_modulo_constant_buffer(const s_task_function_context &context);
static void task_function_modulo_constant_bufferio(const s_task_function_context &context);

static void task_function_test(const s_task_function_context &context);
static void task_function_test_c(const s_task_function_context &context);
static size_t task_memory_query_test(c_task_constant_inputs constant_inputs);
static size_t task_memory_query_test_c(c_task_constant_inputs constant_inputs);

static void task_function_test_delay(const s_task_function_context &context);
static size_t task_memory_query_test_delay(c_task_constant_inputs constant_inputs);

static const s_task_function_description k_task_functions[] = {
	{ task_function_negation_buffer, nullptr, 0, 1, 1, 0 },
	{ task_function_negation_bufferio, nullptr, 0, 0, 0, 1 },

	{ task_function_addition_buffer_buffer, nullptr, 0, 2, 1, 0 },
	{ task_function_addition_bufferio_buffer, nullptr, 0, 1, 0, 1 },
	{ task_function_addition_buffer_constant, nullptr, 1, 1, 1, 0 },
	{ task_function_addition_bufferio_constant, nullptr, 1, 0, 0, 1 },

	{ task_function_subtraction_buffer_buffer, nullptr, 0, 2, 1, 0 },
	{ task_function_subtraction_bufferio_buffer, nullptr, 0, 1, 0, 1 },
	{ task_function_subtraction_buffer_bufferio, nullptr, 0, 1, 0, 1 },
	{ task_function_subtraction_buffer_constant, nullptr, 1, 1, 1, 0 },
	{ task_function_subtraction_bufferio_constant, nullptr, 1, 0, 0, 1 },
	{ task_function_subtraction_constant_buffer, nullptr, 1, 1, 1, 0 },
	{ task_function_subtraction_constant_bufferio, nullptr, 1, 0, 0, 1 },

	{ task_function_multiplication_buffer_buffer, nullptr, 0, 2, 1, 0 },
	{ task_function_multiplication_bufferio_buffer, nullptr, 0, 1, 0, 1 },
	{ task_function_multiplication_buffer_constant, nullptr, 1, 1, 1, 0 },
	{ task_function_multiplication_bufferio_constant, nullptr, 1, 0, 0, 1 },

	{ task_function_division_buffer_buffer, nullptr, 0, 2, 1, 0 },
	{ task_function_division_bufferio_buffer, nullptr, 0, 1, 0, 1 },
	{ task_function_division_buffer_bufferio, nullptr, 0, 1, 0, 1 },
	{ task_function_division_buffer_constant, nullptr, 1, 1, 1, 0 },
	{ task_function_division_bufferio_constant, nullptr, 1, 0, 0, 1 },
	{ task_function_division_constant_buffer, nullptr, 1, 1, 1, 0 },
	{ task_function_division_constant_bufferio, nullptr, 1, 0, 0, 1 },

	{ task_function_modulo_buffer_buffer, nullptr, 0, 2, 1, 0 },
	{ task_function_modulo_bufferio_buffer, nullptr, 0, 1, 0, 1 },
	{ task_function_modulo_buffer_bufferio, nullptr, 0, 1, 0, 1 },
	{ task_function_modulo_buffer_constant, nullptr, 1, 1, 1, 0 },
	{ task_function_modulo_bufferio_constant, nullptr, 1, 0, 0, 1 },
	{ task_function_modulo_constant_buffer, nullptr, 1, 1, 1, 0 },
	{ task_function_modulo_constant_bufferio, nullptr, 1, 0, 0, 1 },

	{ task_function_test, task_memory_query_test, 0, 1, 1, 0 }, 
	{ task_function_test_c, task_memory_query_test_c, 1, 0, 1, 0 }, 
	{ task_function_test_delay, task_memory_query_test_delay, 1, 1, 1, 0 } 
};

static_assert(NUMBEROF(k_task_functions) == k_task_function_count, "Update task functions");

uint32 c_task_function_registry::get_task_function_count() {
	return NUMBEROF(k_task_functions);
}

const s_task_function_description &c_task_function_registry::get_task_function_description(uint32 index) {
	wl_assert(VALID_INDEX(index, NUMBEROF(k_task_functions)));
	return k_task_functions[index];
}

const s_task_function_description &c_task_function_registry::get_task_function_description(
	e_task_function task_function) {
	wl_assert(VALID_INDEX(task_function, NUMBEROF(k_task_functions)));
	return k_task_functions[task_function];
}

// Don't bother with linebreaks here... it would just make it less readable

static void task_function_negation_buffer(const s_task_function_context &context) {
	buffer_operation_negation_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0]);
}

static void task_function_negation_bufferio(const s_task_function_context &context) {
	buffer_operation_negation_bufferio(context.buffer_size, context.inout_buffers[0]);
}

static void task_function_addition_buffer_buffer(const s_task_function_context &context) {
	buffer_operation_addition_buffer_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_buffers[1]);
}

static void task_function_addition_bufferio_buffer(const s_task_function_context &context) {
	buffer_operation_addition_bufferio_buffer(context.buffer_size, context.inout_buffers[0], context.in_buffers[0]);
}

static void task_function_addition_buffer_constant(const s_task_function_context &context) {
	buffer_operation_addition_buffer_constant(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_constants[0]);
}

static void task_function_addition_bufferio_constant(const s_task_function_context &context) {
	buffer_operation_addition_bufferio_constant(context.buffer_size, context.inout_buffers[0], context.in_constants[0]);
}

static void task_function_subtraction_buffer_buffer(const s_task_function_context &context) {
	buffer_operation_subtraction_buffer_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_buffers[1]);
}

static void task_function_subtraction_bufferio_buffer(const s_task_function_context &context) {
	buffer_operation_subtraction_bufferio_buffer(context.buffer_size, context.inout_buffers[0], context.in_buffers[0]);
}

static void task_function_subtraction_buffer_bufferio(const s_task_function_context &context) {
	buffer_operation_subtraction_buffer_bufferio(context.buffer_size, context.in_buffers[0], context.inout_buffers[0]);
}

static void task_function_subtraction_buffer_constant(const s_task_function_context &context) {
	buffer_operation_subtraction_buffer_constant(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_constants[0]);
}

static void task_function_subtraction_bufferio_constant(const s_task_function_context &context) {
	buffer_operation_subtraction_bufferio_constant(context.buffer_size, context.inout_buffers[0], context.in_constants[0]);
}

static void task_function_subtraction_constant_buffer(const s_task_function_context &context) {
	buffer_operation_subtraction_constant_buffer(context.buffer_size, context.out_buffers[0], context.in_constants[0], context.in_buffers[1]);
}

static void task_function_subtraction_constant_bufferio(const s_task_function_context &context) {
	buffer_operation_subtraction_constant_bufferio(context.buffer_size, context.in_constants[0], context.inout_buffers[0]);
}

static void task_function_multiplication_buffer_buffer(const s_task_function_context &context) {
	buffer_operation_multiplication_buffer_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_buffers[1]);
}

static void task_function_multiplication_bufferio_buffer(const s_task_function_context &context) {
	buffer_operation_multiplication_bufferio_buffer(context.buffer_size, context.inout_buffers[0], context.in_buffers[0]);
}

static void task_function_multiplication_buffer_constant(const s_task_function_context &context) {
	buffer_operation_multiplication_buffer_constant(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_constants[0]);
}

static void task_function_multiplication_bufferio_constant(const s_task_function_context &context) {
	buffer_operation_multiplication_bufferio_constant(context.buffer_size, context.inout_buffers[0], context.in_constants[0]);
}

static void task_function_division_buffer_buffer(const s_task_function_context &context) {
	buffer_operation_division_buffer_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_buffers[1]);
}

static void task_function_division_bufferio_buffer(const s_task_function_context &context) {
	buffer_operation_division_bufferio_buffer(context.buffer_size, context.inout_buffers[0], context.in_buffers[0]);
}

static void task_function_division_buffer_bufferio(const s_task_function_context &context) {
	buffer_operation_division_buffer_bufferio(context.buffer_size, context.in_buffers[0], context.inout_buffers[0]);
}

static void task_function_division_buffer_constant(const s_task_function_context &context) {
	buffer_operation_division_buffer_constant(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_constants[0]);
}

static void task_function_division_bufferio_constant(const s_task_function_context &context) {
	buffer_operation_division_bufferio_constant(context.buffer_size, context.inout_buffers[0], context.in_constants[0]);
}

static void task_function_division_constant_buffer(const s_task_function_context &context) {
	buffer_operation_division_constant_buffer(context.buffer_size, context.out_buffers[0], context.in_constants[0], context.in_buffers[1]);
}

static void task_function_division_constant_bufferio(const s_task_function_context &context) {
	buffer_operation_division_constant_bufferio(context.buffer_size, context.in_constants[0], context.inout_buffers[0]);
}

static void task_function_modulo_buffer_buffer(const s_task_function_context &context) {
	buffer_operation_modulo_buffer_buffer(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_buffers[1]);
}

static void task_function_modulo_bufferio_buffer(const s_task_function_context &context) {
	buffer_operation_modulo_bufferio_buffer(context.buffer_size, context.inout_buffers[0], context.in_buffers[0]);
}

static void task_function_modulo_buffer_bufferio(const s_task_function_context &context) {
	buffer_operation_modulo_buffer_bufferio(context.buffer_size, context.in_buffers[0], context.inout_buffers[0]);
}

static void task_function_modulo_buffer_constant(const s_task_function_context &context) {
	buffer_operation_modulo_buffer_constant(context.buffer_size, context.out_buffers[0], context.in_buffers[0], context.in_constants[0]);
}

static void task_function_modulo_bufferio_constant(const s_task_function_context &context) {
	buffer_operation_modulo_bufferio_constant(context.buffer_size, context.inout_buffers[0], context.in_constants[0]);
}

static void task_function_modulo_constant_buffer(const s_task_function_context &context) {
	buffer_operation_modulo_constant_buffer(context.buffer_size, context.out_buffers[0], context.in_constants[0], context.in_buffers[1]);
}

static void task_function_modulo_constant_bufferio(const s_task_function_context &context) {
	buffer_operation_modulo_constant_bufferio(context.buffer_size, context.in_constants[0], context.inout_buffers[0]);
}

struct s_task_function_test_context {
	bool initialized;
	real32 time;
};

static void task_function_test(const s_task_function_context &context) {
	wl_assert(context.task_memory);
	s_task_function_test_context *test_context = reinterpret_cast<s_task_function_test_context *>(context.task_memory);

	if (!test_context->initialized) {
		test_context->initialized = true;
		test_context->time = 0;
	}

	c_buffer *out = context.out_buffers[0];
	const c_buffer *freq = context.in_buffers[0];
	real32 *data = out->get_data<real32>();
	const real32 *freq_data = freq->get_data<real32>();

	static const real32 k_sample_rate = 44100.0f;
	static const real32 k_pi = 3.14159265359f;

	for (uint32 index = 0; index < context.buffer_size; index++) {
		real32 hz = freq_data[index];
		real32 cycles_per_sample = hz / k_sample_rate;

		data[index] = std::sin(test_context->time * (2.0f * k_pi));
		test_context->time += cycles_per_sample;
		test_context->time = test_context->time >= 1.0f ? test_context->time - 1.0f : test_context->time;
	}
}

static void task_function_test_c(const s_task_function_context &context) {
	wl_assert(context.task_memory);
	s_task_function_test_context *test_context = reinterpret_cast<s_task_function_test_context *>(context.task_memory);

	if (!test_context->initialized) {
		test_context->initialized = true;
		test_context->time = 0;
	}

	c_buffer *out = context.out_buffers[0];
	real32 *data = out->get_data<real32>();
	real32 freq_data = context.in_constants[0];

	static const real32 k_sample_rate = 44100.0f;
	static const real32 k_pi = 3.14159265359f;

	for (uint32 index = 0; index < context.buffer_size; index++) {
		real32 hz = freq_data;
		real32 cycles_per_sample = hz / k_sample_rate;

		data[index] = std::sin(test_context->time * (2.0f * k_pi));
		test_context->time += cycles_per_sample;
		test_context->time = test_context->time >= 1.0f ? test_context->time - 1.0f : test_context->time;
	}
}

static size_t task_memory_query_test(c_task_constant_inputs constant_inputs) {
	wl_assert(constant_inputs.get_count() == 0);
	return sizeof(s_task_function_test_context);
}

static size_t task_memory_query_test_c(c_task_constant_inputs constant_inputs) {
	wl_assert(constant_inputs.get_count() == 1);
	return sizeof(s_task_function_test_context);
}

static void task_function_test_delay(const s_task_function_context &context) {
}

static size_t task_memory_query_test_delay(c_task_constant_inputs constant_inputs) {
	// The constant input gives us the delay time
	wl_assert(constant_inputs.get_count() == 1);
	return 0;
}