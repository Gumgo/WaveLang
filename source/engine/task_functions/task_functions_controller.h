#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_modules/native_modules_controller.h"

#include "task_function/task_function.h"

namespace controller_task_functions wl_library(k_controller_library_id, "controller", 0) {

	wl_task_function(0xbeaf383d, "get_note_id", "get_note_id")
	void get_note_id(
		const s_task_function_context &context,
		wl_source("result") c_real_buffer *result);

	wl_task_function(0x8b9d039b, "get_note_velocity", "get_note_velocity")
	void get_note_velocity(
		const s_task_function_context &context,
		wl_source("result") c_real_buffer *result);

	wl_task_memory_query
	size_t get_note_press_duration_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void get_note_press_duration_voice_initializer(const s_task_function_context &context);

	wl_task_function(0x05b9e818, "get_note_press_duration", "get_note_press_duration")
	wl_task_memory_query_function("get_note_press_duration_memory_query")
	wl_task_voice_initializer_function("get_note_press_duration_voice_initializer")
	void get_note_press_duration(
		const s_task_function_context &context,
		wl_source("result") c_real_buffer *result);

	wl_task_memory_query
	size_t get_note_release_duration_memory_query(const s_task_function_context &context);

	wl_task_voice_initializer
	void get_note_release_duration_voice_initializer(const s_task_function_context &context);

	wl_task_function(0xa370e402, "get_note_release_duration", "get_note_release_duration")
	wl_task_memory_query_function("get_note_release_duration_memory_query")
	wl_task_voice_initializer_function("get_note_release_duration_voice_initializer")
	void get_note_release_duration(
		const s_task_function_context &context,
		wl_source("result") c_real_buffer *result);

	wl_task_initializer
	void get_parameter_value_initializer(
		const s_task_function_context &context,
		wl_source("parameter_id") real32 parameter_id);

	wl_task_function(0x6badd8e8, "get_parameter_value", "get_parameter_value")
	wl_task_initializer_function("get_parameter_value_initializer")
	void get_parameter_value(
		const s_task_function_context &context,
		wl_source("parameter_id") real32 parameter_id,
		wl_source("result") c_real_buffer *result);

}

