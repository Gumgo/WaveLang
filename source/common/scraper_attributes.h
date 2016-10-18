#ifndef WAVELANG_COMMON_SCRAPER_ATTRIBUTES_H__
#define WAVELANG_COMMON_SCRAPER_ATTRIBUTES_H__

#include "common/macros.h"

#ifdef SCRAPER_ENABLED
#define SCRAPER_ATTRIBUTE(x) __attribute__((annotate(x)))
#else // SCRAPER_ENABLED
#define SCRAPER_ATTRIBUTE(x)
#endif // SCRAPER_ENABLED

#define SCRAPER_STANDALONE_ATTRIBUTE(x) struct SCRAPER_ATTRIBUTE(x) s_attribute_placeholder

#define WL_LIBRARY_PREFIX "wl_library:"
#define WL_NATIVE_MODULE_DECLARATION "wl_native_module_declaration"
#define WL_UID_PREFIX "wl_uid:"
#define WL_NAME_PREFIX "wl_name:"
#define WL_OPERATOR_PREFIX "wl_operator:"
#define WL_RUNTIME_ONLY "wl_runtime_only"
#define WL_IN "wl_in"
#define WL_OUT "wl_out"
#define WL_IN_CONST "wl_in_const"
#define WL_OUT_RETURN "wl_out_return"
#define WL_OPTIMIZATION_RULE_PREFIX "wl_optimization_rule:"

// Defines a library
#define wl_library(id, name) SCRAPER_STANDALONE_ATTRIBUTE(WL_LIBRARY_PREFIX #id "," name)

// Defines a native module
#define wl_native_module_declaration SCRAPER_ATTRIBUTE(WL_NATIVE_MODULE_DECLARATION)

// Assigns a UID to a native module
#define wl_uid(uid) SCRAPER_ATTRIBUTE(WL_UID_PREFIX #uid)

// Assigns a script-visible name to a native module. Optionally, this can take the form "name$overload_identifier". Only
// the "name" portion will be stored, but the full identifier can be used to uniquely identify this overload in
// optimization rules.
#define wl_name(name) SCRAPER_ATTRIBUTE(WL_NAME_PREFIX name)

// Helper macro which covers native module declaration, UID, and name
#define wl_native_module(uid, name)	\
	wl_native_module_declaration	\
	wl_uid(uid)						\
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
//   arguments. Example: addition$real(x, y)
// - variable[index] - Represents an array dereference operation. Example: x[0]
// - identifier - Used to represent a variable to be matched between the pre- and post-optimization expression.
// - const identifier - Same as above, but the variable is only matched if it is compile-time constant.
// - <real value> - matches with a real value.
// - true, false - matches with a boolean value.
#define wl_optimization_rule(rule) SCRAPER_STANDALONE_ATTRIBUTE(WL_OPTIMIZATION_RULE_PREFIX #rule)

#endif // WAVELANG_COMMON_SCRAPER_ATTRIBUTES_H__
