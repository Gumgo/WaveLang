#ifndef WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__
#define WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

void buffer_operation_assignment_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in);
void buffer_operation_assignment_constant(size_t buffer_size,
	c_buffer_out out, real32 in);

void buffer_operation_negation_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in);
void buffer_operation_negation_bufferio(size_t buffer_size,
	c_buffer_inout inout);

void buffer_operation_addition_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b);
void buffer_operation_addition_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in);
void buffer_operation_addition_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b);
void buffer_operation_addition_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in);

void buffer_operation_subtraction_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b);
void buffer_operation_subtraction_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in);
void buffer_operation_subtraction_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout);
void buffer_operation_subtraction_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b);
void buffer_operation_subtraction_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in);
void buffer_operation_subtraction_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b);
void buffer_operation_subtraction_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout);

void buffer_operation_multiplication_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b);
void buffer_operation_multiplication_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in);
void buffer_operation_multiplication_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b);
void buffer_operation_multiplication_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in);

void buffer_operation_division_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b);
void buffer_operation_division_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in);
void buffer_operation_division_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout);
void buffer_operation_division_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b);
void buffer_operation_division_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in);
void buffer_operation_division_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b);
void buffer_operation_division_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout);

void buffer_operation_modulo_buffer_buffer(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b);
void buffer_operation_modulo_bufferio_buffer(size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in);
void buffer_operation_modulo_buffer_bufferio(size_t buffer_size,
	c_buffer_in in, c_buffer_inout inout);
void buffer_operation_modulo_buffer_constant(size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, real32 in_b);
void buffer_operation_modulo_bufferio_constant(size_t buffer_size,
	c_buffer_inout inout, real32 in);
void buffer_operation_modulo_constant_buffer(size_t buffer_size,
	c_buffer_out out, real32 in_a, c_buffer_in in_b);
void buffer_operation_modulo_constant_bufferio(size_t buffer_size,
	real32 in, c_buffer_inout inout);

#endif // WAVELANG_BUFFER_OPERATIONS_ARITHMETIC_H__