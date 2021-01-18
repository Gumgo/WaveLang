#pragma once

#include "common/common.h"

#include "execution_graph/native_module_registry.h"

#include "task_function/task_function.h"

// $TODO $HANDLE add h_task_function and h_task_function_library
static constexpr uint32 k_invalid_task_function_index = static_cast<uint32>(-1);

// The registry of all task functions
class c_task_function_registry {
public:
	static void initialize();
	static void shutdown();

	static void begin_registration();
	static bool end_registration();

	// Registers a library which task functions are grouped under
	static bool register_task_function_library(const s_task_function_library &library);

	static bool is_task_function_library_registered(uint32 library_id);
	static uint32 get_task_function_library_index(uint32 library_id);

	static uint32 get_task_function_library_count();
	static const s_task_function_library &get_task_function_library(uint32 index);

	static bool register_task_function(const s_task_function &task_function);

	static uint32 get_task_function_count();
	static uint32 get_task_function_index(s_task_function_uid task_function_uid);
	static const s_task_function &get_task_function(uint32 index);

	static s_task_function_uid get_task_function_mapping(h_native_module native_module_handle);
};
