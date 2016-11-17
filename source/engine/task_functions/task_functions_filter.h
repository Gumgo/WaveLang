#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_FILTER_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_FILTER_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_filter.h"

namespace filter_task_functions wl_library(k_filter_library_id, "filter", 0) {

	wl_task_memory_query
	size_t biquad_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void biquad_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xe6fc480d, "biquad_in_in_in_in_in_in_out", "biquad")
	wl_task_memory_query_function("biquad_memory_query")
	wl_task_voice_initializer_function("biquad_voice_initializer")
	void biquad_in_in_in_in_in_in_out(const s_task_function_context &context,
		wl_in_source("a1") c_real_const_buffer_or_constant a1,
		wl_in_source("a2") c_real_const_buffer_or_constant a2,
		wl_in_source("b0") c_real_const_buffer_or_constant b0,
		wl_in_source("b1") c_real_const_buffer_or_constant b1,
		wl_in_source("b2") c_real_const_buffer_or_constant b2,
		wl_in_source("signal") c_real_const_buffer_or_constant signal,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xe9f102a4, "biquad_inout_in_in_in_in_in", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_inout_in_in_in_in_in(const s_task_function_context &context,
			wl_inout_source("a1", "result") c_real_buffer *a1_result,
			wl_in_source("a2") c_real_const_buffer_or_constant a2,
			wl_in_source("b0") c_real_const_buffer_or_constant b0,
			wl_in_source("b1") c_real_const_buffer_or_constant b1,
			wl_in_source("b2") c_real_const_buffer_or_constant b2,
			wl_in_source("signal") c_real_const_buffer_or_constant signal);

	wl_task_function(0x55803ba7, "biquad_in_inout_in_in_in_in", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_in_inout_in_in_in_in(const s_task_function_context &context,
			wl_in_source("a1") c_real_const_buffer_or_constant a1,
			wl_inout_source("a2", "result") c_real_buffer *a2_result,
			wl_in_source("b0") c_real_const_buffer_or_constant b0,
			wl_in_source("b1") c_real_const_buffer_or_constant b1,
			wl_in_source("b2") c_real_const_buffer_or_constant b2,
			wl_in_source("signal") c_real_const_buffer_or_constant signal);

	wl_task_function(0xd812ccd4, "biquad_in_in_inout_in_in_in", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_in_in_inout_in_in_in(const s_task_function_context &context,
			wl_in_source("a1") c_real_const_buffer_or_constant a1,
			wl_in_source("a2") c_real_const_buffer_or_constant a2,
			wl_inout_source("b0", "result") c_real_buffer *b0_result,
			wl_in_source("b1") c_real_const_buffer_or_constant b1,
			wl_in_source("b2") c_real_const_buffer_or_constant b2,
			wl_in_source("signal") c_real_const_buffer_or_constant signal);

	wl_task_function(0xc75e09b6, "biquad_in_in_in_inout_in_in", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_in_in_in_inout_in_in(const s_task_function_context &context,
			wl_in_source("a1") c_real_const_buffer_or_constant a1,
			wl_in_source("a2") c_real_const_buffer_or_constant a2,
			wl_in_source("b0") c_real_const_buffer_or_constant b0,
			wl_inout_source("b1", "result") c_real_buffer *b1_result,
			wl_in_source("b2") c_real_const_buffer_or_constant b2,
			wl_in_source("signal") c_real_const_buffer_or_constant signal);

	wl_task_function(0x00d64532, "biquad_in_in_in_in_inout_in", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_in_in_in_in_inout_in(const s_task_function_context &context,
			wl_in_source("a1") c_real_const_buffer_or_constant a1,
			wl_in_source("a2") c_real_const_buffer_or_constant a2,
			wl_in_source("b0") c_real_const_buffer_or_constant b0,
			wl_in_source("b1") c_real_const_buffer_or_constant b1,
			wl_inout_source("b2", "result") c_real_buffer *b2_result,
			wl_in_source("signal") c_real_const_buffer_or_constant signal);

	wl_task_function(0xe5689f8c, "biquad_in_in_in_in_in_inout", "biquad")
		wl_task_memory_query_function("biquad_memory_query")
		wl_task_voice_initializer_function("biquad_voice_initializer")
		void biquad_in_in_in_in_in_inout(const s_task_function_context &context,
			wl_in_source("a1") c_real_const_buffer_or_constant a1,
			wl_in_source("a2") c_real_const_buffer_or_constant a2,
			wl_in_source("b0") c_real_const_buffer_or_constant b0,
			wl_in_source("b1") c_real_const_buffer_or_constant b1,
			wl_in_source("b2") c_real_const_buffer_or_constant b2,
			wl_inout_source("signal", "result") c_real_buffer *signal_result);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_FILTER_H__
