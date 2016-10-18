#include "execution_graph/native_modules/native_modules_basic.h"

#include <algorithm>

static uint32 g_next_module_id = 0;

const s_native_module_uid k_native_module_noop_real_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_noop_real(real32 a, real32 &result) {
	result = a;
}

const s_native_module_uid k_native_module_noop_bool_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_noop_bool(bool a, bool &result) {
	result = a;
}

const s_native_module_uid k_native_module_noop_string_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_noop_string(const c_native_module_string &a, c_native_module_string &result) {
	result.get_string() = a.get_string();
}

const s_native_module_uid k_native_module_negation_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_negation(real32 a, real32 &result) {
	result = -a;
}

const s_native_module_uid k_native_module_addition_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_addition(real32 a, real32 b, real32 &result) {
	result = a + b;
}

const s_native_module_uid k_native_module_subtraction_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_subtraction(real32 a, real32 b, real32 &result) {
	result = a - b;
}

const s_native_module_uid k_native_module_multiplication_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_multiplication(real32 a, real32 b, real32 &result) {
	result = a * b;
}

const s_native_module_uid k_native_module_division_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_division(real32 a, real32 b, real32 &result) {
	result = a / b;
}

const s_native_module_uid k_native_module_modulo_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_modulo(real32 a, real32 b, real32 &result) {
	result = std::fmod(a, b);
}

const s_native_module_uid k_native_module_concatenation_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_concatenation(const c_native_module_string &a, const c_native_module_string &b,
	c_native_module_string &result) {
	result.get_string() = a.get_string() + b.get_string();
}

const s_native_module_uid k_native_module_not_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_not(bool a, bool &result) {
	result = !a;
}

const s_native_module_uid k_native_module_real_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_equal(real32 a, real32 b, bool &result) {
	result = (a == b);
}

const s_native_module_uid k_native_module_real_not_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_not_equal(real32 a, real32 b, bool &result) {
	result = (a != b);
}

const s_native_module_uid k_native_module_bool_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_equal(bool a, bool b, bool &result) {
	result = (a == b);
}

const s_native_module_uid k_native_module_bool_not_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_not_equal(bool a, bool b, bool &result) {
	result = (a != b);
}

const s_native_module_uid k_native_module_string_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_equal(const c_native_module_string &a, const c_native_module_string &b, bool &result) {
	result = (a.get_string() == b.get_string());
}

const s_native_module_uid k_native_module_string_not_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_not_equal(const c_native_module_string &a, const c_native_module_string &b, bool &result) {
	result = (a.get_string() != b.get_string());
}

const s_native_module_uid k_native_module_greater_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_greater(real32 a, real32 b, bool &result) {
	result = (a > b);
}

const s_native_module_uid k_native_module_less_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_less(real32 a, real32 b, bool &result) {
	result = (a < b);
}

const s_native_module_uid k_native_module_greater_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_greater_equal(real32 a, real32 b, bool &result) {
	result = (a >= b);
}

const s_native_module_uid k_native_module_less_equal_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_less_equal(real32 a, real32 b, bool &result) {
	result = (a <= b);
}

const s_native_module_uid k_native_module_and_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_and(bool a, bool b, bool &result) {
	result = (a && b);
}

const s_native_module_uid k_native_module_or_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_or(bool a, bool b, bool &result) {
	result = (a || b);
}

const s_native_module_uid k_native_module_real_select_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_select(bool condition, real32 true_value, real32 false_value, real32 &result) {
	result = condition ? true_value : false_value;
}

const s_native_module_uid k_native_module_bool_select_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_select(bool condition, bool true_value, bool false_value, bool &result) {
	result = condition ? true_value : false_value;
}

const s_native_module_uid k_native_module_string_select_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_select(bool condition, const c_native_module_string &true_value,
	const c_native_module_string &false_value, c_native_module_string &result) {
	result.get_string() = condition ? true_value.get_string() : false_value.get_string();
}

static std::vector<uint32> array_combine(const std::vector<uint32> &array_0, const std::vector<uint32> &array_1) {
	std::vector<uint32> result;
	result.resize(array_0.size() + array_1.size());
	std::copy(array_0.begin(), array_0.end(), result.begin());
	std::copy(array_1.begin(), array_1.end(), result.begin() + array_0.size());
	return result;
}

static std::vector<uint32> array_repeat(const std::vector<uint32> &arr, real32 real_repeat_count) {
	// Floor the value automatically in the cast
	int32 repeat_count_signed = std::max(0, static_cast<int32>(real_repeat_count));
	size_t repeat_count = cast_integer_verify<size_t>(repeat_count_signed);
	std::vector<uint32> result;
	result.resize(arr.size() * repeat_count);
	for (size_t rep = 0; rep < repeat_count; rep++) {
		std::copy(arr.begin(), arr.end(), result.begin() + (rep * arr.size()));
	}

	return result;
}

const s_native_module_uid k_native_module_real_array_dereference_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_array_dereference(const c_native_module_real_array &a, real32 b, real32 &result) {
	wl_vhalt("This module call should always be optimized away");
}

const s_native_module_uid k_native_module_real_array_count_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_array_count(const c_native_module_real_array &a, real32 &result) {
	result = static_cast<real32>(a.get_array().size());
}

const s_native_module_uid k_native_module_real_array_combine_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_array_combine(const c_native_module_real_array &a, const c_native_module_real_array &b,
	c_native_module_real_array &result) {
	result.get_array() = array_combine(a.get_array(), b.get_array());
}

const s_native_module_uid k_native_module_real_array_repeat_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_array_repeat(const c_native_module_real_array &a, real32 b,
	c_native_module_real_array &result) {
	result.get_array() = array_repeat(a.get_array(), b);
}

const s_native_module_uid k_native_module_real_array_repeat_rev_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_array_repeat_rev(real32 a, const c_native_module_real_array &b,
	c_native_module_real_array &result) {
	result.get_array() = array_repeat(b.get_array(), a);
}

const s_native_module_uid k_native_module_bool_array_dereference_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_array_dereference(const c_native_module_bool_array &a, real32 b, bool &result) {
	wl_vhalt("This module call should always be optimized away");
}

const s_native_module_uid k_native_module_bool_array_count_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_array_count(const c_native_module_bool_array &a, real32 &result) {
	result = static_cast<real32>(a.get_array().size());
}

const s_native_module_uid k_native_module_bool_array_combine_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_array_combine(const c_native_module_bool_array &a, const c_native_module_bool_array &b,
	c_native_module_bool_array &result) {
	result.get_array() = array_combine(a.get_array(), b.get_array());
}

const s_native_module_uid k_native_module_bool_array_repeat_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_array_repeat(const c_native_module_bool_array &a, real32 b,
	c_native_module_bool_array &result) {
	result.get_array() = array_repeat(a.get_array(), b);
}

const s_native_module_uid k_native_module_bool_array_repeat_rev_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_array_repeat_rev(real32 a, const c_native_module_bool_array &b,
	c_native_module_bool_array &result) {
	result.get_array() = array_repeat(b.get_array(), a);
}

const s_native_module_uid k_native_module_string_array_dereference_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_array_dereference(const c_native_module_string_array &a, real32 b,
	c_native_module_string &result) {
	wl_vhalt("This module call should always be optimized away");
}

const s_native_module_uid k_native_module_string_array_count_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_array_count(const c_native_module_string_array &a, real32 &result) {
	result = static_cast<real32>(a.get_array().size());
}

const s_native_module_uid k_native_module_string_array_combine_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_array_combine(const c_native_module_string_array &a, const c_native_module_string_array &b,
	c_native_module_string_array &result) {
	result.get_array() = array_combine(a.get_array(), b.get_array());
}

const s_native_module_uid k_native_module_string_array_repeat_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_array_repeat(const c_native_module_string_array &a, real32 b,
	c_native_module_string_array &result) {
	result.get_array() = array_repeat(a.get_array(), b);
}

const s_native_module_uid k_native_module_string_array_repeat_rev_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_array_repeat_rev(real32 a, const c_native_module_string_array &b,
	c_native_module_string_array &result) {
	result.get_array() = array_repeat(b.get_array(), a);
}

// __native_enforce_const functions exist so that a compile-time error is produced if a value is not constant.
// The string version is only there for overload-completeness.

const s_native_module_uid k_native_module_real_enforce_const_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_real_enforce_const(real32 a, real32 &result) {
	a = result;
}

const s_native_module_uid k_native_module_bool_enforce_const_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_bool_enforce_const(bool a, bool &result) {
	a = result;
}

const s_native_module_uid k_native_module_string_enforce_const_uid = s_native_module_uid::build(k_native_modules_basic_library_id, g_next_module_id++);
void native_module_string_enforce_const(const c_native_module_string &a, c_native_module_string &result) {
	result.get_string() = a.get_string();
}
