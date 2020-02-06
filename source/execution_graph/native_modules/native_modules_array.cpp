#include "execution_graph/native_modules/native_modules_array.h"

#include <algorithm>

static std::vector<c_node_reference> array_combine(
	const std::vector<c_node_reference> &array_0, const std::vector<c_node_reference> &array_1) {
	std::vector<c_node_reference> result;
	result.resize(array_0.size() + array_1.size());
	std::copy(array_0.begin(), array_0.end(), result.begin());
	std::copy(array_1.begin(), array_1.end(), result.begin() + array_0.size());
	return result;
}

static std::vector<c_node_reference> array_repeat(const std::vector<c_node_reference> &arr, real32 real_repeat_count) {
	// Floor the value automatically in the cast
	int32 repeat_count_signed = std::max(0, static_cast<int32>(real_repeat_count));
	size_t repeat_count = cast_integer_verify<size_t>(repeat_count_signed);
	std::vector<c_node_reference> result;
	result.resize(arr.size() * repeat_count);
	for (size_t rep = 0; rep < repeat_count; rep++) {
		std::copy(arr.begin(), arr.end(), result.begin() + (rep * arr.size()));
	}

	return result;
}

namespace array_native_modules {

	void dereference_real(const c_native_module_real_array &a, real32 b, real32 &result) {
		wl_vhalt("This module call should always be optimized away");
	}

	void count_real(const c_native_module_real_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_real(const c_native_module_real_array &a, const c_native_module_real_array &b,
		c_native_module_real_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_real(const c_native_module_real_array &a, real32 b, c_native_module_real_array &result) {
		result.get_array() = array_repeat(a.get_array(), b);
	}

	void repeat_rev_real(real32 a, const c_native_module_real_array &b, c_native_module_real_array &result) {
		result.get_array() = array_repeat(b.get_array(), a);
	}

	void dereference_bool(const c_native_module_bool_array &a, real32 b, bool &result) {
		wl_vhalt("This module call should always be optimized away");
	}

	void count_bool(const c_native_module_bool_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_bool(const c_native_module_bool_array &a, const c_native_module_bool_array &b,
		c_native_module_bool_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_bool(const c_native_module_bool_array &a, real32 b, c_native_module_bool_array &result) {
		result.get_array() = array_repeat(a.get_array(), b);
	}

	void repeat_rev_bool(real32 a, const c_native_module_bool_array &b, c_native_module_bool_array &result) {
		result.get_array() = array_repeat(b.get_array(), a);
	}

	void dereference_string(const c_native_module_string_array &a, real32 b, c_native_module_string &result) {
		wl_vhalt("This module call should always be optimized away");
	}

	void count_string(const c_native_module_string_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_string(const c_native_module_string_array &a, const c_native_module_string_array &b,
		c_native_module_string_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_string(const c_native_module_string_array &a, real32 b, c_native_module_string_array &result) {
		result.get_array() = array_repeat(a.get_array(), b);
	}

	void repeat_rev_string(real32 a, const c_native_module_string_array &b, c_native_module_string_array &result) {
		result.get_array() = array_repeat(b.get_array(), a);
	}

}