#pragma once

#include "common/common.h"

#include "instrument/native_module_registry.h"

#include "task_function/task_function.h"

struct s_task_function_library_handle_identifier {};
using h_task_function_library = c_handle<s_task_function_library_handle_identifier, uint32>;

struct s_task_function_handle_identifier {};
using h_task_function = c_handle<s_task_function_handle_identifier, uint32>;

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
	static h_task_function_library get_task_function_library_handle(uint32 library_id);

	static uint32 get_task_function_library_count();
	static c_index_handle_iterator<h_task_function_library> iterate_task_function_libraries();
	static const s_task_function_library &get_task_function_library(h_task_function_library handle);

	static bool register_task_function(const s_task_function &task_function);

	static uint32 get_task_function_count();
	static c_index_handle_iterator<h_task_function> iterate_task_functions();
	static h_task_function get_task_function_handle(s_task_function_uid task_function_uid);
	static const s_task_function &get_task_function(h_task_function handle);

	static s_task_function_uid get_task_function_mapping(h_native_module native_module_handle);
};
