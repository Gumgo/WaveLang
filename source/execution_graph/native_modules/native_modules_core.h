#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_core_library_id = 0;

namespace core_native_modules wl_library(k_core_library_id, "core", 0) {

	// Note: "and", "or", and "not" are actually reserved C++ keywords, so we append an underscore to those functions

	wl_native_module(0x50e37f45, "noop$real")
	wl_operator(e_native_operator::k_noop)
	void noop_real(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x31813363, "noop$bool")
	wl_operator(e_native_operator::k_noop)
	void noop_bool(wl_in wl_dependent_const bool a, wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x243fcbca, "noop$string")
	wl_operator(e_native_operator::k_noop)
	void noop_string(
		wl_in wl_const const c_native_module_string &a,
		wl_out_return wl_const c_native_module_string &result);

	wl_native_module(0x3daee7de, "negation")
	wl_operator(e_native_operator::k_negation)
	void negation(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xe2f69812, "addition")
	wl_operator(e_native_operator::k_addition)
	void addition(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xc5ecc7fc, "subtraction")
	wl_operator(e_native_operator::k_subtraction)
	void subtraction(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x83cfe0ae, "multiplication")
	wl_operator(e_native_operator::k_multiplication)
	void multiplication(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xe6fa619c, "division")
	wl_operator(e_native_operator::k_division)
	void division(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xa95858a3, "modulo")
	wl_operator(e_native_operator::k_modulo)
	void modulo(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x9e71b219, "concatenation")
	wl_operator(e_native_operator::k_addition)
	void concatenation(
		wl_in wl_const const c_native_module_string &a,
		wl_in wl_const const c_native_module_string &b,
		wl_out_return wl_const c_native_module_string &result);

	wl_native_module(0xa0581ec6, "not")
	wl_operator(e_native_operator::k_not)
	void not_(wl_in wl_dependent_const bool a, wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x1e943a9d, "equal$real")
	wl_operator(e_native_operator::k_equal)
	void equal_real(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0xbe67d153, "not_equal$real")
	wl_operator(e_native_operator::k_not_equal)
	void not_equal_real(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x606660f4, "equal$bool")
	wl_operator(e_native_operator::k_equal)
	void equal_bool(
		wl_in wl_dependent_const bool a,
		wl_in wl_dependent_const bool b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0xd8acbb09, "not_equal$bool")
	wl_operator(e_native_operator::k_not_equal)
	void not_equal_bool(
		wl_in wl_dependent_const bool a,
		wl_in wl_dependent_const bool b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0xc33f00a6, "equal$string")
	wl_operator(e_native_operator::k_equal)
	void equal_string(
		wl_in wl_const const c_native_module_string &a,
		wl_in wl_const const c_native_module_string &b,
		wl_out_return wl_const bool &result);

	wl_native_module(0xcf137e06, "not_equal$string")
	wl_operator(e_native_operator::k_not_equal)
	void not_equal_string(
		wl_in wl_const const c_native_module_string &a,
		wl_in wl_const const c_native_module_string &b,
		wl_out_return wl_const bool &result);

	wl_native_module(0x383b72fd, "greater")
	wl_operator(e_native_operator::k_greater)
	void greater(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x42e469cf, "less")
	wl_operator(e_native_operator::k_less)
	void less(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x7a3c25af, "greater_equal")
	wl_operator(e_native_operator::k_greater_equal)
	void greater_equal(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x6a09950a, "less_equal")
	wl_operator(e_native_operator::k_less_equal)
	void less_equal(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0xaa43870c, "and")
	wl_operator(e_native_operator::k_and)
	void and_(
		wl_in wl_dependent_const bool a,
		wl_in wl_dependent_const bool b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x371bded7, "or")
	wl_operator(e_native_operator::k_or)
	void or_(
		wl_dependent_const wl_in bool a,
		wl_in wl_dependent_const bool b,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x94ca7bc2, "select$real")
	void select_real(
		wl_in wl_dependent_const bool condition,
		wl_in wl_dependent_const real32 true_value,
		wl_in wl_dependent_const real32 false_value,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xd3880d8c, "select$bool")
	void select_bool(
		wl_in wl_dependent_const bool condition,
		wl_in wl_dependent_const bool true_value,
		wl_in wl_dependent_const bool false_value,
		wl_out_return wl_dependent_const bool &result);

	wl_native_module(0x7ea6fc04, "select$string")
	void select_string(
		wl_in wl_const bool condition,
		wl_in wl_const const c_native_module_string &true_value,
		wl_in wl_const const c_native_module_string &false_value,
		wl_out_return wl_const c_native_module_string &result);

	struct

	wl_optimization_rule(noop$real(x)->x)
	wl_optimization_rule(noop$bool(x)->x)
	wl_optimization_rule(noop$string(x)->x)
	wl_optimization_rule(negation(negation(x))->x)

	wl_optimization_rule(addition(const x, y) -> addition(y, x))
	wl_optimization_rule(addition(x, negation(y)) -> subtraction(x, y))
	wl_optimization_rule(addition(negation(x), y) -> subtraction(y, x))
	wl_optimization_rule(addition(negation(x), const y)->subtraction(y, x))
	wl_optimization_rule(addition(x, 0.0f) -> x)
	wl_optimization_rule(subtraction(x, const y) -> addition(x, negation(y)))
	wl_optimization_rule(subtraction(const x, negation(y)) -> addition(y, x))
	wl_optimization_rule(subtraction(x, negation(y)) -> addition(x, y))
	wl_optimization_rule(subtraction(0.0f, x) -> negation(x))

	wl_optimization_rule(multiplication(const x, y) -> multiplication(y, x))
	wl_optimization_rule(multiplication(x, 0.0f) -> 0.0f)
	wl_optimization_rule(multiplication(x, 1.0f) -> x)
	wl_optimization_rule(multiplication(x, -1.0f) -> negation(x))
	wl_optimization_rule(division(x, 1.0f) -> x)
	wl_optimization_rule(division(x, -1.0f) -> negation(x))
	wl_optimization_rule(division(negation(x), const y) -> division(x, negation(y)))
	wl_optimization_rule(division(const x, negation(y)) -> division(negation(x), y))

	wl_optimization_rule(not(not(x)) -> x)

	wl_optimization_rule(equal$real(const x, y) -> equal$real(y, x))
	wl_optimization_rule(not(equal$real(x, y)) -> not_equal$real(x, y))
	wl_optimization_rule(not(equal$real(x, const y)) -> not_equal$real(x, y))
	wl_optimization_rule(equal$bool(const x, y) -> equal$bool(y, x))
	wl_optimization_rule(not(equal$bool(x, y)) -> not_equal$bool(x, y))
	wl_optimization_rule(not(equal$bool(x, const y)) -> not_equal$bool(x, y))
	wl_optimization_rule(equal$bool(x, false) -> not(x))
	wl_optimization_rule(equal$bool(x, true) -> x)

	wl_optimization_rule(not_equal$real(const x, y) -> not_equal$real(y, x))
	wl_optimization_rule(not(not_equal$real(x, y)) -> equal$real(x, y))
	wl_optimization_rule(not(not_equal$real(x, const y)) -> equal$real(x, y))
	wl_optimization_rule(not_equal$bool(const x, y) -> not_equal$bool(y, x))
	wl_optimization_rule(not(not_equal$bool(x, y)) -> equal$bool(x, y))
	wl_optimization_rule(not(not_equal$bool(x, const y)) -> equal$bool(x, y))
	wl_optimization_rule(not_equal$bool(x, false) -> x)
	wl_optimization_rule(not_equal$bool(x, true) -> not(x))

	wl_optimization_rule(greater(const x, y) -> less_equal(y, x))
	wl_optimization_rule(not(greater(x, y)) -> less_equal(x, y))
	wl_optimization_rule(not(greater(x, const y)) -> less_equal(x, y))

	wl_optimization_rule(less(const x, y) -> greater_equal(y, x))
	wl_optimization_rule(not(less(x, y)) -> greater_equal(x, y))
	wl_optimization_rule(not(less(x, const y)) -> greater_equal(x, y))

	wl_optimization_rule(greater_equal(const x, y) -> less(y, x))
	wl_optimization_rule(not(greater_equal(x, y)) -> less(x, y))
	wl_optimization_rule(not(greater_equal(x, const y)) -> less(x, y))

	wl_optimization_rule(less_equal(const x, y) -> greater(y, x))
	wl_optimization_rule(not(less_equal(x, y)) -> greater(x, y))
	wl_optimization_rule(not(less_equal(x, const y)) -> greater(x, y))

	wl_optimization_rule(and(const x, y) -> and(y, x))
	wl_optimization_rule(and(x, false) -> false)
	wl_optimization_rule(and(x, true) -> x)

	wl_optimization_rule(or(const x, y) -> or(y, x))
	wl_optimization_rule(or(x, false) -> x)
	wl_optimization_rule(or(x, true) -> true)

	wl_optimization_rule(select$real(true, x, y) -> x)
	wl_optimization_rule(select$real(false, x, y) -> y)
	wl_optimization_rule(select$real(true, const x, y) -> x)
	wl_optimization_rule(select$real(false, const x, y) -> y)
	wl_optimization_rule(select$real(true, x, const y) -> x)
	wl_optimization_rule(select$real(false, x, const y) -> y)

	wl_optimization_rule(select$bool(true, x, y) -> x)
	wl_optimization_rule(select$bool(false, x, y) -> y)
	wl_optimization_rule(select$bool(true, const x, y) -> x)
	wl_optimization_rule(select$bool(false, const x, y) -> y)
	wl_optimization_rule(select$bool(true, x, const y) -> x)
	wl_optimization_rule(select$bool(false, x, const y) -> y)

	// Strings are always const so no select optimization rules needed

	s_optimization_rules;
}

