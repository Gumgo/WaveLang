#include "engine/task_functions/task_functions_basic.h"
#include "execution_graph/native_modules/native_modules_basic.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/buffer_operations/buffer_operations_internal.h"

static const uint32 k_task_functions_basic_library_id = 0;

static const s_task_function_uid k_task_function_negation_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 0);
static const s_task_function_uid k_task_function_negation_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 1);

static const s_task_function_uid k_task_function_addition_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 2);
static const s_task_function_uid k_task_function_addition_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 3);
static const s_task_function_uid k_task_function_addition_buffer_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 4);
static const s_task_function_uid k_task_function_addition_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 5);

static const s_task_function_uid k_task_function_subtraction_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 10);
static const s_task_function_uid k_task_function_subtraction_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 11);
static const s_task_function_uid k_task_function_subtraction_buffer_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 12);
static const s_task_function_uid k_task_function_subtraction_buffer_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 13);
static const s_task_function_uid k_task_function_subtraction_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 14);
static const s_task_function_uid k_task_function_subtraction_constant_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 15);
static const s_task_function_uid k_task_function_subtraction_constant_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 16);

static const s_task_function_uid k_task_function_multiplication_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 20);
static const s_task_function_uid k_task_function_multiplication_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 21);
static const s_task_function_uid k_task_function_multiplication_buffer_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 22);
static const s_task_function_uid k_task_function_multiplication_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 23);

static const s_task_function_uid k_task_function_division_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 30);
static const s_task_function_uid k_task_function_division_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 31);
static const s_task_function_uid k_task_function_division_buffer_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 32);
static const s_task_function_uid k_task_function_division_buffer_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 33);
static const s_task_function_uid k_task_function_division_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 34);
static const s_task_function_uid k_task_function_division_constant_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 35);
static const s_task_function_uid k_task_function_division_constant_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 36);

static const s_task_function_uid k_task_function_modulo_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 40);
static const s_task_function_uid k_task_function_modulo_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 41);
static const s_task_function_uid k_task_function_modulo_buffer_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 42);
static const s_task_function_uid k_task_function_modulo_buffer_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 43);
static const s_task_function_uid k_task_function_modulo_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 44);
static const s_task_function_uid k_task_function_modulo_constant_buffer_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 45);
static const s_task_function_uid k_task_function_modulo_constant_bufferio_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 46);

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

void register_task_functions_basic() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_buffer_uid,
				nullptr, nullptr, task_function_negation_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_bufferio_uid,
				nullptr, nullptr, task_function_negation_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_negation_bufferio_uid, "b.",
				s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_negation_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_negation_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_buffer_buffer_uid,
				nullptr, nullptr, task_function_addition_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_bufferio_buffer_uid,
				nullptr, nullptr, task_function_addition_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_buffer_constant_uid,
				nullptr, nullptr, task_function_addition_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_bufferio_constant_uid,
				nullptr, nullptr, task_function_addition_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_addition_bufferio_buffer_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_addition_bufferio_buffer_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_addition_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_addition_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_addition_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_addition_bufferio_constant_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_addition_buffer_constant_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(2, 1, 0))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_addition_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_buffer_buffer_uid,
				nullptr, nullptr, task_function_subtraction_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_bufferio_buffer_uid,
				nullptr, nullptr, task_function_subtraction_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_buffer_bufferio_uid,
				nullptr, nullptr, task_function_subtraction_buffer_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_in), TDT(real_buffer_inout))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_buffer_constant_uid,
				nullptr, nullptr, task_function_subtraction_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_bufferio_constant_uid,
				nullptr, nullptr, task_function_subtraction_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_constant_buffer_uid,
				nullptr, nullptr, task_function_subtraction_constant_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_constant_bufferio_uid,
				nullptr, nullptr, task_function_subtraction_constant_bufferio,
				s_task_function_argument_list::build(TDT(real_constant_in), TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_subtraction_bufferio_buffer_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_buffer_bufferio_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_subtraction_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_constant_bufferio_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_subtraction_constant_buffer_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_subtraction_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_buffer_buffer_uid,
				nullptr, nullptr, task_function_multiplication_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_bufferio_buffer_uid,
				nullptr, nullptr, task_function_multiplication_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_buffer_constant_uid,
				nullptr, nullptr, task_function_multiplication_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_bufferio_constant_uid,
				nullptr, nullptr, task_function_multiplication_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_multiplication_bufferio_buffer_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_bufferio_buffer_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_bufferio_constant_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_buffer_constant_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(2, 1, 0))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_multiplication_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_buffer_buffer_uid,
				nullptr, nullptr, task_function_division_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_bufferio_buffer_uid,
				nullptr, nullptr, task_function_division_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_buffer_bufferio_uid,
				nullptr, nullptr, task_function_division_buffer_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_in), TDT(real_buffer_inout))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_buffer_constant_uid,
				nullptr, nullptr, task_function_division_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_bufferio_constant_uid,
				nullptr, nullptr, task_function_division_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_constant_buffer_uid,
				nullptr, nullptr, task_function_division_constant_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_constant_bufferio_uid,
				nullptr, nullptr, task_function_division_constant_bufferio,
				s_task_function_argument_list::build(TDT(real_constant_in), TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_division_bufferio_buffer_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_division_buffer_bufferio_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_division_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_division_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_division_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_division_constant_bufferio_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_division_constant_buffer_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_division_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_buffer_buffer_uid,
				nullptr, nullptr, task_function_modulo_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_bufferio_buffer_uid,
				nullptr, nullptr, task_function_modulo_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_buffer_bufferio_uid,
				nullptr, nullptr, task_function_modulo_buffer_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_in), TDT(real_buffer_inout))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_buffer_constant_uid,
				nullptr, nullptr, task_function_modulo_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_bufferio_constant_uid,
				nullptr, nullptr, task_function_modulo_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_constant_buffer_uid,
				nullptr, nullptr, task_function_modulo_constant_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_constant_bufferio_uid,
				nullptr, nullptr, task_function_modulo_constant_bufferio,
				s_task_function_argument_list::build(TDT(real_constant_in), TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_modulo_bufferio_buffer_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_modulo_buffer_bufferio_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_modulo_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_modulo_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_modulo_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_modulo_constant_bufferio_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_modulo_constant_buffer_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_modulo_uid, c_task_function_mapping_list::construct(mappings));
	}
}