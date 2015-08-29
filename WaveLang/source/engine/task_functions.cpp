#include "engine/task_functions.h"
#include "engine/buffer_operations/buffer_operations_arithmetic.h"
#include "engine/buffer_operations/buffer_operations_utility.h"
#include "engine/buffer_operations/buffer_operations_sampler.h"
#include "engine/sample/sample_library.h"


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

static void task_function_abs_buffer(const s_task_function_context &context);
static void task_function_abs_bufferio(const s_task_function_context &context);

static void task_function_floor_buffer(const s_task_function_context &context);
static void task_function_floor_bufferio(const s_task_function_context &context);

static void task_function_ceil_buffer(const s_task_function_context &context);
static void task_function_ceil_bufferio(const s_task_function_context &context);

static void task_function_round_buffer(const s_task_function_context &context);
static void task_function_round_bufferio(const s_task_function_context &context);

static void task_function_min_buffer_buffer(const s_task_function_context &context);
static void task_function_min_bufferio_buffer(const s_task_function_context &context);
static void task_function_min_buffer_constant(const s_task_function_context &context);
static void task_function_min_bufferio_constant(const s_task_function_context &context);

static void task_function_max_buffer_buffer(const s_task_function_context &context);
static void task_function_max_bufferio_buffer(const s_task_function_context &context);
static void task_function_max_buffer_constant(const s_task_function_context &context);
static void task_function_max_bufferio_constant(const s_task_function_context &context);

static void task_function_exp_buffer(const s_task_function_context &context);
static void task_function_exp_bufferio(const s_task_function_context &context);

static void task_function_log_buffer(const s_task_function_context &context);
static void task_function_log_bufferio(const s_task_function_context &context);

static void task_function_sqrt_buffer(const s_task_function_context &context);
static void task_function_sqrt_bufferio(const s_task_function_context &context);

static void task_function_pow_buffer_buffer(const s_task_function_context &context);
static void task_function_pow_bufferio_buffer(const s_task_function_context &context);
static void task_function_pow_buffer_bufferio(const s_task_function_context &context);
static void task_function_pow_buffer_constant(const s_task_function_context &context);
static void task_function_pow_bufferio_constant(const s_task_function_context &context);
static void task_function_pow_constant_buffer(const s_task_function_context &context);
static void task_function_pow_constant_bufferio(const s_task_function_context &context);

static void task_function_sampler_buffer(const s_task_function_context &context);
static void task_function_sampler_bufferio(const s_task_function_context &context);
static void task_function_sampler_constant(const s_task_function_context &context);
static size_t task_memory_query_sampler(const s_task_function_context &context);
static void task_initializer_sampler(const s_task_function_context &context);

static void task_function_test(const s_task_function_context &context);
static void task_function_test_c(const s_task_function_context &context);
static size_t task_memory_query_test(const s_task_function_context &context);
static size_t task_memory_query_test_c(const s_task_function_context &context);

static void task_function_test_delay(const s_task_function_context &context);
static size_t task_memory_query_test_delay(const s_task_function_context &context);

struct s_task_function_argument_list {
	e_task_data_type arguments[k_max_task_function_arguments];
};

s_task_function_argument_list make_args(
	e_task_data_type arg_0 = k_task_data_type_count,
	e_task_data_type arg_1 = k_task_data_type_count,
	e_task_data_type arg_2 = k_task_data_type_count,
	e_task_data_type arg_3 = k_task_data_type_count,
	e_task_data_type arg_4 = k_task_data_type_count,
	e_task_data_type arg_5 = k_task_data_type_count,
	e_task_data_type arg_6 = k_task_data_type_count,
	e_task_data_type arg_7 = k_task_data_type_count,
	e_task_data_type arg_8 = k_task_data_type_count,
	e_task_data_type arg_9 = k_task_data_type_count);

static s_task_function_description make_task(
	f_task_memory_query task_memory_query,
	f_task_initializer task_initializer,
	f_task_function task_function,
	const s_task_function_argument_list &arguments);

#define TDT(type) k_task_data_type_ ## type

// Argument order convention: compile_time_constant, out, in_a, in_b, ...;
// compile_time_constant are for things like string names of samples, which are required to be constant, unlike e.g.
// reals, which can either resolve to buffers or constants depending on usage
// inout buffer position will be based on the "in" location
// This attempts to follow the pattern "dst = f(src_a, src_b, ...)"
static const s_task_function_description k_task_functions[] = {
	make_task(nullptr, nullptr, task_function_negation_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_negation_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_addition_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_addition_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_addition_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_addition_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),

	make_task(nullptr, nullptr, task_function_subtraction_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_subtraction_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_subtraction_buffer_bufferio, make_args(TDT(real_buffer_in), TDT(real_buffer_inout))),
	make_task(nullptr, nullptr, task_function_subtraction_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_subtraction_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_subtraction_constant_buffer, make_args(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_subtraction_constant_bufferio, make_args(TDT(real_constant_in), TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_multiplication_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_multiplication_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_multiplication_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_multiplication_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),

	make_task(nullptr, nullptr, task_function_division_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_division_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_division_buffer_bufferio, make_args(TDT(real_buffer_in), TDT(real_buffer_inout))),
	make_task(nullptr, nullptr, task_function_division_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_division_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_division_constant_buffer, make_args(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_division_constant_bufferio, make_args(TDT(real_constant_in), TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_modulo_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_modulo_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_modulo_buffer_bufferio, make_args(TDT(real_buffer_in), TDT(real_buffer_inout))),
	make_task(nullptr, nullptr, task_function_modulo_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_modulo_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_modulo_constant_buffer, make_args(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_modulo_constant_bufferio, make_args(TDT(real_constant_in), TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_abs_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_abs_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_floor_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_floor_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_ceil_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_ceil_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_round_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_round_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_min_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_min_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_min_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_min_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),

	make_task(nullptr, nullptr, task_function_max_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_max_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_max_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_max_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),

	make_task(nullptr, nullptr, task_function_exp_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_exp_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_log_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_log_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_sqrt_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_sqrt_bufferio, make_args(TDT(real_buffer_inout))),

	make_task(nullptr, nullptr, task_function_pow_buffer_buffer, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_pow_bufferio_buffer, make_args(TDT(real_buffer_inout), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_pow_buffer_bufferio, make_args(TDT(real_buffer_in), TDT(real_buffer_inout))),
	make_task(nullptr, nullptr, task_function_pow_buffer_constant, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_pow_bufferio_constant, make_args(TDT(real_buffer_inout), TDT(real_constant_in))),
	make_task(nullptr, nullptr, task_function_pow_constant_buffer, make_args(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))),
	make_task(nullptr, nullptr, task_function_pow_constant_bufferio, make_args(TDT(real_constant_in), TDT(real_buffer_inout))),

	make_task(task_memory_query_sampler, task_initializer_sampler, task_function_sampler_buffer,
		make_args(TDT(string_constant_in), TDT(real_constant_in), TDT(bool_constant_in), TDT(bool_constant_in), TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(task_memory_query_sampler, task_initializer_sampler, task_function_sampler_bufferio,
		make_args(TDT(string_constant_in), TDT(real_constant_in), TDT(bool_constant_in), TDT(bool_constant_in), TDT(real_buffer_inout))),
	make_task(task_memory_query_sampler, task_initializer_sampler, task_function_sampler_constant,
		make_args(TDT(string_constant_in), TDT(real_constant_in), TDT(bool_constant_in), TDT(bool_constant_in), TDT(real_buffer_out), TDT(real_constant_in))),

	make_task(task_memory_query_test, nullptr, task_function_test, make_args(TDT(real_buffer_out), TDT(real_buffer_in))),
	make_task(task_memory_query_test_c, nullptr, task_function_test_c, make_args(TDT(real_buffer_out), TDT(real_constant_in))),
	make_task(task_memory_query_test_delay, nullptr, task_function_test_delay, make_args(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in)))
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

s_task_function_argument_list make_args(
	e_task_data_type arg_0,
	e_task_data_type arg_1,
	e_task_data_type arg_2,
	e_task_data_type arg_3,
	e_task_data_type arg_4,
	e_task_data_type arg_5,
	e_task_data_type arg_6,
	e_task_data_type arg_7,
	e_task_data_type arg_8,
	e_task_data_type arg_9) {
	s_task_function_argument_list arg_list = {
		arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9
	};
	return arg_list;
}

static s_task_function_description make_task(
	f_task_memory_query task_memory_query,
	f_task_initializer task_initializer,
	f_task_function task_function,
	const s_task_function_argument_list &arguments) {
	wl_assert(task_function);
	s_task_function_description desc;
	ZERO_STRUCT(&desc);
	desc.memory_query = task_memory_query;
	desc.initializer = task_initializer;
	desc.function = task_function;
	for (desc.argument_count = 0; desc.argument_count < k_max_task_function_arguments; desc.argument_count++) {
		e_task_data_type arg = arguments.arguments[desc.argument_count];
		// Loop until we find an k_task_data_type_count
		if (arg != k_task_data_type_count) {
			wl_assert(VALID_INDEX(arg, k_task_data_type_count));
			desc.argument_types[desc.argument_count] = arg;
		} else {
			break;
		}
	}

	return desc;
}

// Don't bother with linebreaks here... it would just make it less readable

static void task_function_negation_buffer(const s_task_function_context &context) {
	s_buffer_operation_negation::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_negation_bufferio(const s_task_function_context &context) {
	s_buffer_operation_negation::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_addition_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_addition::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_addition_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_addition::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_addition_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_addition::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_addition_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_addition::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_subtraction_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_subtraction::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_subtraction_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_subtraction::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_subtraction_buffer_bufferio(const s_task_function_context &context) {
	s_buffer_operation_subtraction::buffer_bufferio(context.buffer_size, context.arguments[0].get_real_buffer_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_subtraction_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_subtraction::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_subtraction_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_subtraction::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_subtraction_constant_buffer(const s_task_function_context &context) {
	s_buffer_operation_subtraction::constant_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_constant_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_subtraction_constant_bufferio(const s_task_function_context &context) {
	s_buffer_operation_subtraction::constant_bufferio(context.buffer_size, context.arguments[0].get_real_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_multiplication_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_multiplication::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_multiplication_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_multiplication::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_multiplication_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_multiplication::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_multiplication_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_multiplication::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_division_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_division::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_division_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_division::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_division_buffer_bufferio(const s_task_function_context &context) {
	s_buffer_operation_division::buffer_bufferio(context.buffer_size, context.arguments[0].get_real_buffer_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_division_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_division::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_division_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_division::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_division_constant_buffer(const s_task_function_context &context) {
	s_buffer_operation_division::constant_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_constant_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_division_constant_bufferio(const s_task_function_context &context) {
	s_buffer_operation_division::constant_bufferio(context.buffer_size, context.arguments[0].get_real_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_modulo_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_modulo::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_modulo_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_modulo::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_modulo_buffer_bufferio(const s_task_function_context &context) {
	s_buffer_operation_modulo::buffer_bufferio(context.buffer_size, context.arguments[0].get_real_buffer_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_modulo_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_modulo::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_modulo_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_modulo::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_modulo_constant_buffer(const s_task_function_context &context) {
	s_buffer_operation_modulo::constant_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_constant_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_modulo_constant_bufferio(const s_task_function_context &context) {
	s_buffer_operation_modulo::constant_bufferio(context.buffer_size, context.arguments[0].get_real_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_abs_buffer(const s_task_function_context &context) {
	s_buffer_operation_abs::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_abs_bufferio(const s_task_function_context &context) {
	s_buffer_operation_abs::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_floor_buffer(const s_task_function_context &context) {
	s_buffer_operation_floor::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_floor_bufferio(const s_task_function_context &context) {
	s_buffer_operation_floor::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_ceil_buffer(const s_task_function_context &context) {
	s_buffer_operation_ceil::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_ceil_bufferio(const s_task_function_context &context) {
	s_buffer_operation_ceil::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_round_buffer(const s_task_function_context &context) {
	s_buffer_operation_round::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_round_bufferio(const s_task_function_context &context) {
	s_buffer_operation_round::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_min_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_min::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_min_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_min::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_min_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_min::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_min_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_min::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_max_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_max::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_max_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_max::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_max_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_max::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_max_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_max::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_exp_buffer(const s_task_function_context &context) {
	s_buffer_operation_exp::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_exp_bufferio(const s_task_function_context &context) {
	s_buffer_operation_exp::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_log_buffer(const s_task_function_context &context) {
	s_buffer_operation_log::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_log_bufferio(const s_task_function_context &context) {
	s_buffer_operation_log::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_sqrt_buffer(const s_task_function_context &context) {
	s_buffer_operation_sqrt::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_sqrt_bufferio(const s_task_function_context &context) {
	s_buffer_operation_sqrt::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_pow_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_pow_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_pow_buffer_bufferio(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_bufferio(context.buffer_size, context.arguments[0].get_real_buffer_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_pow_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_pow_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_pow::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_pow_constant_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::constant_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_constant_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_pow_constant_bufferio(const s_task_function_context &context) {
	s_buffer_operation_pow::constant_bufferio(context.buffer_size, context.arguments[0].get_real_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_sampler_buffer(const s_task_function_context &context) {
	s_buffer_operation_sampler::buffer(
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[4].get_real_buffer_out(),
		context.arguments[5].get_real_buffer_in());
}

static void task_function_sampler_bufferio(const s_task_function_context &context) {
	s_buffer_operation_sampler::bufferio(
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[4].get_real_buffer_inout());
}

static void task_function_sampler_constant(const s_task_function_context &context) {
	s_buffer_operation_sampler::constant(
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[4].get_real_buffer_out(),
		context.arguments[5].get_real_constant_in());
}

static size_t task_memory_query_sampler(const s_task_function_context &context) {
	return sizeof(s_buffer_operation_sampler);
}

static void task_initializer_sampler(const s_task_function_context &context) {
	s_buffer_operation_sampler *task_context = static_cast<s_buffer_operation_sampler *>(context.task_memory);
	const char *name = context.arguments[0].get_string_constant_in();
	real32 channel = context.arguments[1].get_real_constant_in();
	bool loop = context.arguments[2].get_bool_constant_in();
	bool bidi = context.arguments[3].get_bool_constant_in();

	task_context->sample_handle = context.sample_requester->request_sample(name,
		loop ? (bidi ? k_sample_loop_mode_bidi_loop : k_sample_loop_mode_loop) : k_sample_loop_mode_none);
	if (channel >= 0.0f && channel == floor(channel)) {
		task_context->channel = static_cast<uint32>(channel);
	} else {
		task_context->channel = 0xffffffff;
	}
}

// TEMP TEST CODE

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

	c_buffer *out = context.arguments[0].get_real_buffer_out();
	const c_buffer *freq = context.arguments[1].get_real_buffer_in();
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

	c_buffer *out = context.arguments[0].get_real_buffer_out();
	real32 *data = out->get_data<real32>();
	real32 freq_data = context.arguments[1].get_real_constant_in();

	static const real32 k_sample_rate = 44100.0f;
	static const real32 k_pi = 3.14159265359f;

	for (uint32 index = 0; index < context.buffer_size; index++) {
		real32 hz = freq_data;
		real32 cycles_per_sample = hz / k_sample_rate;

		data[index] = std::sin(test_context->time * (2.0f * k_pi));
		test_context->time += cycles_per_sample;
		test_context->time = test_context->time >= 1.0f ? test_context->time - 1.0f : test_context->time;

		// Switch to this code to generate a line which increases slowly from 0. Useful for debugging.
		//data[index] = test_context->time;
		//test_context->time += 1.0f / k_sample_rate;
	}
}

static size_t task_memory_query_test(const s_task_function_context &context) {
	return sizeof(s_task_function_test_context);
}

static size_t task_memory_query_test_c(const s_task_function_context &context) {
	return sizeof(s_task_function_test_context);
}

static void task_function_test_delay(const s_task_function_context &context) {
}

static size_t task_memory_query_test_delay(const s_task_function_context &context) {
	// The constant input gives us the delay time
	return 0;
}