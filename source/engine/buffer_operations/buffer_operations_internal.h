#ifndef WAVELANG_BUFFER_OPERATIONS_INTERNAL_H__
#define WAVELANG_BUFFER_OPERATIONS_INTERNAL_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

// A set of core buffer operations which are used internally in the executor. These are also wrapped in task functions.

struct s_op_assignment {
	c_real32_4 operator()(const c_real32_4 &a) const { return a; }
};

struct s_op_negation {
	c_real32_4 operator()(const c_real32_4 &a) const { return -a; }
};

struct s_op_addition {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a + b; }
};

struct s_op_subtraction {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a - b; }
};

struct s_op_subtraction_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return b - a; }
};

struct s_op_multiplication {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a * b; }
};

struct s_op_division {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a / b; }
};

struct s_op_division_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return b / a; }
};

struct s_op_modulo {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a % b; }
};

struct s_op_modulo_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return b % a; }
};

typedef s_buffer_operation_real<s_op_assignment> s_buffer_operation_assignment;
typedef s_buffer_operation_real<s_op_negation> s_buffer_operation_negation;
typedef s_buffer_operation_real_real<s_op_addition> s_buffer_operation_addition;
typedef s_buffer_operation_real_real<s_op_subtraction, s_op_subtraction_reverse> s_buffer_operation_subtraction;

// Multiplication is a special case because we can mark buffers as constant if we're multiplying by 0
struct s_buffer_operation_multiplication {
	static void in_in_out(size_t buffer_size, c_real_buffer_or_constant_in in_a, c_real_buffer_or_constant_in in_b,
		c_real_buffer_out out) {
		if ((in_a.is_constant() && in_a.get_constant() == 0.0f) ||
			(in_b.is_constant() && in_b.get_constant() == 0.0f)) {
			*out->get_data<real32>() = 0.0f;
			out->set_constant(true);
			return;
		}

		buffer_operator_real_in_real_in_real_out(s_op_multiplication(), buffer_size, in_a, in_b, out);
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout inout_a, c_real_buffer_or_constant_in in_b) {
		if ((inout_a->is_constant() && *inout_a->get_data<real32>() == 0.0f) ||
			(in_b.is_constant() && in_b.get_constant() == 0.0f)) {
			*inout_a->get_data<real32>() = 0.0f;
			inout_a->set_constant(true);
			return;
		}

		buffer_operator_real_inout_real_in(s_op_multiplication(), buffer_size, inout_a, in_b);
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in in_a, c_real_buffer_inout inout_b) {
		if ((in_a.is_constant() && in_a.get_constant() == 0.0f) ||
			(inout_b->is_constant() && *inout_b->get_data<real32>() == 0.0f)) {
			*inout_b->get_data<real32>() = 0.0f;
			inout_b->set_constant(true);
			return;
		}

		buffer_operator_real_inout_real_in(s_op_multiplication(), buffer_size, inout_b, in_a);
	}
};

typedef s_buffer_operation_real_real<s_op_division, s_op_division_reverse> s_buffer_operation_division;
typedef s_buffer_operation_real_real<s_op_modulo, s_op_modulo_reverse> s_buffer_operation_modulo;

#endif // WAVELANG_BUFFER_OPERATIONS_INTERNAL_H__