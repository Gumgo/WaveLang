#pragma once

#include "common/common.h"

#include "engine/task_function.h"

static const uint32 k_invalid_task_function_index = static_cast<uint32>(-1);

// The registry of all task functions
class c_task_function_registry {
public:
	static void initialize();
	static void shutdown();

	static void begin_registration();
	static bool end_registration();

	static bool register_task_function(const s_task_function &task_function);

	static bool register_task_function_mapping_list(
		s_native_module_uid native_module_uid,
		c_task_function_mapping_list task_function_mapping_list);

	static uint32 get_task_function_count();
	static uint32 get_task_function_index(s_task_function_uid task_function_uid);
	static const s_task_function &get_task_function(uint32 index);

	static c_task_function_mapping_list get_task_function_mapping_list(uint32 native_module_index);
};

