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

struct s_op_pow_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(a, b); }
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
typedef s_buffer_operation_2_inputs<s_op_pow, s_op_pow_reverse> s_buffer_operation_pow;

#endif // WAVELANG_BUFFER_OPERATIONS_UTILITY_H__