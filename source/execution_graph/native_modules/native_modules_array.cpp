#include "execution_graph/native_modules/native_modules_array.h"

#include <algorithm>
#include <cmath>

template<typename t_element>
static bool array_subscript(
	const s_native_module_context &context,
	const std::vector<t_element> &array,
	real32 array_index_real,
	t_element &element_out) {
	size_t array_index;
	if (!get_and_validate_native_module_array_index(array_index_real, array.size(), array_index)) {
		context.diagnostic_interface->error(
			"Array index '%f' is out of bounds for array of size '%llu'",
			array_index_real,
			array_count);
		return false;
	}

	element_out = array[array_index];
	return true;
}

template<typename t_element>
static std::vector<t_element> array_combine(
	const std::vector<t_element> &array_0,
	const std::vector<t_element> &array_1) {
	std::vector<t_element> result;
	result.resize(array_0.size() + array_1.size());
	std::copy(array_0.begin(), array_0.end(), result.begin());
	std::copy(array_1.begin(), array_1.end(), result.begin() + array_0.size());
	return result;
}

template<typename t_element>
static std::vector<t_element> array_repeat(
	const s_native_module_context &context,
	const std::vector<t_element> &arr,
	real32 real_repeat_count) {
	if (real_repeat_count < 0.0f || std::isnan(real_repeat_count) || std::isinf(real_repeat_count)) {
		context.diagnostic_interface->error(
			"Illegal array repeat count '%f'",
			real_repeat_count);
		return std::vector<t_element>();
	}

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

	void subscript_real(
		const s_native_module_context &context,
		const c_native_module_real_reference_array &a,
		real32 b,
		c_native_module_real_reference &result) {
		if (!array_subscript(context, a.get_array(), b, result)) {
			result = context.reference_interface->create_constant_reference(0.0f);
		}
	}

	void count_real(const c_native_module_real_reference_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_real(
		const c_native_module_real_reference_array &a,
		const c_native_module_real_reference_array &b,
		c_native_module_real_reference_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_real(
		const s_native_module_context &context,
		const c_native_module_real_reference_array &a,
		real32 b,
		c_native_module_real_reference_array &result) {
		result.get_array() = array_repeat(context, a.get_array(), b);
	}

	void repeat_rev_real(
		const s_native_module_context &context,
		real32 a,
		const c_native_module_real_reference_array &b,
		c_native_module_real_reference_array &result) {
		result.get_array() = array_repeat(context, b.get_array(), a);
	}

	void subscript_bool(
		const s_native_module_context &context,
		const c_native_module_bool_reference_array &a,
		real32 b,
		c_native_module_bool_reference &result) {
		if (!array_subscript(context, a.get_array(), b, result)) {
			result = context.reference_interface->create_constant_reference(false);
		}
	}

	void count_bool(const c_native_module_bool_reference_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_bool(
		const c_native_module_bool_reference_array &a,
		const c_native_module_bool_reference_array &b,
		c_native_module_bool_reference_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_bool(
		const s_native_module_context &context,
		const c_native_module_bool_reference_array &a,
		real32 b,
		c_native_module_bool_reference_array &result) {
		result.get_array() = array_repeat(context, a.get_array(), b);
	}

	void repeat_rev_bool(
		const s_native_module_context &context,
		real32 a,
		const c_native_module_bool_reference_array &b,
		c_native_module_bool_reference_array &result) {
		result.get_array() = array_repeat(context, b.get_array(), a);
	}

	void subscript_string(
		const s_native_module_context &context,
		const c_native_module_string_reference_array &a,
		real32 b,
		c_native_module_string_reference &result) {
		if (!array_subscript(context, a.get_array(), b, result)) {
			result = context.reference_interface->create_constant_reference("");
		}
	}

	void count_string(const c_native_module_string_reference_array &a, real32 &result) {
		result = static_cast<real32>(a.get_array().size());
	}

	void combine_string(
		const c_native_module_string_reference_array &a,
		const c_native_module_string_reference_array &b,
		c_native_module_string_reference_array &result) {
		result.get_array() = array_combine(a.get_array(), b.get_array());
	}

	void repeat_string(
		const s_native_module_context &context,
		const c_native_module_string_reference_array &a,
		real32 b,
		c_native_module_string_reference_array &result) {
		result.get_array() = array_repeat(context, a.get_array(), b);
	}

	void repeat_rev_string(
		const s_native_module_context &context,
		real32 a,
		const c_native_module_string_reference_array &b,
		c_native_module_string_reference_array &result) {
		result.get_array() = array_repeat(context, b.get_array(), a);
	}

}