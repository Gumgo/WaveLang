#pragma once

#include "common/macros.h"

#ifdef SCRAPER_ENABLED
#define SCRAPER_ATTRIBUTE(x) __attribute__((annotate(x)))
#else // SCRAPER_ENABLED
#define SCRAPER_ATTRIBUTE(x)
#endif // SCRAPER_ENABLED

// Convention:
// If an argument is expected, use _PREFIX suffix and end the annotation with ':'
// Otherwise, end the annotation with '.'
#define WL_LIBRARY_DECLARATION "wl_library_declaration."
#define WL_LIBRARY_COMPILER_INITIALIZER "wl_library_compiler_initializer."
#define WL_LIBRARY_COMPILER_DEINITIALIZER "wl_library_compiler_deinitializer."
#define WL_LIBRARY_ENGINE_INITIALIZER "wl_library_engine_initializer."
#define WL_LIBRARY_ENGINE_DEINITIALIZER "wl_library_engine_deinitializer."
#define WL_LIBRARY_TASKS_PRE_INITIALIZER "wl_library_tasks_pre_initializer."
#define WL_LIBRARY_TASKS_POST_INITIALIZER "wl_library_tasks_post_initializer."
#define WL_NATIVE_MODULE_DECLARATION "wl_native_module_declaration."
#define WL_ID_PREFIX "wl_id:"
#define WL_NAME_PREFIX "wl_name:"
#define WL_VERSION_PREFIX "wl_version."
#define WL_OPERATOR_PREFIX "wl_operator:"
#define WL_RUNTIME_ONLY "wl_runtime_only."
#define WL_IN "wl_in."
#define WL_OUT "wl_out."
#define WL_IN_CONST "wl_in_const."
#define WL_OUT_RETURN "wl_out_return."
#define WL_OPTIMIZATION_RULE_PREFIX "wl_optimization_rule:"
#define WL_TASK_FUNCTION_DECLARATION "wl_task_function_declaration."
#define WL_SOURCE_PREFIX "wl_source:"
#define WL_UNSHARED "wl_unshared."
#define WL_TASK_MEMORY_QUERY "wl_task_memory_query."
#define WL_TASK_MEMORY_QUERY_FUNCTION_PREFIX "wl_task_memory_query_function:"
#define WL_TASK_INITIALIZER "wl_task_initializer."
#define WL_TASK_INITIALIZER_FUNCTION_PREFIX "wl_task_initializer_function:"
#define WL_TASK_VOICE_INITIALIZER "wl_task_voice_initializer."
#define WL_TASK_VOICE_INITIALIZER_FUNCTION_PREFIX "wl_task_voice_initializer_function:"

// Declares a library
#define wl_library_declaration SCRAPER_ATTRIBUTE(WL_LIBRARY_DECLARATION)

// Declares a function used to initialize the library for the compiler. This function should return a pointer to the
// initialized library context.
#define wl_library_compiler_initializer SCRAPER_ATTRIBUTE(WL_LIBRARY_COMPILER_INITIALIZER)

// Declares a function used to deinitialize the library for the compiler. This function receives a pointer to the
// initialized library context.
#define wl_library_compiler_deinitializer SCRAPER_ATTRIBUTE(WL_LIBRARY_COMPILER_DEINITIALIZER)

// Declares a function used to initialize the library for the engine. This function should return a pointer to
// initialized library context.
#define wl_library_engine_initializer SCRAPER_ATTRIBUTE(WL_LIBRARY_ENGINE_INITIALIZER)

// Declares a function used to deinitialize the library for the engine. This function receives a pointer to the
// initialized library context.
#define wl_library_engine_deinitializer SCRAPER_ATTRIBUTE(WL_LIBRARY_ENGINE_DEINITIALIZER)

// Declares a function which is called directly before tasks are initialized. This function receives a pointer to the
// initialized library context.
#define wl_library_tasks_pre_initializer SCRAPER_ATTRIBUTE(WL_LIBRARY_TASKS_PRE_INITIALIZER)

// Declares a function which is called directly after tasks are initialized. This function receives a pointer to the
// initialized library context.
#define wl_library_tasks_post_initializer SCRAPER_ATTRIBUTE(WL_LIBRARY_TASKS_POST_INITIALIZER)

// Defines a native module
#define wl_native_module_declaration SCRAPER_ATTRIBUTE(WL_NATIVE_MODULE_DECLARATION)

// Assigns an ID to a library, native module, or task function
#define wl_id(id) SCRAPER_ATTRIBUTE(WL_ID_PREFIX #id)

// Assigns a script-visible name to a library, native module, or task function. Optionally, for native modules, this can
// take the form "name$overload_identifier". Only the "name" portion will be stored, but the full identifier can be used
// to uniquely identify this overload in optimization rules.
#define wl_name(name) SCRAPER_ATTRIBUTE(WL_NAME_PREFIX name)

// Assigns a version number to a library
#define wl_version(version) SCRAPER_ATTRIBUTE(WL_VERSION_PREFIX #version)

// Helper macro which covers library declaration, ID, name, and version
#define wl_library(id, name, version)	\
	wl_library_declaration				\
	wl_id(id)							\
	wl_name(name)						\
	wl_version(version)

// Helper macro which covers native module declaration, ID, and name
#define wl_native_module(id, name)	\
	wl_native_module_declaration	\
	wl_id(id)						\
	wl_name(name)

// Associates an operator with the native module. A name should also be provided but it is only used for identification
// within optimization rules.
#define wl_operator(operator_type) SCRAPER_ATTRIBUTE(WL_OPERATOR_PREFIX #operator_type)

// Indicates that the native module does not have an associated compile-time call and must always be executed at
// runtime, e.g. a waveform generator cannot be optimized away even if provided with constant arguments
#define wl_runtime_only SCRAPER_ATTRIBUTE(WL_RUNTIME_ONLY)

// An input parameter
#define wl_in SCRAPER_ATTRIBUTE(WL_IN)

// An output parameter
#define wl_out SCRAPER_ATTRIBUTE(WL_OUT)

// An input parameter which must resolve to a constant at compile-time
#define wl_in_const SCRAPER_ATTRIBUTE(WL_IN_CONST)

// An output parameter which is used as the return value in script and in optimization rules
#define wl_out_return SCRAPER_ATTRIBUTE(WL_OUT_RETURN)

// Defines an optimization rule using the following syntax:
// x -> y
// meaning that if the expression "x" is identified, it will be replaced with "y" during optimization. Expressions can
// be built using the following pieces:
// - module_name$overload_identifier(x, y, z, ...) - Represents a native module call, where x, y, z, ... are the
//   arguments. Optional syntax: library_name.module_name$overload_identifier. Example: addition$real(x, y)
// - variable[index] - Represents an array subscript operation. Example: x[0]
// - identifier - Used to represent a variable to be matched between the pre- and post-optimization expression.
// - const identifier - Same as above, but the variable is only matched if it is compile-time constant.
// - <real value> - matches with a real value.
// - true, false - matches with a boolean value.
#define wl_optimization_rule(rule) SCRAPER_ATTRIBUTE(WL_OPTIMIZATION_RULE_PREFIX #rule)

// Defines a task function
#define wl_task_function_declaration SCRAPER_ATTRIBUTE(WL_TASK_FUNCTION_DECLARATION)

// Associates a task function or task function argument with a native module or native module argument source
#define wl_source(source) SCRAPER_ATTRIBUTE(WL_SOURCE_PREFIX source)

// Helper macro which covers task function declaration, ID, name, and native module source
#define wl_task_function(id, name, source)	\
	wl_task_function_declaration			\
	wl_id(id)								\
	wl_name(name)							\
	wl_source(source)

// Indicates that the underlying buffer for a task function argument cannot be shared with another argument
#define wl_unshared SCRAPER_ATTRIBUTE(WL_UNSHARED)

// Declares a task memory query function
#define wl_task_memory_query SCRAPER_ATTRIBUTE(WL_TASK_MEMORY_QUERY)

// Associates the provided task memory query function with the task function being declared
#define wl_task_memory_query_function(func) SCRAPER_ATTRIBUTE(WL_TASK_MEMORY_QUERY_FUNCTION_PREFIX func)

// Declares a task initializer function
#define wl_task_initializer SCRAPER_ATTRIBUTE(WL_TASK_INITIALIZER)

// Associates the provided task initializer function with the task function being declared
#define wl_task_initializer_function(func) SCRAPER_ATTRIBUTE(WL_TASK_INITIALIZER_FUNCTION_PREFIX func)

// Declares a task voice initializer function
#define wl_task_voice_initializer SCRAPER_ATTRIBUTE(WL_TASK_VOICE_INITIALIZER)

// Associates the provided task voice initializer function with the task function being declared
#define wl_task_voice_initializer_function(func) SCRAPER_ATTRIBUTE(WL_TASK_VOICE_INITIALIZER_FUNCTION_PREFIX func)

