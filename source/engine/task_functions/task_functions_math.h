#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_math.h"

namespace math_task_functions wl_library(k_math_library_id, "math", 0) {

	wl_task_function(0x58acf9ca, "abs", "abs")
	void abs(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xd090bfa7, "floor", "floor")
	void floor(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xc0e9f7af, "ceil", "ceil")
	void ceil(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xd120c3b5, "round", "round")
	void round(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x17eea946, "min", "min")
	void min(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x500ed33c, "max", "max")
	void max(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xf0d33c62, "exp", "exp")
	void exp(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x1b308bb4, "log", "log")
	void log(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xf3c4357a, "sqrt", "sqrt")
	void sqrt(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x50b6af65, "pow", "pow")
	void pow(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("b") const c_real_buffer *b,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x6123b4e2, "sin", "sin")
	void sin(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xb95cad11, "cos", "cos")
	void cos(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0xd0448dbf, "sincos", "sincos")
	void sincos(
		const s_task_function_context &context,
		wl_source("a") const c_real_buffer *a,
		wl_source("out_sin") c_real_buffer *out_sin,
		wl_source("out_cos") c_real_buffer *out_cos);

}
