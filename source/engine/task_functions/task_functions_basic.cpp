#include "engine/task_functions/task_functions_basic.h"
#include "execution_graph/native_modules/native_modules_basic.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/buffer_operations/buffer_operations_internal.h"

static const uint32 k_task_functions_basic_library_id = 0;

static const s_task_function_uid k_task_function_negation_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 0);
static const s_task_function_uid k_task_function_negation_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 1);

static const s_task_function_uid k_task_function_addition_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 2);
static const s_task_function_uid k_task_function_addition_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 3);

static const s_task_function_uid k_task_function_subtraction_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 10);
static const s_task_function_uid k_task_function_subtraction_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 11);
static const s_task_function_uid k_task_function_subtraction_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 12);

static const s_task_function_uid k_task_function_multiplication_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 20);
static const s_task_function_uid k_task_function_multiplication_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 21);

static const s_task_function_uid k_task_function_division_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 30);
static const s_task_function_uid k_task_function_division_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 31);
static const s_task_function_uid k_task_function_division_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 32);

static const s_task_function_uid k_task_function_modulo_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 40);
static const s_task_function_uid k_task_function_modulo_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 41);
static const s_task_function_uid k_task_function_modulo_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 42);

static const s_task_function_uid k_task_function_real_array_dereference_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 50);
static const s_task_function_uid k_task_function_real_array_dereference_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 51);

static void task_function_negation_in_out(const s_task_function_context &context) {
	s_buffer_operation_negation::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_negation_inout(const s_task_function_context &context) {
	s_buffer_operation_negation::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_addition_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_addition::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_addition_inout_in(const s_task_function_context &context) {
	s_buffer_operation_addition::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_subtraction_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_subtraction::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_subtraction_inout_in(const s_task_function_context &context) {
	s_buffer_operation_subtraction::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_subtraction_in_inout(const s_task_function_context &context) {
	s_buffer_operation_subtraction::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_multiplication_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_multiplication::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_multiplication_inout_in(const s_task_function_context &context) {
	s_buffer_operation_multiplication::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_division_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_division::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_division_inout_in(const s_task_function_context &context) {
	s_buffer_operation_division::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_division_in_inout(const s_task_function_context &context) {
	s_buffer_operation_division::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_modulo_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_modulo::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_modulo_inout_in(const s_task_function_context &context) {
	s_buffer_operation_modulo::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_modulo_in_inout(const s_task_function_context &context) {
	s_buffer_operation_modulo::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static uint32 get_index_or_invalid(uint32 array_count, real32 real_index) {
	// If the value is NaN or infinity, all bits in the exponent are 1. To avoid two function calls, we can just use a
	// quick bitwise test.
	//int32 is_invalid = std::isnan(real_index) | std::isinf(real_index);
	static const uint32 k_exponent_mask = 0x7f800000;
	uint32 real_index_bits = *reinterpret_cast<const uint32 *>(&real_index);
	int32 is_invalid = (real_index_bits & k_exponent_mask) == k_exponent_mask;

	// If is_invalid is true, this might have an undefined value, but that's fine
	int32 index = static_cast<int32>(std::floor(real_index));

	is_invalid |= (index < 0);
	is_invalid |= (static_cast<uint32>(index) >= array_count);

	// OR with validity mask -0 == 0, -1 == 0xffffffff
	index |= -is_invalid;
	return static_cast<uint32>(index);
}

static void task_function_real_array_dereference_in_out(const s_task_function_context &context) {
	c_real_array real_array = context.arguments[0].get_real_array_in();
	c_real_buffer_or_constant_in index = context.arguments[1].get_real_buffer_or_constant_in();
	c_real_buffer_out out = context.arguments[2].get_real_buffer_out();

	validate_buffer(out);
	validate_buffer(index);

	wl_vassert(!index.is_constant() || index.is_constant_buffer(),
		"Constant-index dereference should have been optimized away");

	uint32 array_count = cast_integer_verify<uint32>(real_array.get_count());
	real32 *out_ptr = out->get_data<real32>();
	const real32 *index_ptr = index.get_buffer()->get_data<real32>();

	if (real_array.get_count() == 0) {
		// The array is empty, therefore all values will dereference to an invalid index, so we can set the result to a
		// constant 0.
		*out_ptr = 0.0f;
		out->set_constant(true);

		// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are sure
		// that the array has at least 1 element.
	} else if (index.is_constant()) {
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
				c_real_buffer_in array_buffer = context.get_buffer_by_index(element.buffer_index_value);

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
		if (context.arguments[0].is_constant()) {
			// The entire array is constant values. This case is optimized to avoid branching.
			for (size_t index = 0; index < context.buffer_size; index++) {
				uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
				int32 array_index_is_valid = (array_index != 0xffffffff);
				// 0 if the index is invalid, 0xffffffff otherwise
				int32 array_dereference_mask = -array_index_is_valid;

				// This will turn the index into 0 if it is invalid
				array_index &= array_dereference_mask;
				// It is always safe to dereference the 0th element due to the first branch in this function
				const s_real_array_element &element = real_array[array_index];
				wl_assert(element.is_constant);

				// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
				int32 dereferenced_value_int =
					*reinterpret_cast<const int32 *>(&element.constant_value) & array_dereference_mask;
				real32 dereferenced_value =
					*reinterpret_cast<const real32 *>(&dereferenced_value_int);

				out_ptr[index] = dereferenced_value;
			}
		} else {
			// The array has some buffers as values. We haven't completely removed branching in this case.
			for (size_t index = 0; index < context.buffer_size; index++) {
				uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
				int32 array_index_is_valid = (array_index != 0xffffffff);
				// 0 if the index is invalid, 0xffffffff otherwise
				int32 array_dereference_mask = -array_index_is_valid;

				// This will turn the index into 0 if it is invalid
				array_index &= array_dereference_mask;
				// It is always safe to dereference the 0th element due to the first branch in this function
				const s_real_array_element &element = real_array[array_index];
				real32 dereferenced_value;

				if (element.is_constant || !array_index_is_valid) {
					// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
					int32 dereferenced_value_int =
						*reinterpret_cast<const int32 *>(&element.constant_value) & array_dereference_mask;
					dereferenced_value =
						*reinterpret_cast<const real32 *>(&dereferenced_value_int);
				} else {
					c_real_buffer_in array_buffer = context.get_buffer_by_index(element.buffer_index_value);
					uint32 dereference_index = array_index;

					// Zero if constant, 0xffffffff otherwise
					int32 array_buffer_non_constant = !array_buffer->is_constant();
					int32 dereference_index_mask = -array_buffer_non_constant;
					dereference_index &= dereference_index_mask;

					dereferenced_value = array_buffer->get_data<real32>()[dereference_index];
				}

				out_ptr[index] = dereferenced_value;
			}
		}

		out->set_constant(false);
	}
}

static void task_function_real_array_dereference_inout(const s_task_function_context &context) {
	c_real_array real_array = context.arguments[0].get_real_array_in();
	c_real_buffer_inout index_out = context.arguments[1].get_real_buffer_inout();

	validate_buffer(index_out);

	uint32 array_count = cast_integer_verify<uint32>(real_array.get_count());
	real32 *index_out_ptr = index_out->get_data<real32>();

	if (real_array.get_count() == 0) {
		// The array is empty, therefore all values will dereference to an invalid index, so we can set the result to a
		// constant 0.
		*index_out_ptr = 0.0f;
		index_out->set_constant(true);

		// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are sure
		// that the array has at least 1 element.
	} else if (index_out->is_constant()) {
		uint32 array_index = get_index_or_invalid(array_count, *index_out_ptr);

		if (array_index == 0xffffffff) {
			*index_out_ptr = 0.0f;
			index_out->set_constant(true);
		} else {
			const s_real_array_element &element = real_array[array_index];
			if (element.is_constant) {
				*index_out_ptr = element.constant_value;
				index_out->set_constant(true);
			} else {
				c_real_buffer_in array_buffer = context.get_buffer_by_index(element.buffer_index_value);

				if (array_buffer->is_constant()) {
					*index_out_ptr = *array_buffer->get_data<real32>();
					index_out->set_constant(true);
				} else {
					memcpy(index_out_ptr, array_buffer->get_data<real32>(), context.buffer_size * sizeof(real32));
					index_out->set_constant(false);
				}
			}
		}
	} else {
		if (context.arguments[0].is_constant()) {
			// The entire array is constant values. This case is optimized to avoid branching.
			for (size_t index = 0; index < context.buffer_size; index++) {
				uint32 array_index = get_index_or_invalid(array_count, index_out_ptr[index]);
				int32 array_index_is_valid = (array_index != 0xffffffff);
				// 0 if the index is invalid, 0xffffffff otherwise
				int32 array_dereference_mask = -array_index_is_valid;

				// This will turn the index into 0 if it is invalid
				array_index &= array_dereference_mask;
				// It is always safe to dereference the 0th element due to the first branch in this function
				const s_real_array_element &element = real_array[array_index];
				wl_assert(element.is_constant);

				// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
				int32 dereferenced_value_int =
					*reinterpret_cast<const int32 *>(&element.constant_value) & array_dereference_mask;
				real32 dereferenced_value =
					*reinterpret_cast<const real32 *>(&dereferenced_value_int);

				index_out_ptr[index] = dereferenced_value;
			}
		} else {
			// The array has some buffers as values. We haven't completely removed branching in this case.
			for (size_t index = 0; index < context.buffer_size; index++) {
				uint32 array_index = get_index_or_invalid(array_count, index_out_ptr[index]);
				int32 array_index_is_valid = (array_index != 0xffffffff);
				// 0 if the index is invalid, 0xffffffff otherwise
				int32 array_dereference_mask = -array_index_is_valid;

				// This will turn the index into 0 if it is invalid
				array_index &= array_dereference_mask;
				// It is always safe to dereference the 0th element due to the first branch in this function
				const s_real_array_element &element = real_array[array_index];
				real32 dereferenced_value;

				if (element.is_constant || !array_index_is_valid) {
					// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
					int32 dereferenced_value_int =
						*reinterpret_cast<const int32 *>(&element.constant_value) & array_dereference_mask;
					dereferenced_value =
						*reinterpret_cast<const real32 *>(&dereferenced_value_int);
				} else {
					c_real_buffer_in array_buffer = context.get_buffer_by_index(element.buffer_index_value);
					uint32 dereference_index = array_index;

					// Zero if constant, 0xffffffff otherwise
					int32 array_buffer_non_constant = !array_buffer->is_constant();
					int32 dereference_index_mask = -array_buffer_non_constant;
					dereference_index &= dereference_index_mask;

					dereferenced_value = array_buffer->get_data<real32>()[dereference_index];
				}

				index_out_ptr[index] = dereferenced_value;
			}
		}

		index_out->set_constant(false);
	}
}

void register_task_functions_basic() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_in_out_uid,
				"negation_in_out",
				nullptr, nullptr, nullptr, task_function_negation_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_inout_uid,
				"negation_inout",
				nullptr, nullptr, nullptr, task_function_negation_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_negation_inout_uid, "b.",
				s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_negation_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_negation_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_in_in_out_uid,
				"addition_in_in_out",
				nullptr, nullptr, nullptr, task_function_addition_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_inout_in_uid,
				"addition_inout_in",
				nullptr, nullptr, nullptr, task_function_addition_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_addition_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_addition_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_addition_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_addition_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_in_in_out_uid,
				"subtraction_in_in_out",
				nullptr, nullptr, nullptr, task_function_subtraction_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_inout_in_uid,
				"subtraction_inout_in",
				nullptr, nullptr, nullptr, task_function_subtraction_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_in_inout_uid,
				"subtraction_in_inout",
				nullptr, nullptr, nullptr, task_function_subtraction_in_inout,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_subtraction_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_subtraction_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_subtraction_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_in_in_out_uid,
				"multiplication_in_in_out",
				nullptr, nullptr, nullptr, task_function_multiplication_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_inout_in_uid,
				"multiplication_inout_in",
				nullptr, nullptr, nullptr, task_function_multiplication_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_multiplication_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_multiplication_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_in_in_out_uid,
				"division_in_in_out",
				nullptr, nullptr, nullptr, task_function_division_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_inout_in_uid,
				"division_inout_in",
				nullptr, nullptr, nullptr, task_function_division_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_in_inout_uid,
				"division_in_inout",
				nullptr, nullptr, nullptr, task_function_division_in_inout,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_division_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_division_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_division_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_division_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_in_in_out_uid,
				"modulo_in_in_out",
				nullptr, nullptr, nullptr, task_function_modulo_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_inout_in_uid,
				"modulo_inout_in",
				nullptr, nullptr, nullptr, task_function_modulo_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_in_inout_uid,
				"modulo_in_inout",
				nullptr, nullptr, nullptr, task_function_modulo_in_inout,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_modulo_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_modulo_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_modulo_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_modulo_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_array_dereference_in_out_uid,
				"real_array_dereference_in_out",
				nullptr, nullptr, nullptr, task_function_real_array_dereference_in_out,
				s_task_function_argument_list::build(TDT(real_array_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_array_dereference_inout_uid,
				"real_array_dereference_inout",
				nullptr, nullptr, nullptr, task_function_real_array_dereference_inout,
				s_task_function_argument_list::build(TDT(real_array_in), TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_real_array_dereference_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_real_array_dereference_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_real_array_dereference_uid, c_task_function_mapping_list::construct(mappings));
	}
}