#include "engine/buffer.h"
#include "engine/array_dereference_interface/array_dereference_interface.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/buffer_operations/buffer_operations_internal.h"
#include "engine/task_function_registry.h"
#include "engine/task_functions/task_functions_array.h"

static uint32 get_index_or_invalid(uint32 array_count, real32 real_index) {
	// If the value is NaN or infinity, all bits in the exponent are 1. To avoid two function calls, we can just use a
	// quick bitwise test.
	//int32 is_invalid = std::isnan(real_index) | std::isinf(real_index);
	static const uint32 k_exponent_mask = 0x7f800000;
	uint32 real_index_bits = reinterpret_bits<uint32>(real_index);
	int32 is_invalid = (real_index_bits & k_exponent_mask) == k_exponent_mask;

	// If is_invalid is true, this might have an undefined value, but that's fine
	int32 index = static_cast<int32>(std::floor(real_index));

	is_invalid |= (index < 0);
	is_invalid |= (static_cast<uint32>(index) >= array_count);

	// OR with validity mask -0 == 0, -1 == 0xffffffff
	index |= -is_invalid;
	return static_cast<uint32>(index);
}

namespace array_task_functions {

	void dereference_real_in_out(
		const s_task_function_context &context,
		c_real_array a,
		c_real_const_buffer_or_constant index,
		c_real_buffer *result) {
		wl_vassert(!index.is_constant() || index.is_constant_buffer(),
			"Constant-index dereference should have been optimized away");

		uint32 array_count = cast_integer_verify<uint32>(a.get_count());
		real32 *out_ptr = result->get_data<real32>();
		const real32 *index_ptr = index.get_buffer()->get_data<real32>();

		if (a.get_count() == 0) {
			// The array is empty, therefore all values will dereference to an invalid index, so we can set the result
			// to a constant 0.
			*out_ptr = 0.0f;
			result->set_constant(true);

			// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are
			// sure that the array has at least 1 element.
		} else if (index.is_constant()) {
			uint32 array_index = get_index_or_invalid(array_count, *index_ptr);

			if (array_index == 0xffffffff) {
				*out_ptr = 0.0f;
				result->set_constant(true);
			} else {
				const s_real_array_element &element = a[array_index];
				if (element.is_constant) {
					*out_ptr = element.constant_value;
					result->set_constant(true);
				} else {
					c_real_buffer_in array_buffer =
						context.array_dereference_interface->get_buffer(element.buffer_handle_value);

					if (array_buffer->is_constant()) {
						*out_ptr = *array_buffer->get_data<real32>();
						result->set_constant(true);
					} else {
						memcpy(out_ptr, array_buffer->get_data<real32>(), context.buffer_size * sizeof(real32));
						result->set_constant(false);
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
					const s_real_array_element &element = a[array_index];
					wl_assert(element.is_constant);

					// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
					int32 dereferenced_value_int =
						reinterpret_bits<int32>(element.constant_value) & array_dereference_mask;
					real32 dereferenced_value =
						reinterpret_bits<real32>(dereferenced_value_int);

					out_ptr[index] = dereferenced_value;
				}
			} else {
				// The array has some buffers as values. We haven't completely removed branching in this case.
				for (uint32 index = 0; index < context.buffer_size; index++) {
					uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
					int32 array_index_is_valid = (array_index != 0xffffffff);
					// 0 if the index is invalid, 0xffffffff otherwise
					int32 array_dereference_mask = -array_index_is_valid;

					// This will turn the index into 0 if it is invalid
					array_index &= array_dereference_mask;
					// It is always safe to dereference the 0th element due to the first branch in this function
					const s_real_array_element &element = a[array_index];
					real32 dereferenced_value;

					if (element.is_constant || !array_index_is_valid) {
						// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
						int32 dereferenced_value_int =
							reinterpret_bits<int32>(element.constant_value) & array_dereference_mask;
						dereferenced_value =
							reinterpret_bits<real32>(dereferenced_value_int);
					} else {
						c_real_buffer_in array_buffer =
							context.array_dereference_interface->get_buffer(element.buffer_handle_value);
						uint32 dereference_index = index;

						// Zero if constant, 0xffffffff otherwise
						int32 array_buffer_non_constant = !array_buffer->is_constant();
						int32 dereference_index_mask = -array_buffer_non_constant;
						dereference_index &= dereference_index_mask;

						dereferenced_value = array_buffer->get_data<real32>()[dereference_index];
					}

					out_ptr[index] = dereferenced_value;
				}
			}

			result->set_constant(false);
		}
	}

	void dereference_real_inout(
		const s_task_function_context &context,
		c_real_array a,
		c_real_buffer *index_result) {
		validate_buffer(index_result);

		uint32 array_count = cast_integer_verify<uint32>(a.get_count());
		real32 *index_out_ptr = index_result->get_data<real32>();

		if (a.get_count() == 0) {
			// The array is empty, therefore all values will dereference to an invalid index, so we can set the result
			// to a constant 0.
			*index_out_ptr = 0.0f;
			index_result->set_constant(true);

			// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are
			// sure that the array has at least 1 element.
		} else if (index_result->is_constant()) {
			uint32 array_index = get_index_or_invalid(array_count, *index_out_ptr);

			if (array_index == 0xffffffff) {
				*index_out_ptr = 0.0f;
				index_result->set_constant(true);
			} else {
				const s_real_array_element &element = a[array_index];
				if (element.is_constant) {
					*index_out_ptr = element.constant_value;
					index_result->set_constant(true);
				} else {
					c_real_buffer_in array_buffer =
						context.array_dereference_interface->get_buffer(element.buffer_handle_value);

					if (array_buffer->is_constant()) {
						*index_out_ptr = *array_buffer->get_data<real32>();
						index_result->set_constant(true);
					} else {
						memcpy(index_out_ptr, array_buffer->get_data<real32>(), context.buffer_size * sizeof(real32));
						index_result->set_constant(false);
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
					const s_real_array_element &element = a[array_index];
					wl_assert(element.is_constant);

					// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
					int32 dereferenced_value_int =
						reinterpret_bits<int32>(element.constant_value) & array_dereference_mask;
					real32 dereferenced_value =
						reinterpret_bits<real32>(dereferenced_value_int);

					index_out_ptr[index] = dereferenced_value;
				}
			} else {
				// The array has some buffers as values. We haven't completely removed branching in this case.
				for (uint32 index = 0; index < context.buffer_size; index++) {
					uint32 array_index = get_index_or_invalid(array_count, index_out_ptr[index]);
					int32 array_index_is_valid = (array_index != 0xffffffff);
					// 0 if the index is invalid, 0xffffffff otherwise
					int32 array_dereference_mask = -array_index_is_valid;

					// This will turn the index into 0 if it is invalid
					array_index &= array_dereference_mask;
					// It is always safe to dereference the 0th element due to the first branch in this function
					const s_real_array_element &element = a[array_index];
					real32 dereferenced_value;

					if (element.is_constant || !array_index_is_valid) {
						// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
						int32 dereferenced_value_int =
							reinterpret_bits<int32>(element.constant_value) & array_dereference_mask;
						dereferenced_value =
							reinterpret_bits<real32>(dereferenced_value_int);
					} else {
						c_real_buffer_in array_buffer =
							context.array_dereference_interface->get_buffer(element.buffer_handle_value);
						uint32 dereference_index = index;

						// Zero if constant, 0xffffffff otherwise
						int32 array_buffer_non_constant = !array_buffer->is_constant();
						int32 dereference_index_mask = -array_buffer_non_constant;
						dereference_index &= dereference_index_mask;

						dereferenced_value = array_buffer->get_data<real32>()[dereference_index];
					}

					index_out_ptr[index] = dereferenced_value;
				}
			}

			index_result->set_constant(false);
		}
	}

	void dereference_bool_in_out(
		const s_task_function_context &context,
		c_bool_array a,
		c_real_const_buffer_or_constant index,
		c_bool_buffer *result) {
		wl_vassert(!index.is_constant() || index.is_constant_buffer(),
			"Constant-index dereference should have been optimized away");

		uint32 array_count = cast_integer_verify<uint32>(a.get_count());
		int32 *out_ptr = result->get_data<int32>();
		const real32 *index_ptr = index.get_buffer()->get_data<real32>();

		if (a.get_count() == 0) {
			// The array is empty, therefore all values will dereference to an invalid index, so we can set the result
			// to a constant false.
			*out_ptr = 0;
			result->set_constant(true);

			// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are
			// sure that the array has at least 1 element.
		} else if (index.is_constant()) {
			uint32 array_index = get_index_or_invalid(array_count, *index_ptr);

			if (array_index == 0xffffffff) {
				*out_ptr = 0;
				result->set_constant(true);
			} else {
				const s_bool_array_element &element = a[array_index];
				if (element.is_constant) {
					*out_ptr = -static_cast<int32>(element.constant_value);
					result->set_constant(true);
				} else {
					c_bool_buffer_in array_buffer =
						context.array_dereference_interface->get_buffer(element.buffer_handle_value);

					if (array_buffer->is_constant()) {
						*out_ptr = *array_buffer->get_data<int32>();
						result->set_constant(true);
					} else {
						memcpy(
							out_ptr,
							array_buffer->get_data<int32>(),
							BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(real32));
						result->set_constant(false);
					}
				}
			}
		} else {
			if (context.arguments[0].is_constant()) {
				// The entire array is constant values. This case is optimized to avoid branching.
				for (uint32 index_outer = 0; index_outer < context.buffer_size; index_outer += 32) {
					int32 value = 0;

					for (uint32 index = index_outer; index < index_outer + 32 && index < context.buffer_size; index++) {
						uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
						int32 array_index_is_valid = (array_index != 0xffffffff);
						// 0 if the index is invalid, 0xffffffff otherwise
						int32 array_dereference_mask = -array_index_is_valid;

						// This will turn the index into 0 if it is invalid
						array_index &= array_dereference_mask;
						// It is always safe to dereference the 0th element due to the first branch in this function
						const s_bool_array_element &element = a[array_index];
						wl_assert(element.is_constant);

						// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
						int32 dereferenced_value = static_cast<int32>(element.constant_value) & array_dereference_mask;

						value |= (dereferenced_value << (index - index_outer));
					}

					out_ptr[index_outer / 32] = value;
				}
			} else {
				// The array has some buffers as values. We haven't completely removed branching in this case.
				for (uint32 index_outer = 0; index_outer < context.buffer_size; index_outer += 32) {
					int32 value = 0;

					for (uint32 index = index_outer; index < index_outer + 32 && index < context.buffer_size; index++) {
						uint32 array_index = get_index_or_invalid(array_count, index_ptr[index]);
						int32 array_index_is_valid = (array_index != 0xffffffff);
						// 0 if the index is invalid, 0xffffffff otherwise
						int32 array_dereference_mask = -array_index_is_valid;

						// This will turn the index into 0 if it is invalid
						array_index &= array_dereference_mask;
						// It is always safe to dereference the 0th element due to the first branch in this function
						const s_bool_array_element &element = a[array_index];
						int32 dereferenced_value;

						if (element.is_constant || !array_index_is_valid) {
							// AND the value with the dereference mask. This will turn it into 0 if the index was invalid.
							dereferenced_value = static_cast<int32>(element.constant_value) & array_dereference_mask;
						} else {
							c_bool_buffer_in array_buffer =
								context.array_dereference_interface->get_buffer(element.buffer_handle_value);
							uint32 dereference_index = index;

							// Zero if constant, 0xffffffff otherwise
							int32 array_buffer_non_constant = !array_buffer->is_constant();
							int32 dereference_index_mask = -array_buffer_non_constant;
							dereference_index &= dereference_index_mask;

							dereferenced_value = array_buffer->get_data<int32>()[dereference_index / 32];

							// Shift the correct bit into position 0 and mask it
							dereferenced_value >>= (dereference_index % 32);
							dereferenced_value &= 1;
						}

						value |= (dereferenced_value << (index - index_outer));
					}

					out_ptr[index_outer / 32] = value;
				}
			}

			result->set_constant(false);
		}
	}

}
