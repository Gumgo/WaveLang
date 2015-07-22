#ifndef WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__
#define WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

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
typedef s_buffer_operation_2_inputs<s_op_multiplication> s_buffer_operation_multiplication;
typedef s_buffer_operation_2_inputs<s_op_division, s_op_division_reverse> s_buffer_operation_division;
typedef s_buffer_operation_2_inputs<s_op_modulo, s_op_modulo_reverse> s_buffer_operation_modulo;

#endif // WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__