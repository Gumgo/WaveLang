#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_time.h"

namespace time_task_functions wl_library(k_time_library_id, "time", 0) {

	wl_task_memory_query
	size_t period_memory_query(const s_task_function_context &context);

	wl_task_initializer
	void period_initializer(
		const s_task_function_context &context,
		wl_in_source("duration") real32 duration);

	wl_task_voice_initializer
	void period_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xee27c86e, "period_out", "period")
	wl_task_memory_query_function("period_memory_query")
	wl_task_initializer_function("period_initializer")
	wl_task_voice_initializer_function("period_voice_initializer")
	void period_out(
		const s_task_function_context &context,
		wl_in_source("duration") real32 duration,
		wl_out_source("result") c_real_buffer *result);

}

