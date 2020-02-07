#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_core.h"

namespace core_task_functions wl_library(k_core_library_id, "core", 0) {

	// Real

	wl_task_function(0x54ae3577, "negation_in_out", "negation")
	void negation_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x973c6c94, "negation_inout", "negation")
	void negation_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xc9171617, "addition_in_in_out", "addition")
	void addition_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x10006377, "addition_inout_in", "addition")
	void addition_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0x0b720f6c, "addition_in_inout", "addition")
	void addition_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0x1f1d8e92, "subtraction_in_in_out", "subtraction")
	void subtraction_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x28dfe312, "subtraction_inout_in", "subtraction")
	void subtraction_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0xc27db6bb, "subtraction_in_inout", "subtraction")
	void subtraction_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0xb81d2e65, "multiplication_in_in_out", "multiplication")
	void multiplication_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x571d56e6, "multiplication_inout_in", "multiplication")
	void multiplication_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0x1df7952a, "multiplication_in_inout", "multiplication")
	void multiplication_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0x1bce0fd6, "division_in_in_out", "division")
	void division_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xa7bb3f37, "division_inout_in", "division")
	void division_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0x3bdfc6f8, "division_in_inout", "division")
	void division_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0xaa104145, "modulo_in_in_out", "modulo")
	void modulo_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xe70e80f6, "modulo_inout_in", "modulo")
	void modulo_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0xa328aeeb, "modulo_in_inout", "modulo")
	void modulo_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	// Bool

	wl_task_function(0x796d904f, "not_in_out", "not")
	void not_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xee389cc1, "not_inout", "not")
	void not_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_bool_buffer *a_result);

	wl_task_function(0xb81092bf, "equal_real_in_in_out", "equal$real")
	void equal_real_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0x09b92133, "not_equal_real_in_in_out", "not_equal$real")
	void not_equal_real_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xfdb20dfa, "equal_bool_in_in_out", "equal$bool")
	void equal_bool_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_in_source("b") c_bool_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xeabfc66c, "equal_bool_inout_in", "equal$bool")
	void equal_bool_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_bool_buffer *a_result,
		wl_in_source("b") c_bool_const_buffer_or_constant b);

	wl_task_function(0xa77322af, "equal_bool_in_inout", "equal$bool")
	void equal_bool_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_bool_buffer *b_result);

	wl_task_function(0xd0f31354, "not_equal_bool_in_in_out", "not_equal$bool")
	void not_equal_bool_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_in_source("b") c_bool_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0x86014eb4, "not_equal_bool_inout_in", "not_equal$bool")
	void not_equal_bool_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_bool_buffer *a_result,
		wl_in_source("b") c_bool_const_buffer_or_constant b);

	wl_task_function(0xf25bcef8, "not_equal_bool_in_inout", "not_equal$bool")
	void not_equal_bool_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_bool_buffer *b_result);

	wl_task_function(0x80b0f714, "greater_in_in_out", "greater")
	void greater_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xd51c2202, "less_in_in_out", "less")
	void less_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xabd7961b, "greater_equal_in_in_out", "greater_equal")
	void greater_equal_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xbe4a3f1c, "less_equal_in_in_out", "less_equal")
	void less_equal_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xa63e91eb, "and_in_in_out", "and")
	void and_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_in_source("b") c_bool_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0x6eac0be0, "and_inout_in", "and")
	void and_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_bool_buffer *a_result,
		wl_in_source("b") c_bool_const_buffer_or_constant b);

	wl_task_function(0x132c4ff5, "and_in_inout", "and")
	void and_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_bool_buffer *b_result);

	wl_task_function(0x0655ac08, "or_in_in_out", "or")
	void or_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_in_source("b") c_bool_const_buffer_or_constant b,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0xed881b8f, "or_inout_in", "or")
	void or_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_bool_buffer *a_result,
		wl_in_source("b") c_bool_const_buffer_or_constant b);

	wl_task_function(0xd4d15e21, "or_in_inout", "or")
	void or_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_bool_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_bool_buffer *b_result);

	// Select

	wl_task_function(0x716c993c, "select_real_in_in_in_out", "select$real")
	void select_real_in_in_in_out(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_in_source("true_value") c_real_const_buffer_or_constant true_value,
		wl_in_source("false_value") c_real_const_buffer_or_constant false_value,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x66ba606b, "select_real_in_inout_in", "select$real")
	void select_real_in_inout_in(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_inout_source("true_value", "result") c_real_buffer *true_value_result,
		wl_in_source("false_value") c_real_const_buffer_or_constant false_value);

	wl_task_function(0x5c6ef5c8, "select_real_in_in_inout", "select$real")
	void select_real_in_in_inout(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_in_source("true_value") c_real_const_buffer_or_constant true_value,
		wl_inout_source("false_value", "result") c_real_buffer *false_value_result);

	wl_task_function(0xd5383677, "select_bool_in_in_in_out", "select$bool")
	void select_bool_in_in_in_out(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_in_source("true_value") c_bool_const_buffer_or_constant true_value,
		wl_in_source("false_value") c_bool_const_buffer_or_constant false_value,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0x4f26db76, "select_bool_inout_in_in", "select$bool")
	void select_bool_inout_in_in(
		const s_task_function_context &context,
		wl_inout_source("condition", "result") c_bool_buffer *condition_result,
		wl_in_source("true_value") c_bool_const_buffer_or_constant true_value,
		wl_in_source("false_value") c_bool_const_buffer_or_constant false_value);

	wl_task_function(0xe3639fd0, "select_bool_in_inout_in", "select$bool")
	void select_bool_in_inout_in(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_inout_source("true_value", "result") c_bool_buffer *true_value_result,
		wl_in_source("false_value") c_bool_const_buffer_or_constant false_value);

	wl_task_function(0x72c3c39c, "select_bool_in_in_inout", "select$bool")
	void select_bool_in_in_inout(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_in_source("true_value") c_bool_const_buffer_or_constant true_value,
		wl_inout_source("false_value", "result") c_bool_buffer *false_value_result);

}

