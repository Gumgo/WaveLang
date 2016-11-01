#include "execution_graph/native_modules/native_modules_core.h"

#include <algorithm>

namespace core_native_modules {

	void noop_real(real32 a, real32 &result) {
		result = a;
	}

	void noop_bool(bool a, bool &result) {
		result = a;
	}

	void noop_string(const c_native_module_string &a, c_native_module_string &result) {
		result.get_string() = a.get_string();
	}

	void negation(real32 a, real32 &result) {
		result = -a;
	}

	void addition(real32 a, real32 b, real32 &result) {
		result = a + b;
	}

	void subtraction(real32 a, real32 b, real32 &result) {
		result = a - b;
	}

	void multiplication(real32 a, real32 b, real32 &result) {
		result = a * b;
	}

	void division(real32 a, real32 b, real32 &result) {
		result = a / b;
	}

	void modulo(real32 a, real32 b, real32 &result) {
		result = std::fmod(a, b);
	}

	void concatenation(const c_native_module_string &a, const c_native_module_string &b,
		c_native_module_string &result) {
		result.get_string() = a.get_string() + b.get_string();
	}

	void not_(bool a, bool &result) {
		result = !a;
	}

	void equal_real(real32 a, real32 b, bool &result) {
		result = (a == b);
	}

	void not_equal_real(real32 a, real32 b, bool &result) {
		result = (a != b);
	}

	void equal_bool(bool a, bool b, bool &result) {
		result = (a == b);
	}

	void not_equal_bool(bool a, bool b, bool &result) {
		result = (a != b);
	}

	void equal_string(const c_native_module_string &a, const c_native_module_string &b, bool &result) {
		result = (a.get_string() == b.get_string());
	}

	void not_equal_string(const c_native_module_string &a, const c_native_module_string &b, bool &result) {
		result = (a.get_string() != b.get_string());
	}

	void greater(real32 a, real32 b, bool &result) {
		result = (a > b);
	}

	void less(real32 a, real32 b, bool &result) {
		result = (a < b);
	}

	void greater_equal(real32 a, real32 b, bool &result) {
		result = (a >= b);
	}

	void less_equal(real32 a, real32 b, bool &result) {
		result = (a <= b);
	}

	void and_(bool a, bool b, bool &result) {
		result = (a && b);
	}

	void or_(bool a, bool b, bool &result) {
		result = (a || b);
	}

	void select_real(bool condition, real32 true_value, real32 false_value, real32 &result) {
		result = condition ? true_value : false_value;
	}

	void select_bool(bool condition, bool true_value, bool false_value, bool &result) {
		result = condition ? true_value : false_value;
	}

	void select_string(bool condition, const c_native_module_string &true_value,
		const c_native_module_string &false_value, c_native_module_string &result) {
		result.get_string() = condition ? true_value.get_string() : false_value.get_string();
	}

	// __native_enforce_const functions exist so that a compile-time error is produced if a value is not constant.
	// The string version is only there for overload-completeness.

	void enforce_const_real(real32 a, real32 &result) {
		a = result;
	}

	void enforce_const_bool (bool a, bool &result) {
		a = result;
	}

	void enforce_const_string(const c_native_module_string &a, c_native_module_string &result) {
		result.get_string() = a.get_string();
	}

}
