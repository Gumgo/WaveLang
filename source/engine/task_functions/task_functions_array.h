#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_ARRAY_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_ARRAY_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_array.h"

namespace array_task_functions wl_library(k_array_library_id, "array", 0) {

	wl_task_function(0xbeaf383d, "dereference_real_in_out", "dereference$real")
	void dereference_real_in_out(const s_task_function_context &context,
		wl_in_source("a") c_real_array a,
		wl_in_source("index") c_real_const_buffer_or_constant index,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x8b9d039b, "dereference_real_inout", "dereference$real")
	void dereference_real_inout(const s_task_function_context &context,
		wl_in_source("a") c_real_array a,
		wl_inout_source("index", "result") c_real_buffer *index_result);

	wl_task_function(0x91b5380b, "dereference_bool_in_out", "dereference$bool")
	void dereference_bool_in_out(const s_task_function_context &context,
		wl_in_source("a") c_bool_array a,
		wl_in_source("index") c_real_const_buffer_or_constant index,
		wl_out_source("result") c_bool_buffer *result);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_ARRAY_H__
