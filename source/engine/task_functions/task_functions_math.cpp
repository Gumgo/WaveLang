#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_function_registration.h"

namespace math_task_functions {

	void abs(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = -a;
			});
	}

	void floor(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = floor(a);
			});
	}

	void ceil(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = ceil(a);
			});
	}

	void round(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = round(a);
			});
	}

	void min(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = min(a, b);
			});
	}

	void max(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = max(a, b);
			});
	}

	void exp(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = exp(a);
			});
	}

	void log(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = log(a);
			});
	}

	void sqrt(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = sqrt(a);
			});
	}

	void pow(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		// Exponentiation is special because it is cheaper if the base is constant
		if (a->is_constant()) {
			real32 base = a->get_constant();
			real32xN log_base = log(real32xN(base));
			iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *b, *result,
				[&log_base](size_t i, const real32xN &b, real32xN &result) {
					result = exp(log_base * b);
				});
		} else {
			iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
				[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
					result = pow(a, b);
				});
		}
	}

	void sin(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = sin(a);
			});
	}

	void cos(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = cos(a);
			});
	}

	void sincos(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, sin_out),
		wl_task_argument(c_real_buffer *, cos_out)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *sin_out, *cos_out,
			[](size_t i, const real32xN &a, real32xN &sin_out, real32xN &cos_out) {
				sincos(a, sin_out, cos_out);
			});
	}

	static constexpr uint32 k_math_library_id = 2;
	wl_task_function_library(k_math_library_id, "math", 0);

	wl_task_function(0x58acf9ca, "abs")
		.set_function<abs>();

	wl_task_function(0xd090bfa7, "floor")
		.set_function<floor>();

	wl_task_function(0xc0e9f7af, "ceil")
		.set_function<ceil>();

	wl_task_function(0xd120c3b5, "round")
		.set_function<round>();

	wl_task_function(0x17eea946, "min")
		.set_function<min>();

	wl_task_function(0x500ed33c, "max")
		.set_function<max>();

	wl_task_function(0xf0d33c62, "exp")
		.set_function<exp>();

	wl_task_function(0x1b308bb4, "log")
		.set_function<log>();

	wl_task_function(0xf3c4357a, "sqrt")
		.set_function<sqrt>();

	wl_task_function(0x50b6af65, "pow")
		.set_function<pow>();

	wl_task_function(0x6123b4e2, "sin")
		.set_function<sin>();

	wl_task_function(0xb95cad11, "cos")
		.set_function<cos>();

	wl_task_function(0xd0448dbf, "sincos")
		.set_function<sincos>();

	wl_end_active_library_task_function_registration();

}
