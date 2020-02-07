#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_SAMPLER_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_SAMPLER_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "engine/task_function.h"

#include "execution_graph/native_modules/native_modules_sampler.h"

namespace sampler_task_functions wl_library(k_sampler_library_id, "sampler", 0) {

	wl_task_memory_query
	size_t sampler_memory_query(const s_task_function_context &context);

	wl_task_initializer
	void sampler_initializer(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("channel") real32 channel);

	wl_task_voice_initializer
	void sampler_voice_initializer(const s_task_function_context &context);

	wl_task_function(0x8cd13477, "sampler_in_out", "sampler")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_in_out(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("speed") c_real_const_buffer_or_constant speed,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xad2a9605, "sampler_inout", "sampler")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_inout(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_inout_source("speed", "result") c_real_buffer *speed_result);

	wl_task_initializer
	void sampler_loop_initializer(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("channel") real32 channel,
		wl_in_source("bidi") bool bidi,
		wl_in_source("phase") c_real_const_buffer_or_constant phase); // We can use this if it's a constant

	wl_task_function(0xdf906f59, "sampler_loop_in_in_out", "sampler_loop")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_loop_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_loop_in_in_out(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("speed") c_real_const_buffer_or_constant speed,
		wl_in_source("phase") c_real_const_buffer_or_constant phase,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0xdd0b489f, "sampler_loop_inout_in", "sampler_loop")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_loop_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_loop_inout_in(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_inout_source("speed", "result") c_real_buffer *speed_result,
		wl_in_source("phase") c_real_const_buffer_or_constant phase);

	// Version where phase cannot ever be a constant
	wl_task_initializer
	void sampler_loop_in_inout_initializer(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("channel") real32 channel,
		wl_in_source("bidi") bool bidi);

	wl_task_function(0x340ed99a, "sampler_loop_in_inout", "sampler_loop")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_loop_in_inout_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_loop_in_inout(
		const s_task_function_context &context,
		wl_in_source("name") const char *name,
		wl_in_source("speed") c_real_const_buffer_or_constant speed,
		wl_inout_source("phase", "result") c_real_buffer *phase_result);

	wl_task_initializer
	void sampler_wavetable_initializer(
		const s_task_function_context &context,
		wl_in_source("harmonic_weights") c_real_array harmonic_weights,
		wl_in_source("sample_count") real32 sample_count,
		wl_in_source("phase") c_real_const_buffer_or_constant phase); // We can use this if it's a constant

	wl_task_function(0x7429e1e3, "sampler_wavetable_in_in_out", "sampler_wavetable")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_wavetable_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_wavetable_in_in_out(
		const s_task_function_context &context,
		wl_in_source("frequency") c_real_const_buffer_or_constant frequency,
		wl_in_source("phase") c_real_const_buffer_or_constant phase,
		wl_out_source("result") c_real_buffer *result);

	wl_task_function(0x079cf631, "sampler_wavetable_inout_in", "sampler_wavetable")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_wavetable_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_wavetable_inout_in(
		const s_task_function_context &context,
		wl_inout_source("frequency", "result") c_real_buffer *frequency_result,
		wl_in_source("phase") c_real_const_buffer_or_constant phase);

	// Version where phase cannot ever be a constant
	wl_task_initializer
	void sampler_wavetable_in_inout_initializer(
		const s_task_function_context &context,
		wl_in_source("harmonic_weights") c_real_array harmonic_weights,
		wl_in_source("sample_count") real32 sample_count);

	wl_task_function(0x9e7c215f, "sampler_wavetable_in_inout", "sampler_wavetable")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_wavetable_in_inout_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_wavetable_in_inout(
		const s_task_function_context &context,
		wl_in_source("frequency") c_real_const_buffer_or_constant frequency,
		wl_inout_source("phase", "result") c_real_buffer *phase_result);

}

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_TASK_FUNCTIONS_SAMPLER_H__
