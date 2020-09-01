#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_core.h"

namespace core_task_functions wl_library(k_core_library_id, "core", 0) {

	// Real

	wl_task_function(0x54ae3577, "negation", "negation")
	void negation(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xc9171617, "addition", "addition")
	void addition(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x1f1d8e92, "subtraction", "subtraction")
	void subtraction(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer * a,
		wl_source("b") const c_real_buffer * b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xb81d2e65, "multiplication", "multiplication")
	void multiplication(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x1bce0fd6, "division", "division")
	void division(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xaa104145, "modulo", "modulo")
	void modulo(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	// Bool

	wl_task_function(0x796d904f, "not", "not")
	void not_(
		const s_task_function_context &context,
		wl_source("a") const c_bool_buffer *a,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xb81092bf, "equal_real", "equal$real")
	void equal_real(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0x09b92133, "not_equal_real", "not_equal$real")
	void not_equal_real(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xfdb20dfa, "equal_bool", "equal$bool")
	void equal_bool(
		const s_task_function_context &context,
		wl_source("a") const c_bool_buffer *a,
		wl_source("b") const c_bool_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xd0f31354, "not_equal_bool", "not_equal$bool")
	void not_equal_bool(
		const s_task_function_context &context,
		wl_source("a") const c_bool_buffer *a,
		wl_source("b") const c_bool_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0x80b0f714, "greater", "greater")
	void greater(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xd51c2202, "less", "less")
	void less(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xabd7961b, "greater_equal", "greater_equal")
	void greater_equal(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xbe4a3f1c, "less_equal", "less_equal")
	void less_equal(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0xa63e91eb, "and", "and")
	void and_(
		const s_task_function_context &context,
		wl_source("a") const c_bool_buffer *a,
		wl_source("b") const c_bool_buffer *b,
		wl_source("result") c_bool_buffer *result);

	wl_task_function(0x0655ac08, "or", "or")
	void or_(
		const s_task_function_context &context,
		wl_source("a") const c_bool_buffer *a,
		wl_source("b") const c_bool_buffer *b,
		wl_source("result") c_bool_buffer *result);

	// Select

	wl_task_function(0x716c993c, "select_real", "select$real")
	void select_real(
		const s_task_function_context &context,
		wl_source("condition") const c_bool_buffer *condition,
		wl_source("true_value") const c_real_buffer *true_value,
		wl_source("false_value") const c_real_buffer *false_value,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xd5383677, "select_bool", "select$bool")
	void select_bool(
		const s_task_function_context &context,
		wl_source("condition") const c_bool_buffer *condition,
		wl_source("true_value") const c_bool_buffer *true_value,
		wl_source("false_value") const c_bool_buffer *false_value,
		wl_source("result") c_bool_buffer *result);

}
