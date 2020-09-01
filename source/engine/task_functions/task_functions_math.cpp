#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_functions/task_functions_math.h"

namespace math_task_functions {

	void abs(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = -a;
			});
	}

	void floor(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = floor(a);
			});
	}

	void ceil(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = ceil(a);
			});
	}

	void round(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = round(a);
			});
	}

	void min(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = min(a, b);
			});
	}

	void max(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, b, result,
			[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
				result = max(a, b);
			});
	}

	void exp(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = exp(a);
			});
	}

	void log(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = log(a);
			});
	}

	void sqrt(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = sqrt(a);
			});
	}

	void pow(
		const s_task_function_context &context,
		const c_real_buffer *a,
		const c_real_buffer *b,
		c_real_buffer *result) {
		// Exponentiation is special because it is cheaper if the base is constant
		if (a->is_constant()) {
			real32 base = a->get_constant();
			real32x4 log_base = log(real32x4(base));
			iterate_buffers<4, true>(context.buffer_size, b, result,
				[&log_base](size_t i, const real32x4 &b, real32x4 &result) {
					result = exp(log_base * b);
				});
		} else {
			iterate_buffers<4, true>(context.buffer_size, a, b, result,
				[](size_t i, const real32x4 &a, const real32x4 &b, real32x4 &result) {
					result = pow(a, b);
				});
		}
	}

	void sin(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = sin(a);
			});
	}

	void cos(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *result) {
		iterate_buffers<4, true>(context.buffer_size, a, result,
			[](size_t i, const real32x4 &a, real32x4 &result) {
				result = cos(a);
			});
	}

	void sincos(
		const s_task_function_context &context,
		const c_real_buffer *a,
		c_real_buffer *out_sin,
		c_real_buffer *out_cos) {
		iterate_buffers<4, true>(context.buffer_size, a, out_sin, out_cos,
			[](size_t i, const real32x4 &a, real32x4 &out_sin, real32x4 &out_cos) {
				sincos(a, out_sin, out_cos);
			});
	}

}
