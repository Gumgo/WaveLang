#pragma once

#include "common/common.h"

#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

// A set of core buffer operations which are used internally in the executor. These are also wrapped in task functions.

struct s_op_assignment {
	real32x4 operator()(const real32x4 &a) const { return a; }
};

struct s_op_negation {
	real32x4 operator()(const real32x4 &a) const { return -a; }
};

struct s_op_addition {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return a + b; }
};

struct s_op_subtraction {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return a - b; }
};

struct s_op_multiplication {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return a * b; }
};

struct s_op_division {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return a / b; }
};

struct s_op_modulo {
	real32x4 operator()(const real32x4 &a, const real32x4 &b) const { return a % b; }
};

using s_buffer_operation_assignment = s_buffer_operation_real_real<s_op_assignment>;
using s_buffer_operation_negation = s_buffer_operation_real_real<s_op_negation>;
using s_buffer_operation_addition = s_buffer_operation_real_real_real<s_op_addition>;
using s_buffer_operation_subtraction = s_buffer_operation_real_real_real<s_op_subtraction>;

// Multiplication is a special case because we can mark buffers as constant if we're multiplying by 0
struct s_buffer_operation_multiplication {
	static void in_in_out(
		size_t buffer_size,
		c_real_buffer_or_constant_in a,
		c_real_buffer_or_constant_in b,
		c_real_buffer_out c) {
		if ((a.is_constant() && a.get_constant() == 0.0f) ||
			(b.is_constant() && b.get_constant() == 0.0f)) {
			*c->get_data<real32>() = 0.0f;
			c->set_constant(true);
			return;
		}

		buffer_operator_in_in_out(
			s_op_multiplication(),
			buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_in(b),
			c_iterable_buffer_real_out(c));
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout a, c_real_buffer_or_constant_in b) {
		if ((a->is_constant() && *a->get_data<real32>() == 0.0f) ||
			(b.is_constant() && b.get_constant() == 0.0f)) {
			*a->get_data<real32>() = 0.0f;
			a->set_constant(true);
			return;
		}

		buffer_operator_inout_in(
			s_op_multiplication(),
			buffer_size,
			c_iterable_buffer_real_inout(a),
			c_iterable_buffer_real_in(b));
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in a, c_real_buffer_inout b) {
		if ((a.is_constant() && a.get_constant() == 0.0f) ||
			(b->is_constant() && *b->get_data<real32>() == 0.0f)) {
			*b->get_data<real32>() = 0.0f;
			b->set_constant(true);
			return;
		}

		buffer_operator_in_inout(
			s_op_multiplication(),
			buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_inout(b));
	}
};

using s_buffer_operation_division = s_buffer_operation_real_real_real<s_op_division>;
using s_buffer_operation_modulo = s_buffer_operation_real_real_real<s_op_modulo>;

