#pragma once

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
		wl_source("name") const char *name,
		wl_source("channel") real32 channel);

	wl_task_voice_initializer
	void sampler_voice_initializer(const s_task_function_context &context);

	wl_task_function(0x8cd13477, "sampler", "sampler")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler(
		const s_task_function_context &context,
		wl_source("name") const char *name,
		wl_source("speed") const c_real_buffer *speed,
		wl_source("result") c_real_buffer *result);

	wl_task_initializer
	void sampler_loop_initializer(
		const s_task_function_context &context,
		wl_source("name") const char *name,
		wl_source("channel") real32 channel,
		wl_source("bidi") bool bidi,
		wl_source("phase") const c_real_buffer *phase); // We can use this if it's a constant

	wl_task_function(0xdf906f59, "sampler_loop", "sampler_loop")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_loop_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_loop(
		const s_task_function_context &context,
		wl_source("name") const char *name,
		wl_source("speed") const c_real_buffer *speed,
		wl_source("phase") const c_real_buffer *phase,
		wl_source("result") c_real_buffer *result);

	wl_task_initializer
	void sampler_wavetable_initializer(
		const s_task_function_context &context,
		wl_source("harmonic_weights") c_real_constant_array harmonic_weights,
		wl_source("phase") const c_real_buffer *phase); // We can use this if it's a constant

	wl_task_function(0x7429e1e3, "sampler_wavetable", "sampler_wavetable")
	wl_task_memory_query_function("sampler_memory_query")
	wl_task_initializer_function("sampler_wavetable_initializer")
	wl_task_voice_initializer_function("sampler_voice_initializer")
	void sampler_wavetable(
		const s_task_function_context &context,
		wl_source("frequency") const c_real_buffer *frequency,
		wl_source("phase") const c_real_buffer *phase,
		wl_source("result") c_real_buffer *result);

}
