#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

#include "native_module/native_module.h"

struct s_native_module_library_handle_identifier {};
using h_native_module_library = c_handle<s_native_module_library_handle_identifier, uint32>;

struct s_native_module_handle_identifier {};
using h_native_module = c_handle<s_native_module_handle_identifier, uint32>;

struct s_native_module_optimization_rule_identifier {};
using h_native_module_optimization_rule = c_handle<s_native_module_optimization_rule_identifier, uint32>;

// The registry of all native modules, which are the built-in modules of the language
class c_native_module_registry {
public:
	static void initialize();
	static void shutdown();

	static void begin_registration();
	static bool end_registration();

	// Registers a library which native modules are grouped under
	static bool register_native_module_library(const s_native_module_library &library);

	static bool is_native_module_library_registered(uint32 library_id);
	static h_native_module_library get_native_module_library_handle(uint32 library_id);

	static uint32 get_native_module_library_count();
	static c_index_handle_iterator<h_native_module_library> iterate_native_module_libraries();
	static const s_native_module_library &get_native_module_library(h_native_module_library handle);

	// Registers a native module - returns whether successful
	static bool register_native_module(const s_native_module &native_module);

	// Associates a registered native module with an intrinsic
	static bool register_native_module_intrinsic(
		e_native_module_intrinsic intrinsic,
		const s_native_module_uid &native_module_uid);

	static s_native_module_uid get_native_module_intrinsic(e_native_module_intrinsic intrinsic);

	// If a native module is registered using a native operator name, it is automatically associated with that operator
	static e_native_operator get_native_module_operator(s_native_module_uid native_module_uid);

	// Registers an optimization rule - name is only used for error reporting
	static bool register_optimization_rule(
		const s_native_module_optimization_rule &optimization_rule,
		const char *name);

	static uint32 get_native_module_count();
	static c_index_handle_iterator<h_native_module> iterate_native_modules();
	static h_native_module get_native_module_handle(s_native_module_uid native_module_uid);
	static const s_native_module &get_native_module(h_native_module handle);

	static uint32 get_optimization_rule_count();
	static c_index_handle_iterator<h_native_module_optimization_rule> iterate_optimization_rules();
	static const s_native_module_optimization_rule &get_optimization_rule(h_native_module_optimization_rule handle);

	static bool output_registered_native_modules(const char *filename);
};
