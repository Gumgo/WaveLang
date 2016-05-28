#ifndef WAVELANG_NATIVE_MODULE_REGISTRY_H__
#define WAVELANG_NATIVE_MODULE_REGISTRY_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

static const uint32 k_invalid_native_module_index = static_cast<uint32>(-1);

// Fixed list of operators which are to be associated with native modules
enum e_native_operator {
	k_native_operator_noop, // Special case "no-op" operator
	k_native_operator_negation,
	k_native_operator_addition,
	k_native_operator_subtraction,
	k_native_operator_multiplication,
	k_native_operator_division,
	k_native_operator_modulo,
	k_native_operator_concatenation,
	k_native_operator_not,
	k_native_operator_equal,
	k_native_operator_not_equal,
	k_native_operator_less,
	k_native_operator_greater,
	k_native_operator_less_equal,
	k_native_operator_greater_equal,
	k_native_operator_and,
	k_native_operator_or,
	k_native_operator_array_dereference,

	k_native_operator_count
};

// The registry of all native modules, which are the built-in modules of the language
class c_native_module_registry {
public:
	static void initialize();
	static void shutdown();

	static void begin_registration(bool optimizations_enabled);
	static bool end_registration();

	// Registers a library which native modules are grouped under
	static bool register_native_module_library(uint32 library_id, const char *library_name);

	// Registers a native module - returns whether successful
	static bool register_native_module(const s_native_module &native_module);

	// Registers a native operator by associating it with the name of a native module. We use the name and not UID
	// because the operator can be overloaded.
	static void register_native_operator(e_native_operator native_operator, const char *native_module_name);

	// Registers an optimization rule
	static void register_optimization_rule(const s_native_module_optimization_rule &optimization_rule);

	static uint32 get_native_module_count();
	static uint32 get_native_module_index(s_native_module_uid native_module_uid);
	static const s_native_module &get_native_module(uint32 index);
	static const char *get_native_module_for_native_operator(e_native_operator native_operator);

	static uint32 get_optimization_rule_count();
	static const s_native_module_optimization_rule &get_optimization_rule(uint32 index);

	static bool output_registered_native_modules(const char *filename);
};

#endif // WAVELANG_NATIVE_MODULE_REGISTRY_H__