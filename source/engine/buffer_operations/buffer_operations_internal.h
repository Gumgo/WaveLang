#ifndef WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_INTERNAL_H__
#define WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_INTERNAL_H__

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

struct s_op_multiplication {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a * b; }
};

struct s_op_division {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a / b; }
};

struct s_op_modulo {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a % b; }
};

typedef s_buffer_operation_real_real<s_op_assignment> s_buffer_operation_assignment;
typedef s_buffer_operation_real_real<s_op_negation> s_buffer_operation_negation;
typedef s_buffer_operation_real_real_real<s_op_addition> s_buffer_operation_addition;
typedef s_buffer_operation_real_real_real<s_op_subtraction> s_buffer_operation_subtraction;

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

typedef s_buffer_operation_real_real_real<s_op_division> s_buffer_operation_division;
typedef s_buffer_operation_real_real_real<s_op_modulo> s_buffer_operation_modulo;

#endif // WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_INTERNAL_H__
