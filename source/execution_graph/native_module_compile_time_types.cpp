#include "execution_graph/native_module_compile_time_types.h"

std::string &c_native_module_string::get_string() {
	return m_string;
}

const std::string &c_native_module_string::get_string() const {
	return m_string;
}

std::vector<c_node_reference> &c_native_module_array::get_array() {
	return m_array;
}

const std::vector<c_node_reference> &c_native_module_array::get_array() const {
	return m_array;
}
