#include "engine/buffer_operations/buffer_operations_arithmetic.h"

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

void buffer_operation_assignment_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in) {
	buffer_operator_out_in(s_op_assignment(), buffer_size, out, in);
}

void buffer_operation_assignment_constant(size_t buffer_size,
	c_buffer_out out, real32 in) {
	buffer_operator_out_in(s_op_assignment(), buffer_size, out, in);
}

void buffer_operation_negation_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in) {
	buffer_operator_out_in(s_op_negation(), buffer_size, out, in);
}

void buffer_operation_negation_bufferio(size_t buffer_size,
	c_buffer_inout inout) {
	buffer_operator_inout(s_op_negation(), buffer_size, inout);
}

void buffer_operation_addition_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_addition(), buffer_size, out, in_a, in_b);
}

void buffer_operation_addition_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	buffer_operator_inout_in(s_op_addition(), buffer_size, inout, in);
}

void buffer_operation_addition_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b) {
	buffer_operator_out_in_in(s_op_addition(), buffer_size, out, in_a, in_b);
}

void buffer_operation_addition_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in) {
	buffer_operator_inout_in(s_op_addition(), buffer_size, inout, in);
}

void buffer_operation_subtraction_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_subtraction(), buffer_size, out, in_a, in_b);
}

void buffer_operation_subtraction_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	buffer_operator_inout_in(s_op_subtraction(), buffer_size, inout, in);
}

void buffer_operation_subtraction_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_subtraction_reverse(), buffer_size, inout, in);
}

void buffer_operation_subtraction_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b) {
	buffer_operator_out_in_in(s_op_subtraction(), buffer_size, out, in_a, in_b);
}

void buffer_operation_subtraction_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in) {
	buffer_operator_inout_in(s_op_subtraction(), buffer_size, inout, in);
}

void buffer_operation_subtraction_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_subtraction_reverse(), buffer_size, out, in_b, in_a);
}

void buffer_operation_subtraction_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_subtraction_reverse(), buffer_size, inout, in);
}

void buffer_operation_multiplication_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_multiplication(), buffer_size, out, in_a, in_b);
}

void buffer_operation_multiplication_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	buffer_operator_inout_in(s_op_multiplication(), buffer_size, inout, in);
}

void buffer_operation_multiplication_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b) {
	buffer_operator_out_in_in(s_op_multiplication(), buffer_size, out, in_a, in_b);
}

void buffer_operation_multiplication_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in) {
	buffer_operator_inout_in(s_op_multiplication(), buffer_size, inout, in);
}

void buffer_operation_division_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_division(), buffer_size, out, in_a, in_b);
}

void buffer_operation_division_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	buffer_operator_inout_in(s_op_division(), buffer_size, inout, in);
}

void buffer_operation_division_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_division_reverse(), buffer_size, inout, in);
}

void buffer_operation_division_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b) {
	buffer_operator_out_in_in(s_op_division(), buffer_size, out, in_a, in_b);
}

void buffer_operation_division_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in) {
	buffer_operator_inout_in(s_op_division(), buffer_size, inout, in);
}

void buffer_operation_division_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_division_reverse(), buffer_size, out, in_b, in_a);
}

void buffer_operation_division_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_division_reverse(), buffer_size, inout, in);
}

void buffer_operation_modulo_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_modulo(), buffer_size, out, in_a, in_b);
}

void buffer_operation_modulo_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	buffer_operator_inout_in(s_op_modulo(), buffer_size, inout, in);
}

void buffer_operation_modulo_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_modulo_reverse(), buffer_size, inout, in);
}

void buffer_operation_modulo_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b) {
	buffer_operator_out_in_in(s_op_modulo(), buffer_size, out, in_a, in_b);
}

void buffer_operation_modulo_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in) {
	buffer_operator_inout_in(s_op_modulo(), buffer_size, inout, in);
}

void buffer_operation_modulo_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b) {
	buffer_operator_out_in_in(s_op_modulo_reverse(), buffer_size, out, in_b, in_a);
}

void buffer_operation_modulo_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout) {
	buffer_operator_inout_in(s_op_modulo_reverse(), buffer_size, inout, in);
}
