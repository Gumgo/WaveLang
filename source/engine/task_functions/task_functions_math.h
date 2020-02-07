#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_MATH_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_MATH_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_math.h"

namespace math_task_functions wl_library(k_math_library_id, "math", 0) {

	wl_task_function(0x58acf9ca, "abs_in_out", "abs")
	void abs_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xf76ef694, "abs_inout", "abs")
	void abs_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xd090bfa7, "floor_in_out", "floor")
	void floor_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x0a68a18a, "floor_inout", "floor")
	void floor_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xc0e9f7af, "ceil_in_out", "ceil")
	void ceil_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x4ccf9e78, "ceil_inout", "ceil")
	void ceil_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xd120c3b5, "round_in_out", "round")
	void round_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x85288481, "round_inout", "round")
	void round_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0x17eea946, "min_in_in_out", "min")
	void min_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xe6e175bb, "min_inout_in", "min")
	void min_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0x6763c101, "min_in_inout", "min")
	void min_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0x500ed33c, "max_in_in_out", "max")
	void max_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x719cd972, "max_inout_in", "max")
	void max_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0x9065e49a, "max_in_inout", "max")
	void max_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0xf0d33c62, "exp_in_out", "exp")
	void exp_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xddd6ee91, "exp_inout", "exp")
	void exp_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0x1b308bb4, "log_in_out", "log")
	void log_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xe14c7bad, "log_inout", "log")
	void log_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xf3c4357a, "sqrt_in_out", "sqrt")
	void sqrt_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x412dd926, "sqrt_inout", "sqrt")
	void sqrt_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0x50b6af65, "pow_in_in_out", "pow")
	void pow_in_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_in_source("b") c_real_const_buffer_or_constant b,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x080f1c5e, "pow_inout_in", "pow")
	void pow_inout_in(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result,
		wl_in_source("b") c_real_const_buffer_or_constant b);

	wl_task_function(0xfc761e09, "pow_in_inout", "pow")
	void pow_in_inout(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_inout_source("b", "result") c_real_buffer *b_result);

	wl_task_function(0x6123b4e2, "sin_in_out", "sin")
	void sin_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xc18e5556, "sin_inout", "sin")
	void sin_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xb95cad11, "cos_in_out", "cos")
	void cos_in_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x676f70a7, "cos_inout", "cos")
	void cos_inout(
		const s_task_function_context &context,
		wl_inout_source("a", "result") c_real_buffer *a_result);

	wl_task_function(0xd0448dbf, "sincos_in_out_out", "sincos")
	void sincos_in_out_out(
		const s_task_function_context &context,
		wl_in_source("a") c_real_const_buffer_or_constant a,
		wl_out_source("out_sin") c_real_buffer *out_sin,
		wl_out_source("out_cos") c_real_buffer *out_cos);

	wl_task_function(0x14b9c46f, "sincos_inout_out", "sincos")
	void sincos_inout_out(
		const s_task_function_context &context,
		wl_inout_source("a", "out_sin") c_real_buffer *a_out_sin,
		wl_out_source("out_cos") c_real_buffer *out_cos);

	wl_task_function(0xc9743b41, "sincos_out_inout", "sincos")
	void sincos_out_inout(
		const s_task_function_context &context,
		wl_out_source("out_sin") c_real_buffer *out_sin,
		wl_inout_source("a", "out_cos") c_real_buffer *a_out_cos);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_MATH_H__
