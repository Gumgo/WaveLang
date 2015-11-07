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

static const s_task_function_uid k_task_function_real_array_dereference_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 50);

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

static uint32 get_index_or_invalid(uint32 array_count, real32 real_index) {
	// $TODO optimize
	int32 is_invalid = std::isnan(real_index) | std::isinf(real_index);

	// If is_invalid is true, this might have an undefined value, but that's fine
	int32 index = static_cast<int32>(std::floor(real_index));

	is_invalid |= (index < 0);
	is_invalid |= (static_cast<uint32>(index) >= array_count);

	// OR with validity mask -0 == 0, -1 == 0xffffffff
	index |= -is_invalid;
	return static_cast<uint32>(index);
}

static void task_function_real_array_dereference(const s_task_function_context &context) {
	c_buffer_out out = context.arguments[0].get_real_buffer_out();
	c_real_array real_array = context.arguments[1].get_real_array_in();
	c_buffer_in index = context.arguments[2].get_real_buffer_in();

	validate_buffer(out);
	validate_buffer(index);

	// $TODO optimize this

	uint32 array_count = cast_integer_verify<uint32>(real_array.get_count());
	real32 *out_ptr = out->get_data<real32>();
	const real32 *index_ptr = index->get_data<real32>();

	if (index->is_constant()) {
		uint32 array_index = get_index_or_invalid(array_count, *index_ptr);

		if (array_index == 0xffffffff) {
			*out_ptr = 0.0f;
			out->set_constant(true);
		} else {
			const s_real_array_element &element = real_array[array_index];
			if (element.is_constant) {
				*out_ptr = element.constant_value;
				out->set_constant(true);
			} else {
				c_buffer_in array_buffer = context.fast_real_buffer_accessor + element.buffer_index_value;

				if (array_buffer->is_constant()) {
					*out_ptr = *array_buffer->get_data<real32>();
					out->set_constant(true);
				} else {
					memcpy(out_ptr, array_buffer->get_data<real32>(), context.buffer_size * sizeof(real32));
					out->set_constant(false);
				}
			}
		}
	} else {
		for (size_t index = 0; index < context.buffer_size; index++) {
			uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
			real32 dereferenced_value;

			if (array_index == 0xffffffff) {
				dereferenced_value = 0.0f;
			} else {
				const s_real_array_element &element = real_array[array_index];
				if (element.is_constant) {
					dereferenced_value = element.constant_value;
				} else {
					c_buffer_in array_buffer = context.fast_real_buffer_accessor + element.buffer_index_value;
					uint32 dereference_index = array_index;

					// Zero if constant, 0xffffffff otherwise
					int32 array_buffer_non_constant = !array_buffer->is_constant();
					int32 dereference_index_mask = -array_buffer_non_constant;
					dereference_index &= dereference_index_mask;

					dereferenced_value = array_buffer->get_data<real32>()[dereference_index];
				}
			}

			out_ptr[index] = dereferenced_value;
		}

		out->set_constant(false);
	}
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

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_array_dereference_uid,
				nullptr, nullptr, task_function_real_array_dereference,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_array_in), TDT(real_buffer_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_real_array_dereference_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_real_array_dereference_uid, c_task_function_mapping_list::construct(mappings));
	}
}