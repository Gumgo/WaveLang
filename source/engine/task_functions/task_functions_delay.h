#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_modules/native_modules_delay.h"

#include "task_function/task_function.h"

namespace delay_task_functions wl_library(k_delay_library_id, "delay", 0) {

	wl_task_memory_query
	size_t delay_memory_query(
		const s_task_function_context &context,
		wl_source("duration") real32 duration);

	wl_task_initializer
	void delay_initializer(
		const s_task_function_context &context,
		wl_source("duration") real32 duration);

	wl_task_voice_initializer
	void delay_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xbd8b8e85, "delay", "delay")
	wl_task_memory_query_function("delay_memory_query")
	wl_task_initializer_function("delay_initializer")
	wl_task_voice_initializer_function("delay_voice_initializer")
	void delay(
		const s_task_function_context &context,
		wl_source("signal") const c_real_buffer *signal,
		wl_source("result") wl_unshared c_real_buffer *result);

	wl_task_memory_query
	size_t memory_real_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void memory_real_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xbe5eb8bd, "memory_real", "memory$real")
	wl_task_memory_query_function("memory_real_memory_query")
	wl_task_voice_initializer_function("memory_real_voice_initializer")
	void memory_real(
		const s_task_function_context &context,
		wl_source("value") const c_real_buffer *value,
		wl_source("write") const c_bool_buffer *write,
		wl_source("result") c_real_buffer *result);

	wl_task_memory_query
	size_t memory_bool_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void memory_bool_voice_initializer(const s_task_function_context &context);

	wl_task_function(0x1fbebc67, "memory_bool", "memory$bool")
	wl_task_memory_query_function("memory_bool_memory_query")
	wl_task_voice_initializer_function("memory_bool_voice_initializer")
	void memory_bool(
		const s_task_function_context &context,
		wl_source("value") const c_bool_buffer *value,
		wl_source("write") const c_bool_buffer *write,
		wl_source("result") c_bool_buffer *result);

}

