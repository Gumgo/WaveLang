#ifndef WAVELANG_BUFFER_OPERATIONS_UTILITY_H__
#define WAVELANG_BUFFER_OPERATIONS_UTILITY_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

struct s_op_abs {
	c_real32_4 operator()(const c_real32_4 &a) const { return abs(a); }
};

struct s_op_floor {
	c_real32_4 operator()(const c_real32_4 &a) const { return floor(a); }
};

struct s_op_ceil {
	c_real32_4 operator()(const c_real32_4 &a) const { return ceil(a); }
};

struct s_op_round {
	c_real32_4 operator()(const c_real32_4 &a) const { return round(a); }
};

struct s_op_min {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return min(a, b); }
};

struct s_op_max {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return max(a, b); }
};

struct s_op_exp {
	c_real32_4 operator()(const c_real32_4 &a) const { return exp(a); }
};

struct s_op_log {
	c_real32_4 operator()(const c_real32_4 &a) const { return log(a); }
};

struct s_op_sqrt {
	c_real32_4 operator()(const c_real32_4 &a) const { return sqrt(a); }
};

struct s_op_pow {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(a, b); }
};

struct s_op_pow_constant_base {
	c_real32_4 operator()(const c_real32_4 &log_a, const c_real32_4 &b) const { return exp(b * log_a); }
};

struct s_op_pow_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(b, a); }
};

struct s_op_pow_constant_base_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &log_b) const { return exp(a * log_b); }
};

typedef s_buffer_operation_1_input<s_op_abs> s_buffer_operation_abs;
typedef s_buffer_operation_1_input<s_op_floor> s_buffer_operation_floor;
typedef s_buffer_operation_1_input<s_op_ceil> s_buffer_operation_ceil;
typedef s_buffer_operation_1_input<s_op_round> s_buffer_operation_round;
typedef s_buffer_operation_2_inputs<s_op_min> s_buffer_operation_min;
typedef s_buffer_operation_2_inputs<s_op_max> s_buffer_operation_max;
typedef s_buffer_operation_1_input<s_op_exp> s_buffer_operation_exp;
typedef s_buffer_operation_1_input<s_op_log > s_buffer_operation_log;
typedef s_buffer_operation_1_input<s_op_sqrt> s_buffer_operation_sqrt;

//typedef s_buffer_operation_2_inputs<s_op_pow, s_op_pow_reverse> s_buffer_operation_pow;

// Exponentiation is special because it is cheaper if the base is constant
struct s_buffer_operation_pow {
	static void buffer_buffer(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
		if (in_a->is_constant()) {
			const real32 *in_a_ptr = in_a->get_data<real32>();
			real32 log_in_a = std::log(*in_a_ptr);
			buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, out, in_b, log_in_a);
		} else {
			buffer_operator_out_in_in(s_op_pow(), buffer_size, out, in_a, in_b);
		}
	}

	static void bufferio_buffer(size_t buffer_size, c_buffer_inout inout, c_buffer_in in) {
		if (inout->is_constant()) {
			const real32 *inout_ptr = inout->get_data<real32>();
			real32 log_inout = std::log(*inout_ptr);
			buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, inout, in, log_inout);
		} else {
			buffer_operator_inout_in(s_op_pow(), buffer_size, inout, in);
		}
	}

	static void buffer_bufferio(size_t buffer_size, c_buffer_in in, c_buffer_inout inout) {
		if (in->is_constant()) {
			const real32 *in_ptr = in->get_data<real32>();
			real32 log_in = std::log(*in_ptr);
			buffer_operator_inout_in(s_op_pow_constant_base_reverse(), buffer_size, inout, log_in);
		} else {
			buffer_operator_inout_in(s_op_pow_reverse(), buffer_size, inout, in);
		}
	}

	static void buffer_constant(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, real32 in_b) {
		// If in_a is constant, we will call the non-constant-base version, but only once, so it's fine
		buffer_operator_out_in_in(s_op_pow(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_constant(size_t buffer_size, c_buffer_inout inout, real32 in) {
		// If inout is constant, we will call the non-constant-base version, but only once, so it's fine
		buffer_operator_inout_in(s_op_pow(), buffer_size, inout, in);
	}

	static void constant_buffer(size_t buffer_size, c_buffer_out out, real32 in_a, c_buffer_in in_b) {
		real32 log_in_a = std::log(in_a);
		buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, out, in_b, log_in_a);
	}

	static void constant_bufferio(size_t buffer_size, real32 in, c_buffer_inout inout) {
		real32 log_in = std::log(in);
		buffer_operator_inout_in(s_op_pow_constant_base_reverse(), buffer_size, inout, log_in);
	}
};

#endif // WAVELANG_BUFFER_OPERATIONS_UTILITY_H__