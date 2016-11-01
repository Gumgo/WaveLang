#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_delay.h"

namespace delay_task_functions wl_library(k_delay_library_id, "delay", 0) {

	wl_task_memory_query
	size_t delay_memory_query(const s_task_function_context &context,
		wl_in_source("duration") real32 duration);

	wl_task_initializer
	void delay_initializer(const s_task_function_context &context,
		wl_in_source("duration") real32 duration);

	wl_task_voice_initializer
	void delay_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xbd8b8e85, "delay_in_out", "delay")
	wl_task_memory_query_function("delay_memory_query")
	wl_task_initializer_function("delay_initializer")
	wl_task_voice_initializer_function("delay_voice_initializer")
	void delay_in_out(const s_task_function_context &context,
		wl_in_source("signal") c_real_const_buffer_or_constant signal,
		wl_out_source("result") c_real_buffer *result);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_DELAY_H__
