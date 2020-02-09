#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/task_functions/task_functions_math.h"

struct s_op_abs {
	real32x4 operator()(const real32x4 &a) const { return abs(a); }
};

struct s_op_floor {
	real32x4 operator()(const real32x4 &a) const { return floor(a); }
};

struct s_op_ceil {
	real32x4 operator()(const real32x4 &a) const { return ceil(a); }
};

struct s_op_round {
	real32x4 operator()(const real32x4 &a) const { return round(a); }
};

struct s_op_min {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return min(a, b); }
};

struct s_op_max {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return max(a, b); }
};

struct s_op_exp {
	real32x4 operator()(const real32x4 &a) const { return exp(a); }
};

struct s_op_log {
	real32x4 operator()(const real32x4 &a) const { return log(a); }
};

struct s_op_sqrt {
	real32x4 operator()(const real32x4 &a) const { return sqrt(a); }
};

struct s_op_pow {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return pow(a, b); }
};

struct s_op_pow_reverse {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return pow(b, a); }
};

struct s_op_pow_constant_base {
	real32x4 log_a;
	s_op_pow_constant_base(real32 a) : log_a(std::log(a)) {}
	real32x4 operator()(const real32x4 &b) const { return exp(log_a * b); }
};

struct s_op_sin {
	real32x4 operator()(const real32x4 &a) const { return sin(a); }
};

struct s_op_cos {
	real32x4 operator()(const real32x4 &a) const { return cos(a); }
};

struct s_op_sincos {
	void operator()(const real32x4 &a, real32x4 &out_sin, real32x4 &out_cos) const {
		sincos(a, out_sin, out_cos);
	}
};

using s_buffer_operation_abs = s_buffer_operation_real_real<s_op_abs>;
using s_buffer_operation_floor = s_buffer_operation_real_real<s_op_floor>;
using s_buffer_operation_ceil = s_buffer_operation_real_real<s_op_ceil>;
using s_buffer_operation_round = s_buffer_operation_real_real<s_op_round>;
using s_buffer_operation_min = s_buffer_operation_real_real_real<s_op_min>;
using s_buffer_operation_max = s_buffer_operation_real_real_real<s_op_max>;
using s_buffer_operation_exp = s_buffer_operation_real_real<s_op_exp>;
using s_buffer_operation_log = s_buffer_operation_real_real<s_op_log >;
using s_buffer_operation_sqrt = s_buffer_operation_real_real<s_op_sqrt>;
//using s_buffer_operation_pow = s_buffer_operation_real_real_real<s_op_pow>;
using s_buffer_operation_sin = s_buffer_operation_real_real<s_op_sin>;
using s_buffer_operation_cos = s_buffer_operation_real_real<s_op_cos>;
using s_buffer_operation_sincos = s_buffer_operation_real_real_real<s_op_sincos>;

// Exponentiation is special because it is cheaper if the base is constant
struct s_buffer_operation_pow {
	static void in_in_out(
		size_t buffer_size,
		c_real_buffer_or_constant_in a,
		c_real_buffer_or_constant_in b,
		c_real_buffer_out c) {
		if (a.is_constant()) {
			real32 base = a.get_constant();
			buffer_operator_in_out(
				s_op_pow_constant_base(base),
				buffer_size,
				c_iterable_buffer_real_in(b),
				c_iterable_buffer_real_out(c));
		} else {
			buffer_operator_in_in_out(
				s_op_pow(),
				buffer_size,
				c_iterable_buffer_real_in(a),
				c_iterable_buffer_real_in(b),
				c_iterable_buffer_real_out(c));
		}
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout a, c_real_buffer_or_constant_in b) {
		if (a->is_constant()) {
			real32 base = *a->get_data<real32>();
			buffer_operator_in_out(
				s_op_pow_constant_base(base),
				buffer_size,
				c_iterable_buffer_real_in(b),
				c_iterable_buffer_real_out(a));
		} else {
			buffer_operator_inout_in(
				s_op_pow(),
				buffer_size,
				c_iterable_buffer_real_inout(a),
				c_iterable_buffer_real_in(b));
		}
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in a, c_real_buffer_inout b) {
		if (a.is_constant()) {
			real32 base = a.get_constant();
			buffer_operator_inout(
				s_op_pow_constant_base(base),
				buffer_size,
				c_iterable_buffer_real_inout(b));
		} else {
			buffer_operator_inout_in(
				s_op_pow_reverse(),
				buffer_size,
				c_iterable_buffer_real_inout(b),
				c_iterable_buffer_real_in(a));
		}
	}
};

namespace math_task_functions {

	void abs_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_abs::in_out(context.buffer_size, a, result);
	}

	void abs_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_abs::inout(context.buffer_size, a_result);
	}

	void floor_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_floor::in_out(context.buffer_size, a, result);
	}

	void floor_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_floor::inout(context.buffer_size, a_result);
	}

	void ceil_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_ceil::in_out(context.buffer_size, a, result);
	}

	void ceil_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_ceil::inout(context.buffer_size, a_result);
	}

	void round_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_round::in_out(context.buffer_size, a, result);
	}

	void round_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_round::inout(context.buffer_size, a_result);
	}

	void min_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_min::in_in_out(context.buffer_size, a, b, result);
	}

	void min_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_min::inout_in(context.buffer_size, a_result, b);
	}

	void min_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_min::in_inout(context.buffer_size, a, b_result);
	}

	void max_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_max::in_in_out(context.buffer_size, a, b, result);
	}

	void max_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_max::inout_in(context.buffer_size, a_result, b);
	}

	void max_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_max::in_inout(context.buffer_size, a, b_result);
	}

	void exp_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_exp::in_out(context.buffer_size, a, result);
	}

	void exp_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_exp::inout(context.buffer_size, a_result);
	}

	void log_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_log::in_out(context.buffer_size, a, result);
	}

	void log_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_log::inout(context.buffer_size, a_result);
	}

	void sqrt_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_sqrt::in_out(context.buffer_size, a, result);
	}

	void sqrt_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_sqrt::inout(context.buffer_size, a_result);
	}

	void pow_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_pow::in_in_out(context.buffer_size, a, b, result);
	}

	void pow_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_pow::inout_in(context.buffer_size, a_result, b);
	}

	void pow_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_pow::in_inout(context.buffer_size, a, b_result);
	}

	void sin_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_sin::in_out(context.buffer_size, a, result);
	}

	void sin_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_sin::inout(context.buffer_size, a_result);
	}

	void cos_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_cos::in_out(context.buffer_size, a, result);
	}

	void cos_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_cos::inout(context.buffer_size, a_result);
	}

	void sincos_in_out_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *out_sin,
		c_real_buffer *out_cos) {
		s_buffer_operation_sincos::in_out_out(context.buffer_size, a, out_sin, out_cos);
	}

	void sincos_inout_out(
		const s_task_function_context &context,
		c_real_buffer *a_out_sin,
		c_real_buffer *out_cos) {
		s_buffer_operation_sincos::inout_out(context.buffer_size, a_out_sin, out_cos);
	}

	void sincos_out_inout(
		const s_task_function_context &context,
		c_real_buffer *out_sin,
		c_real_buffer *a_out_cos) {
		s_buffer_operation_sincos::out_inout(context.buffer_size, out_sin, a_out_cos);
	}

}
