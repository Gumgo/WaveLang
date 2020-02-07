#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_delay.h"

namespace delay_task_functions wl_library(k_delay_library_id, "delay", 0) {

	wl_task_memory_query
	size_t delay_memory_query(
		const s_task_function_context &context,
		wl_in_source("duration") real32 duration);

	wl_task_initializer
	void delay_initializer(
		const s_task_function_context &context,
		wl_in_source("duration") real32 duration);

	wl_task_voice_initializer
	void delay_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xbd8b8e85, "delay_in_out", "delay")
	wl_task_memory_query_function("delay_memory_query")
	wl_task_initializer_function("delay_initializer")
	wl_task_voice_initializer_function("delay_voice_initializer")
	void delay_in_out(
		const s_task_function_context &context,
		wl_in_source("signal") c_real_const_buffer_or_constant signal,
		wl_out_source("result") c_real_buffer *result);

	wl_task_memory_query
	size_t memory_real_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void memory_real_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xbe5eb8bd, "memory_real_in_in_out", "memory$real")
	wl_task_memory_query_function("memory_real_memory_query")
	wl_task_voice_initializer_function("memory_real_voice_initializer")
	void memory_real_in_in_out(
		const s_task_function_context &context,
		wl_in_source("value") c_real_const_buffer_or_constant value,
		wl_in_source("write") c_bool_const_buffer_or_constant write,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x5913de92, "memory_real_inout_in", "memory$real")
	wl_task_memory_query_function("memory_real_memory_query")
	wl_task_voice_initializer_function("memory_real_voice_initializer")
	void memory_real_inout_in(
		const s_task_function_context &context,
		wl_inout_source("value", "result") c_real_buffer *value_result,
		wl_in_source("write") c_bool_const_buffer_or_constant write);

	wl_task_memory_query
	size_t memory_bool_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void memory_bool_voice_initializer(const s_task_function_context &context);

	wl_task_function(0x1fbebc67, "memory_bool_in_in_out", "memory$bool")
	wl_task_memory_query_function("memory_bool_memory_query")
	wl_task_voice_initializer_function("memory_bool_voice_initializer")
	void memory_bool_in_in_out(
		const s_task_function_context &context,
		wl_in_source("value") c_bool_const_buffer_or_constant value,
		wl_in_source("write") c_bool_const_buffer_or_constant write,
		wl_out_source("result") c_bool_buffer *result);

	wl_task_function(0x9c57fa4f, "memory_bool_inout_in", "memory$bool")
	wl_task_memory_query_function("memory_bool_memory_query")
	wl_task_voice_initializer_function("memory_bool_voice_initializer")
	void memory_bool_inout_in(
		const s_task_function_context &context,
		wl_inout_source("value", "result") c_bool_buffer *value_result,
		wl_in_source("write") c_bool_const_buffer_or_constant write);

	wl_task_function(0xee93bfac, "memory_bool_in_inout", "memory$bool")
	wl_task_memory_query_function("memory_bool_memory_query")
	wl_task_voice_initializer_function("memory_bool_voice_initializer")
	void memory_bool_in_inout(
		const s_task_function_context &context,
		wl_in_source("value") c_bool_const_buffer_or_constant value,
		wl_inout_source("write", "result") c_bool_buffer *write_result);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__
