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

typedef s_buffer_operation_1_input<s_op_assignment> s_buffer_operation_assignment;
typedef s_buffer_operation_1_input<s_op_negation> s_buffer_operation_negation;
typedef s_buffer_operation_2_inputs<s_op_addition> s_buffer_operation_addition;
typedef s_buffer_operation_2_inputs<s_op_subtraction, s_op_subtraction_reverse> s_buffer_operation_subtraction;

// Multiplication is a special case because we can mark buffers as constant if we're multiplying by 0
struct s_buffer_operation_multiplication {
	static void buffer_buffer(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
		if (in_a->is_constant()) {
			const real32 *in_a_ptr = in_a->get_data<real32>();
			real32 in_a_val = *in_a_ptr;
			if (in_a_val == 0.0f) {
				real32 *out_ptr = out->get_data<real32>();
				c_real32_4(0.0f).store(out_ptr);
				out->set_constant(true);
				return;
			}
		}

		if (in_b->is_constant()) {
			const real32 *in_b_ptr = in_b->get_data<real32>();
			real32 in_b_val = *in_b_ptr;
			if (in_b_val == 0.0f) {
				real32 *out_ptr = out->get_data<real32>();
				c_real32_4(0.0f).store(out_ptr);
				out->set_constant(true);
				return;
			}
		}

		buffer_operator_out_in_in(s_op_multiplication(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_buffer(size_t buffer_size, c_buffer_inout inout, c_buffer_in in) {
		if (inout->is_constant()) {
			real32 *inout_ptr = inout->get_data<real32>();
			real32 inout_val = *inout_ptr;
			if (inout_val == 0.0f) {
				c_real32_4(0.0f).store(inout_ptr);
				return;
			}
		}

		if (in->is_constant()) {
			const real32 *in_ptr = in->get_data<real32>();
			real32 in_val = *in_ptr;
			if (in_val == 0.0f) {
				real32 *inout_ptr = inout->get_data<real32>();
				c_real32_4(0.0f).store(inout_ptr);
				inout->set_constant(true);
				return;
			}
		}

		buffer_operator_inout_in(s_op_multiplication(), buffer_size, inout, in);
	}

	static void buffer_constant(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, real32 in_b) {
		// in_b should never be 0 because we would have removed this in the execution graph optimization phase
		buffer_operator_out_in_in(s_op_multiplication(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_constant(size_t buffer_size, c_buffer_inout inout, real32 in) {
		// in should never be 0 because we would have removed this in the execution graph optimization phase
		buffer_operator_inout_in(s_op_multiplication(), buffer_size, inout, in);
	}
};

typedef s_buffer_operation_2_inputs<s_op_division, s_op_division_reverse> s_buffer_operation_division;
typedef s_buffer_operation_2_inputs<s_op_modulo, s_op_modulo_reverse> s_buffer_operation_modulo;

#endif // WAVELANG_BUFFER_OPERATIONS_INTERNAL_H__