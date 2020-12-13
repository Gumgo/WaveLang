#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_functions/task_functions_array.h"

#include <cmath>

static constexpr uint32 k_invalid_array_index = 0xffffffffu;

static uint32 get_index_or_invalid(uint32 array_count, real32 real_index) {
	// If the value is NaN or infinity, all bits in the exponent are 1. To avoid two function calls, we can just use a
	// quick bitwise test.
	//int32 is_invalid = std::isnan(real_index) | std::isinf(real_index);
	static constexpr uint32 k_exponent_mask = 0x7f800000;
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

	void subscript_real(
		const s_task_function_context &context,
		c_real_buffer_array_in a,
		const c_real_buffer *index,
		c_real_buffer *result) {
		wl_vassert(!index->is_compile_time_constant(), "Constant-index subscript should have been optimized away");

		uint32 array_count = cast_integer_verify<uint32>(a.get_count());
		if (array_count == 0) {
			// The array is empty, therefore all values will dereference to an invalid index, so we can set the result
			// to a constant 0.
			result->assign_constant(0.0f);

			// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are
			// sure that the array has at least 1 element.
		} else if (index->is_constant()) {
			uint32 array_index = get_index_or_invalid(array_count, index->get_constant());

			if (array_index == k_invalid_array_index) {
				result->assign_constant(0.0f);
			} else {
				const c_real_buffer *element = a[array_index];
				if (element->is_constant()) {
					result->assign_constant(element->get_constant());
				} else {
					copy_type(result->get_data(), element->get_data(), context.buffer_size);
					result->set_is_constant(false);
				}
			}
		} else {
			iterate_buffers<1, false>(context.buffer_size, index, result,
				[&a, &array_count](size_t i, real32 index, real32 &result) {
					// This is written using masks to avoid branching
					// Note: could SIMD-optimize part of this using gather
					uint32 array_index = get_index_or_invalid(array_count, index);
					int32 array_index_is_valid = (array_index != k_invalid_array_index);

					// Sign-extend to turn this into a mask, then AND the array index to convert invalid indices to 0
					int32 array_index_mask = -array_index_is_valid;
					array_index &= array_index_mask;

					// It is always safe to dereference the 0th element due to the first branch in this function
					const c_real_buffer *element = a[array_index];

					// Dereference the element, masking with 0 if the element is constant and thus only has a 0th value
					// We use ptrdiff_t for sign extension purposes since it is probably the same size as size_t
					STATIC_ASSERT_MSG(sizeof(size_t) == sizeof(ptrdiff_t), "Need a signed size_t equivalent");
					ptrdiff_t element_is_constant = element->is_constant();
					ptrdiff_t element_index_mask = ~(-element_is_constant);
					size_t element_index = i & element_index_mask;

					real32 value = element->get_data()[element_index];

					// AND with the array dereference mask to eliminate invalid values
					result = reinterpret_bits<real32>(reinterpret_bits<int32>(value) & array_index_mask);
				});
		}
	}

	void subscript_bool(
		const s_task_function_context &context,
		c_bool_buffer_array_in a,
		const c_real_buffer *index,
		c_bool_buffer *result) {
		wl_vassert(!index->is_compile_time_constant(), "Constant-index subscript should have been optimized away");

		uint32 array_count = cast_integer_verify<uint32>(a.get_count());
		if (array_count == 0) {
			// The array is empty, therefore all values will dereference to an invalid index, so we can set the result
			// to a constant 0.
			result->assign_constant(false);

			// Note: it is guaranteed safe to dereference element 0 of the array in all other branches because we are
			// sure that the array has at least 1 element.
		} else if (index->is_constant()) {
			uint32 array_index = get_index_or_invalid(array_count, index->get_constant());

			if (array_index == k_invalid_array_index) {
				result->assign_constant(0.0f);
			} else {
				const c_bool_buffer *element = a[array_index];
				if (element->is_constant()) {
					result->assign_constant(element->get_constant());
				} else {
					copy_type(
						result->get_data(),
						element->get_data(),
						bool_buffer_int32_count(context.buffer_size));
					result->set_is_constant(false);
				}
			}
		} else {
			iterate_buffers<1, false>(context.buffer_size, index, result,
				[&a, &array_count](size_t i, real32 index, bool &result) {
					// This is written using masks to avoid branching
					uint32 array_index = get_index_or_invalid(array_count, index);
					int32 array_index_is_valid = (array_index != k_invalid_array_index);

					// Sign-extend to turn this into a mask, then AND the array index to convert invalid indices to 0
					int32 array_index_mask = -array_index_is_valid;
					array_index &= array_index_mask;

					// It is always safe to dereference the 0th element due to the first branch in this function
					const c_bool_buffer *element = a[array_index];

					// Dereference the element, masking with 0 if the element is constant and thus only has a 0th value
					// We use ptrdiff_t for sign extension purposes since it is probably the same size as size_t
					STATIC_ASSERT_MSG(sizeof(size_t) == sizeof(ptrdiff_t), "Need a signed size_t equivalent");
					ptrdiff_t element_is_constant = element->is_constant();
					ptrdiff_t element_index_mask = ~(-element_is_constant);
					size_t element_index = i & element_index_mask;

					int32 value = element->get_data()[element_index / 32];
					value = (value >> (element_index % 32)) & 1;

					// AND with the array dereference mask to eliminate invalid values
					result = (value & array_index_mask) != 0;
				});
		}
	}

}
