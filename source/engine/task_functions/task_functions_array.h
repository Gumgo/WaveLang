#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_array.h"

namespace array_task_functions wl_library(k_array_library_id, "array", 0) {

	wl_task_function(0xc02d173d, "dereference_real", "dereference$real")
	void dereference_real(
		const s_task_function_context &context,
		wl_source("a") c_real_buffer_array_in a,
		wl_source("index") const c_real_buffer *index,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x91b5380b, "dereference_bool", "dereference$bool")
	void dereference_bool(
		const s_task_function_context &context,
		wl_source("a") c_bool_buffer_array_in a,
		wl_source("index") const c_real_buffer *index,
		wl_source("result") c_bool_buffer *result);

}

