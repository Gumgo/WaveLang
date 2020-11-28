#include "execution_graph/native_module_compile_time_types.h"

#include <cmath>

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

bool get_and_validate_native_module_array_index(real32 index_real, size_t array_count, size_t &index_out) {
	if (index_real < 0.0f || std::isnan(index_real) || std::isinf(index_real)) {
		return false;
	}

	// We allow fractional indices to be rounded down for convenience
	index_out = static_cast<size_t>(index_real);
	return index_out < array_count;
}
