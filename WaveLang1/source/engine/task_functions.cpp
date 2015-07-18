#include "engine/task_functions.h"

static const s_task_function_description k_task_functions[] = {
	{ nullptr, 0, 1, 1, 0 }, // k_task_function_negation_buffer
	{ nullptr, 0, 0, 0, 1 }, // k_task_function_negation_bufferio

	{ nullptr, 0, 2, 1, 0 }, // k_task_function_addition_buffer_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_addition_bufferio_buffer
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_addition_buffer_constant
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_addition_bufferio_constant

	{ nullptr, 0, 2, 1, 0 }, // k_task_function_subtraction_buffer_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_subtraction_bufferio_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_subtraction_buffer_bufferio
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_subtraction_buffer_constant
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_subtraction_bufferio_constant
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_subtraction_constant_buffer
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_subtraction_constant_bufferio

	{ nullptr, 0, 2, 1, 0 }, // k_task_function_multiplication_buffer_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_multiplication_bufferio_buffer
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_multiplication_buffer_constant
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_multiplication_bufferio_constant

	{ nullptr, 0, 2, 1, 0 }, // k_task_function_division_buffer_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_division_bufferio_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_division_buffer_bufferio
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_division_buffer_constant
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_division_bufferio_constant
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_division_constant_buffer
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_division_constant_bufferio

	{ nullptr, 0, 2, 1, 0 }, // k_task_function_modulo_buffer_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_modulo_bufferio_buffer
	{ nullptr, 0, 1, 0, 1 }, // k_task_function_modulo_buffer_bufferio
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_modulo_buffer_constant
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_modulo_bufferio_constant
	{ nullptr, 1, 1, 1, 0 }, // k_task_function_modulo_constant_buffer
	{ nullptr, 1, 0, 0, 1 }, // k_task_function_modulo_constant_bufferio

	{ nullptr, 0, 0, 1, 0 }  // k_task_function_test
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