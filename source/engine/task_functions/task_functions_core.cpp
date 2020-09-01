#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_functions/task_functions_core.h"

namespace core_task_functions {

	void negation(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = -a;
			});
	}

	void addition(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = a + b;
			});
	}

	void subtraction(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = a - b;
			});
	}

	void multiplication(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		// Detect multiply by zero, since it's a common case
		if ((a->is_constant() && a->get_constant() == 0.0f) || (b->is_constant() && b->get_constant() == 0.0f)) {
			result->assign_constant(0.0f);
			return;
		}

		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = a * b;
			});
	}

	void division(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = a / b;
			});
	}

	void modulo(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = a % b;
			});
	}

	void not_(
		const s_task_function_context &context,
		const c_bool_buffer *a,
		c_bool_buffer *result) {
		iterate_buffers<128, true>(context.buffer_size, a, result,
			[](size_t i, const int32x4 &a, int32x4 &result) {
				result = ~a;
			});
	}

	void equal_real(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a == b;
			});
	}

	void not_equal_real(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a != b;
			});
	}

	void equal_bool(
		const s_task_function_context &context,
		const c_bool_buffer *a,
		const c_bool_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<128, true>(context.buffer_size, a, b, result,
			[](size_t i, const int32x4 &a, const int32x4 &b, int32x4 &result) {
				result = ~(a ^ b);
			});
	}

	void not_equal_bool(
		const s_task_function_context &context,
		const c_bool_buffer *a,
		const c_bool_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<128, true>(context.buffer_size, a, b, result,
			[](size_t i, const int32x4 &a, const int32x4 &b, int32x4 &result) {
				result = a ^ b;
			});
	}

	void greater(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a > b;
			});
	}

	void less(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a < b;
			});
	}

	void greater_equal(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a >= b;
			});
	}

	void less_equal(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, int32x4 &result) {
				result = a <= b;
			});
	}

	void and_(
		const s_task_function_context &context,
		const c_bool_buffer *a,
		const c_bool_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<128, true>(context.buffer_size, a, b, result,
			[](size_t i, const int32x4 &a, const int32x4 &b, int32x4 &result) {
				result = a & b;
			});
	}

	void or_(
		const s_task_function_context &context,
		const c_bool_buffer *a,
		const c_bool_buffer *b,
		c_bool_buffer *result) {
		iterate_buffers<128, true>(context.buffer_size, a, b, result,
			[](size_t i, const int32x4 &a, const int32x4 &b, int32x4 &result) {
				result = a | b;
			});
	}

	void select_real(
		const s_task_function_context &context,
		const c_bool_buffer *condition,
		const c_real_buffer *true_value,
		const c_real_buffer *false_value,
		c_real_buffer *result) {
		// Optimize case where the condition is constant
		if (condition->is_constant()) {
			const c_real_buffer *value = condition->get_constant() ? true_value : false_value;
			if (value->is_constant()) {
				result->assign_constant(value->get_constant());
			} else {
				memcpy(result->get_data(), value->get_data(), context.buffer_size * sizeof(real32));
				result->set_is_constant(false);
			}
		} else {
			iterate_buffers<4, true>(context.buffer_size, condition, true_value, false_value, result,
				[](
					size_t i,
					const int32x4 &condition,
					const real32x4 &true_value,
					const real32x4 &false_value,
					real32x4 &result) {
						result = reinterpret_bits<real32x4>(
							(condition & reinterpret_bits<int32x4>(true_value))
							| (~condition & reinterpret_bits<int32x4>(false_value)));
				});
		}
	}

	void select_bool(
		const s_task_function_context &context,
		wl_source("condition") const c_bool_buffer *condition,
		wl_source("true_value") const c_bool_buffer *true_value,
		wl_source("false_value") const c_bool_buffer *false_value,
		wl_source("result") c_bool_buffer *result) {
		// Optimize case where the condition is constant
		if (condition->is_constant()) {
			const c_bool_buffer *value = condition->get_constant() ? true_value : false_value;
			if (value->is_constant()) {
				result->assign_constant(value->get_constant());
			} else {
				memcpy(
					result->get_data(),
					value->get_data(),
					bool_buffer_int32_count(context.buffer_size) * sizeof(int32));
				result->set_is_constant(false);
			}
		} else {
			iterate_buffers<128, true>(context.buffer_size, condition, true_value, false_value, result,
				[](
					size_t i,
					const int32x4 &condition,
					const int32x4 &true_value,
					const int32x4 &false_value,
					int32x4 &result) {
					result = (condition & true_value) | (~condition & false_value);
				});
		}
	}

}
