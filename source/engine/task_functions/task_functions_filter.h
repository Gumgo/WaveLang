#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_modules/native_modules_filter.h"

#include "task_function/task_function.h"

namespace filter_task_functions wl_library(k_filter_library_id, "filter", 0) {

	wl_task_memory_query
	size_t biquad_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void biquad_voice_initializer(const s_task_function_context &context);

	// $TODO rename this to iir or sos, change the parameter to an array of N biquad filters
	// Fix the order - should be b0 b1 b2 a1 a2
	wl_task_function(0xe6fc480d, "biquad", "biquad")
	wl_task_memory_query_function("biquad_memory_query")
	wl_task_voice_initializer_function("biquad_voice_initializer")
	void biquad(
		const s_task_function_context &context,
		wl_source("a1") const c_real_buffer *a1,
		wl_source("a2") const c_real_buffer *a2,
		wl_source("b0") const c_real_buffer *b0,
		wl_source("b1") const c_real_buffer *b1,
		wl_source("b2") const c_real_buffer *b2,
		wl_source("signal") const c_real_buffer *signal,
		wl_source("result") c_real_buffer *result);

}
