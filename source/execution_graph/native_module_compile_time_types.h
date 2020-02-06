#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_COMPILE_TIME_TYPES_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_COMPILE_TIME_TYPES_H__

#include "common/common.h"

#include "execution_graph/node_reference.h"

#include <string>
#include <vector>

// String type used in native module compile-time functions
class c_native_module_string {
public:
	// $TODO make a real interface for this
	std::string &get_string();
	const std::string &get_string() const;
private:
	std::string m_string;
};

// Array type used in native module compile-time functions
// This refers to the array object itself (i.e. a list of references to other nodes), not the dereferenced values
class c_native_module_array {
public:
	// $TODO make a real interface for this
	std::vector<c_node_reference> &get_array();
	const std::vector<c_node_reference> &get_array() const;

private:
	// Vector of node indices in the execution graph
	std::vector<c_node_reference> m_array;
};

class c_native_module_real_array : public c_native_module_array {};
class c_native_module_bool_array : public c_native_module_array {};
class c_native_module_string_array : public c_native_module_array {};

// We directly cast between these types so they must be identical!
static_assert(sizeof(c_native_module_real_array) == sizeof(c_native_module_array), "Invalid array sizeof");
static_assert(sizeof(c_native_module_bool_array) == sizeof(c_native_module_array), "Invalid array sizeof");
static_assert(sizeof(c_native_module_string_array) == sizeof(c_native_module_array), "Invalid array sizeof");

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULE_COMPILE_TIME_TYPES_H__
