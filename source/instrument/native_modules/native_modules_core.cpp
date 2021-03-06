#include "instrument/native_module_registration.h"

#include <algorithm>
#include <cmath>

static void validate_latency(const s_native_module_context &context, real32 latency) {
	if (latency < 0.0f || std::isnan(latency) || std::isinf(latency) || static_cast<int32>(latency) < 0) {
		context.diagnostic_interface->error("Invalid latency value '%f'", latency);
	}
}

namespace core_native_modules {

	void noop_real(
		wl_argument(in ref const? real, a),
		wl_argument(return out ref const? real, result)) {
		*result = *a;
	}

	void noop_bool(
		wl_argument(in ref const? bool, a),
		wl_argument(return out ref const? bool, result)) {
		*result = *a;
	}

	void noop_string(
		wl_argument(in ref const string, a),
		wl_argument(return out ref const string, result)) {
		*result = *a;
	}

	void negation(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = -*a;
	}

	void addition(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = *a + *b;
	}

	void subtraction(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = *a - *b;
	}

	void multiplication(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = *a * *b;
	}

	void division(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = *a / *b;
	}

	void modulo(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = std::fmod(*a, *b);
	}

	void concatenation(
		wl_argument(in const string, a),
		wl_argument(in const string, b),
		wl_argument(return out const string, result)) {
		result->get_string() = a->get_string() + b->get_string();
	}

	void not_(
		wl_argument(in const? bool, a),
		wl_argument(return out const? bool, result)) {
		*result = !*a;
	}

	void equal_real(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a == *b);
	}

	void not_equal_real(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a != *b);
	}

	void equal_bool(
		wl_argument(in const? bool, a),
		wl_argument(in const? bool, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a == *b);
	}

	void not_equal_bool(
		wl_argument(in const? bool, a),
		wl_argument(in const? bool, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a != *b);
	}

	void equal_string(
		wl_argument(in const string, a),
		wl_argument(in const string, b),
		wl_argument(return out const bool, result)) {
		*result = (a->get_string() == b->get_string());
	}

	void not_equal_string(
		wl_argument(in const string, a),
		wl_argument(in const string, b),
		wl_argument(return out const bool, result)) {
		*result = (a->get_string() != b->get_string());
	}

	void greater(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a > *b);
	}

	void less(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a < *b);
	}

	void greater_equal(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a >= *b);
	}

	void less_equal(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a <= *b);
	}

	void and_(
		wl_argument(in const? bool, a),
		wl_argument(in const? bool, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a && *b);
	}

	void or_(
		wl_argument(in const? bool, a),
		wl_argument(in const? bool, b),
		wl_argument(return out const? bool, result)) {
		*result = (*a || *b);
	}

	void select_real(
		wl_argument(in const? bool, condition),
		wl_argument(in const? real, true_value),
		wl_argument(in const? real, false_value),
		wl_argument(return out const? real, result)) {
		*result = *condition ? *true_value : *false_value;
	}

	void select_bool(
		wl_argument(in const? bool, condition),
		wl_argument(in const? bool, true_value),
		wl_argument(in const? bool, false_value),
		wl_argument(return out const? bool, result)) {
		*result = *condition ? *true_value : *false_value;
	}

	void select_string(
		wl_argument(in const bool, condition),
		wl_argument(in const string, true_value),
		wl_argument(in const string, false_value),
		wl_argument(return out const string, result)) {
		result->get_string() = *condition ? true_value->get_string() : false_value->get_string();
	}

	void assert(
		const s_native_module_context &context,
		wl_argument(in const bool, condition)) {
		if (!*condition) {
			context.diagnostic_interface->error("Assertion failed");
		}
	}

	void assert_message(
		const s_native_module_context &context,
		wl_argument(in const bool, condition),
		wl_argument(in const string, message)) {
		if (!*condition) {
			context.diagnostic_interface->error("Assertion failed: %s", message->get_string().c_str());
		}
	}

	void get_latency_real(
		wl_argument(in ref real, value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	void get_latency_bool(
		wl_argument(in ref bool, value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	void get_latency_string(
		wl_argument(in ref const string, value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	void get_latency_real_array(
		wl_argument(in ref real[], value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	void get_latency_bool_array(
		wl_argument(in ref bool[], value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	void get_latency_string_array(
		wl_argument(in ref const string[], value),
		wl_argument(return out const real, latency)) {
		wl_assert("Custom intrinsic implementation - this should not be called");
	}

	// This only relies on the "latency" argument so we can use this implementation for all type variants
	int32 add_latency_get_latency(
		wl_argument(in const real, latency)) {
		return static_cast<int32>(latency);
	}

	void add_latency_real(
		const s_native_module_context &context,
		wl_argument(in ref real, value),
		wl_argument(in const real, latency),
		wl_argument(return out ref real, value_with_latency)) {
		validate_latency(context, latency);
		*value_with_latency = *value;
	}

	void add_latency_bool(
		const s_native_module_context &context,
		wl_argument(in ref bool, value),
		wl_argument(in const real, latency),
		wl_argument(return out ref bool, value_with_latency)) {
		validate_latency(context, latency);
		*value_with_latency = *value;
	}

	void add_latency_real_array(
		const s_native_module_context &context,
		wl_argument(in ref real[], value),
		wl_argument(in const real, latency),
		wl_argument(return out ref real[], value_with_latency)) {
		validate_latency(context, latency);
		*value_with_latency = *value;
	}

	void add_latency_bool_array(
		const s_native_module_context &context,
		wl_argument(in ref bool[], value),
		wl_argument(in const real, latency),
		wl_argument(return out ref bool[], value_with_latency)) {
		validate_latency(context, latency);
		*value_with_latency = *value;
	}

	void scrape_native_modules() {
		static constexpr uint32 k_core_library_id = 0;
		wl_native_module_library(k_core_library_id, "core", 0);

		// Note: "and", "or", and "not" are actually reserved C++ keywords, so we append an underscore to those
		// functions

		wl_native_module(0x50e37f45, "noop$real")
			.set_native_operator(e_native_operator::k_noop)
			.set_compile_time_call<noop_real>();

		wl_native_module(0x31813363, "noop$bool")
			.set_native_operator(e_native_operator::k_noop)
			.set_compile_time_call<noop_bool>();

		wl_native_module(0x243fcbca, "noop$string")
			.set_native_operator(e_native_operator::k_noop)
			.set_compile_time_call<noop_string>();

		wl_native_module(0x3daee7de, "negation")
			.set_native_operator(e_native_operator::k_negation)
			.set_compile_time_call<negation>();

		wl_native_module(0xe2f69812, "addition")
			.set_native_operator(e_native_operator::k_addition)
			.set_compile_time_call<addition>();

		wl_native_module(0xc5ecc7fc, "subtraction")
			.set_native_operator(e_native_operator::k_subtraction)
			.set_compile_time_call<subtraction>();

		wl_native_module(0x83cfe0ae, "multiplication")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<multiplication>();

		wl_native_module(0xe6fa619c, "division")
			.set_native_operator(e_native_operator::k_division)
			.set_compile_time_call<division>();

		wl_native_module(0xa95858a3, "modulo")
			.set_native_operator(e_native_operator::k_modulo)
			.set_compile_time_call<modulo>();

		wl_native_module(0x9e71b219, "concatenation")
			.set_native_operator(e_native_operator::k_addition)
			.set_compile_time_call<concatenation>();

		wl_native_module(0xa0581ec6, "not")
			.set_native_operator(e_native_operator::k_not)
			.set_compile_time_call<not_>();

		wl_native_module(0x1e943a9d, "equal$real")
			.set_native_operator(e_native_operator::k_equal)
			.set_compile_time_call<equal_real>();

		wl_native_module(0xbe67d153, "not_equal$real")
			.set_native_operator(e_native_operator::k_not_equal)
			.set_compile_time_call<not_equal_real>();

		wl_native_module(0x606660f4, "equal$bool")
			.set_native_operator(e_native_operator::k_equal)
			.set_compile_time_call<equal_bool>();

		wl_native_module(0xd8acbb09, "not_equal$bool")
			.set_native_operator(e_native_operator::k_not_equal)
			.set_compile_time_call<not_equal_bool>();

		wl_native_module(0xc33f00a6, "equal$string")
			.set_native_operator(e_native_operator::k_equal)
			.set_compile_time_call<equal_string>();

		wl_native_module(0xcf137e06, "not_equal$string")
			.set_native_operator(e_native_operator::k_not_equal)
			.set_compile_time_call<not_equal_string>();

		wl_native_module(0x383b72fd, "greater")
			.set_native_operator(e_native_operator::k_greater)
			.set_compile_time_call<greater>();

		wl_native_module(0x42e469cf, "less")
			.set_native_operator(e_native_operator::k_less)
			.set_compile_time_call<less>();

		wl_native_module(0x7a3c25af, "greater_equal")
			.set_native_operator(e_native_operator::k_greater_equal)
			.set_compile_time_call<greater_equal>();

		wl_native_module(0x6a09950a, "less_equal")
			.set_native_operator(e_native_operator::k_less_equal)
			.set_compile_time_call<less_equal>();

		wl_native_module(0xaa43870c, "and")
			.set_native_operator(e_native_operator::k_and)
			.set_compile_time_call<and_>();

		wl_native_module(0x371bded7, "or")
			.set_native_operator(e_native_operator::k_or)
			.set_compile_time_call<or_>();

		wl_native_module(0x94ca7bc2, "select$real")
			.set_compile_time_call<select_real>();

		wl_native_module(0xd3880d8c, "select$bool")
			.set_compile_time_call<select_bool>();

		wl_native_module(0x7ea6fc04, "select$string")
			.set_compile_time_call<select_string>();

		wl_native_module(0xd0a71018, "assert")
			.set_compile_time_call<assert>();

		wl_native_module(0xba5e8b11, "assert$message")
			.set_compile_time_call<assert_message>();

		wl_native_module(0xcec22cbd, "get_latency$real")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_real)
			.set_compile_time_call<get_latency_real>();

		wl_native_module(0x3ce48cc7, "get_latency$bool")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_bool)
			.set_compile_time_call<get_latency_bool>();

		wl_native_module(0xf38d3002, "get_latency$string")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_string)
			.set_compile_time_call<get_latency_string>();

		wl_native_module(0x1f52f7ae, "get_latency$real_array")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_real_array)
			.set_compile_time_call<get_latency_real_array>();

		wl_native_module(0xa78878fb, "get_latency$bool_array")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_bool_array)
			.set_compile_time_call<get_latency_bool_array>();

		wl_native_module(0x4ba1fc23, "get_latency$string_array")
			.set_intrinsic(e_native_module_intrinsic::k_get_latency_string_array)
			.set_compile_time_call<get_latency_string_array>();

		wl_native_module(0x2298346f, "add_latency$real")
			.set_compile_time_call<add_latency_real>()
			.set_get_latency<add_latency_get_latency>();

		wl_native_module(0xcc6bc280, "add_latency$bool")
			.set_compile_time_call<add_latency_bool>()
			.set_get_latency<add_latency_get_latency>();

		wl_native_module(0xfee9ad2e, "add_latency$real_array")
			.set_compile_time_call<add_latency_real_array>()
			.set_get_latency<add_latency_get_latency>();

		wl_native_module(0x206f50c1, "add_latency$bool_array")
			.set_compile_time_call<add_latency_bool_array>()
			.set_get_latency<add_latency_get_latency>();

		wl_optimization_rule(negation(negation(x)) -> x);

		wl_optimization_rule(addition(const x, y) -> addition(y, x));
		wl_optimization_rule(addition(x, negation(y)) -> subtraction(x, y));
		wl_optimization_rule(addition(negation(x), y) -> subtraction(y, x));
		wl_optimization_rule(addition(negation(x), const y) -> subtraction(y, x));
		wl_optimization_rule(addition(x, 0) -> x);
		wl_optimization_rule(subtraction(x, const y) -> addition(x, negation(y)));
		wl_optimization_rule(subtraction(const x, negation(y)) -> addition(y, x));
		wl_optimization_rule(subtraction(x, negation(y)) -> addition(x, y));
		wl_optimization_rule(subtraction(0, x) -> negation(x));

		wl_optimization_rule(multiplication(const x, y) -> multiplication(y, x));
		wl_optimization_rule(multiplication(x, 0) -> 0);
		wl_optimization_rule(multiplication(x, 1) -> x);
		wl_optimization_rule(multiplication(x, -1) -> negation(x));
		wl_optimization_rule(division(x, 1) -> x);
		wl_optimization_rule(division(x, -1) -> negation(x));
		wl_optimization_rule(division(negation(x), const y) -> division(x, negation(y)));
		wl_optimization_rule(division(const x, negation(y)) -> division(negation(x), y));

		wl_optimization_rule(not(not(x)) -> x);

		wl_optimization_rule(equal$real(const x, y) -> equal$real(y, x));
		wl_optimization_rule(not(equal$real(x, y)) -> not_equal$real(x, y));
		wl_optimization_rule(not(equal$real(x, const y)) -> not_equal$real(x, y));
		wl_optimization_rule(equal$bool(const x, y) -> equal$bool(y, x));
		wl_optimization_rule(not(equal$bool(x, y)) -> not_equal$bool(x, y));
		wl_optimization_rule(not(equal$bool(x, const y)) -> not_equal$bool(x, y));
		wl_optimization_rule(equal$bool(x, false)  ->  not(x));
		wl_optimization_rule(equal$bool(x, true) -> x);

		wl_optimization_rule(not_equal$real(const x, y) -> not_equal$real(y, x));
		wl_optimization_rule(not(not_equal$real(x, y)) -> equal$real(x, y));
		wl_optimization_rule(not(not_equal$real(x, const y)) -> equal$real(x, y));
		wl_optimization_rule(not_equal$bool(const x, y) -> not_equal$bool(y, x));
		wl_optimization_rule(not(not_equal$bool(x, y)) -> equal$bool(x, y));
		wl_optimization_rule(not(not_equal$bool(x, const y)) -> equal$bool(x, y));
		wl_optimization_rule(not_equal$bool(x, false) -> x);
		wl_optimization_rule(not_equal$bool(x, true)  ->  not(x));

		wl_optimization_rule(greater(const x, y) -> less_equal(y, x));
		wl_optimization_rule(not(greater(x, y)) -> less_equal(x, y));
		wl_optimization_rule(not(greater(x, const y)) -> less_equal(x, y));

		wl_optimization_rule(less(const x, y) -> greater_equal(y, x));
		wl_optimization_rule(not(less(x, y)) -> greater_equal(x, y));
		wl_optimization_rule(not(less(x, const y)) -> greater_equal(x, y));

		wl_optimization_rule(greater_equal(const x, y) -> less(y, x));
		wl_optimization_rule(not(greater_equal(x, y)) -> less(x, y));
		wl_optimization_rule(not(greater_equal(x, const y)) -> less(x, y));

		wl_optimization_rule(less_equal(const x, y) -> greater(y, x));
		wl_optimization_rule(not(less_equal(x, y)) -> greater(x, y));
		wl_optimization_rule(not(less_equal(x, const y)) -> greater(x, y));

		wl_optimization_rule(and(const x, y) -> and (y, x));
		wl_optimization_rule(and(x, false) -> false);
		wl_optimization_rule(and(x, true) -> x);

		wl_optimization_rule(or(const x, y) -> or(y, x));
		wl_optimization_rule(or(x, false) -> x);
		wl_optimization_rule(or(x, true) -> true);

		wl_optimization_rule(select$real(true, x, y) -> x);
		wl_optimization_rule(select$real(false, x, y) -> y);
		wl_optimization_rule(select$real(true, const x, y) -> x);
		wl_optimization_rule(select$real(false, const x, y) -> y);
		wl_optimization_rule(select$real(true, x, const y) -> x);
		wl_optimization_rule(select$real(false, x, const y) -> y);

		wl_optimization_rule(select$bool(true, x, y) -> x);
		wl_optimization_rule(select$bool(false, x, y) -> y);
		wl_optimization_rule(select$bool(true, const x, y) -> x);
		wl_optimization_rule(select$bool(false, const x, y) -> y);
		wl_optimization_rule(select$bool(true, x, const y) -> x);
		wl_optimization_rule(select$bool(false, x, const y) -> y);

		wl_end_active_library_native_module_registration();
	}

}
