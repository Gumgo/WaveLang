#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_function_registration.h"

namespace core_task_functions {

	void negation(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *result,
			[](size_t i, const real32xN &a, real32xN &result) {
				result = -a;
			});
	}

	void addition(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = a + b;
			});
	}

	void subtraction(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = a - b;
			});
	}

	void multiplication(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		// Detect multiply by zero, since it's a common case
		if ((a->is_constant() && a->get_constant() == 0.0f) || (b->is_constant() && b->get_constant() == 0.0f)) {
			result->assign_constant(0.0f);
			return;
		}

		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = a * b;
			});
	}

	void division(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = a / b;
			});
	}

	void modulo(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_real_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, real32xN &result) {
				result = a % b;
			});
	}

	void not_(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, a),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *a, *result,
			[](size_t i, const int32xN &a, int32xN &result) {
				result = ~a;
			});
	}

	void equal_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a == b;
			});
	}

	void not_equal_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a != b;
			});
	}

	void equal_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, a),
		wl_task_argument(const c_bool_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const int32xN &a, const int32xN &b, int32xN &result) {
				result = ~(a ^ b);
			});
	}

	void not_equal_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, a),
		wl_task_argument(const c_bool_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const int32xN &a, const int32xN &b, int32xN &result) {
				result = a ^ b;
			});
	}

	void greater(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a > b;
			});
	}

	void less(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a < b;
			});
	}

	void greater_equal(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a >= b;
			});
	}

	void less_equal(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, a),
		wl_task_argument(const c_real_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const real32xN &a, const real32xN &b, int32xN &result) {
				result = a <= b;
			});
	}

	void and_(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, a),
		wl_task_argument(const c_bool_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const int32xN &a, const int32xN &b, int32xN &result) {
				result = a & b;
			});
	}

	void or_(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, a),
		wl_task_argument(const c_bool_buffer *, b),
		wl_task_argument(c_bool_buffer *, result)) {
		iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *a, *b, *result,
			[](size_t i, const int32xN &a, const int32xN &b, int32xN &result) {
				result = a | b;
			});
	}

	void select_real(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, condition),
		wl_task_argument(const c_real_buffer *, true_value),
		wl_task_argument(const c_real_buffer *, false_value),
		wl_task_argument(c_real_buffer *, result)) {
		// Optimize case where the condition is constant
		if (condition->is_constant()) {
			const c_real_buffer *value = condition->get_constant() ? true_value : false_value;
			if (value->is_constant()) {
				result->assign_constant(value->get_constant());
			} else {
				copy_type(result->get_data(), value->get_data(), context.buffer_size);
				result->set_is_constant(false);
			}
		} else {
			iterate_buffers<k_simd_32_lanes, true>(context.buffer_size, *condition, *true_value, *false_value, *result,
				[](
					size_t i,
					const int32xN &condition,
					const real32xN &true_value,
					const real32xN &false_value,
					real32xN &result) {
						result = reinterpret_bits<real32xN>(
							(condition & reinterpret_bits<int32xN>(true_value))
							| (~condition & reinterpret_bits<int32xN>(false_value)));
				});
		}
	}

	void select_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, condition),
		wl_task_argument(const c_bool_buffer *, true_value),
		wl_task_argument(const c_bool_buffer *, false_value),
		wl_task_argument(c_bool_buffer *, result)) {
		// Optimize case where the condition is constant
		if (condition->is_constant()) {
			const c_bool_buffer *value = condition->get_constant() ? true_value : false_value;
			if (value->is_constant()) {
				result->assign_constant(value->get_constant());
			} else {
				copy_type(
					result->get_data(),
					value->get_data(),
					bool_buffer_int32_count(context.buffer_size));
				result->set_is_constant(false);
			}
		} else {
			iterate_buffers<k_simd_size_bits, true>(context.buffer_size, *condition, *true_value, *false_value, *result,
				[](
					size_t i,
					const int32xN &condition,
					const int32xN &true_value,
					const int32xN &false_value,
					int32xN &result) {
					result = (condition & true_value) | (~condition & false_value);
				});
		}
	}

	void scrape_task_functions() {
		static constexpr uint32 k_core_library_id = 0;
		wl_task_function_library(k_core_library_id, "core", 0);

		wl_task_function(0x54ae3577, "negation")
			.set_function<negation>();

		wl_task_function(0xc9171617, "addition")
			.set_function<addition>();

		wl_task_function(0x1f1d8e92, "subtraction")
			.set_function<subtraction>();

		wl_task_function(0xb81d2e65, "multiplication")
			.set_function<multiplication>();

		wl_task_function(0x1bce0fd6, "division")
			.set_function<division>();

		wl_task_function(0xaa104145, "modulo")
			.set_function<modulo>();

		wl_task_function(0x796d904f, "not")
			.set_function<not_>();

		wl_task_function(0xb81092bf, "equal$real")
			.set_function<equal_real>();

		wl_task_function(0x09b92133, "not_equal$real")
			.set_function<not_equal_real>();

		wl_task_function(0xfdb20dfa, "equal$bool")
			.set_function<equal_bool>();

		wl_task_function(0xd0f31354, "not_equal$bool")
			.set_function<not_equal_bool>();

		wl_task_function(0x80b0f714, "greater")
			.set_function<greater>();

		wl_task_function(0xd51c2202, "less")
			.set_function<less>();

		wl_task_function(0xabd7961b, "greater_equal")
			.set_function<greater_equal>();

		wl_task_function(0xbe4a3f1c, "less_equal")
			.set_function<less_equal>();

		wl_task_function(0xa63e91eb, "and")
			.set_function<and_>();

		wl_task_function(0x0655ac08, "or")
			.set_function<or_>();

		wl_task_function(0x716c993c, "select$real")
			.set_function<select_real>();

		wl_task_function(0xd5383677, "select$bool")
			.set_function<select_bool>();

		wl_end_active_library_task_function_registration();
	}

}
