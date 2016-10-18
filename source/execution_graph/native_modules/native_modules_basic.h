#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_BASIC_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_BASIC_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_native_modules_basic_library_id = 0;
wl_library(k_native_modules_basic_library_id, "basic");

extern const s_native_module_uid k_native_module_noop_real_uid;
wl_native_module(k_native_module_noop_real_uid, "noop$real")
wl_operator(k_native_operator_noop)
void native_module_noop_real(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_noop_bool_uid;
wl_native_module(k_native_module_noop_bool_uid, "noop$bool")
wl_operator(k_native_operator_noop)
void native_module_noop_bool(bool wl_in a, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_noop_string_uid;
wl_native_module(k_native_module_noop_string_uid, "noop$string")
wl_operator(k_native_operator_noop)
void native_module_noop_string(const c_native_module_string wl_in_const &a, c_native_module_string wl_out_return &result);

extern const s_native_module_uid k_native_module_negation_uid;
wl_native_module(k_native_module_negation_uid, "negation")
wl_operator(k_native_operator_negation)
void native_module_negation(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_addition_uid;
wl_native_module(k_native_module_addition_uid, "addition")
wl_operator(k_native_operator_addition)
void native_module_addition(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_subtraction_uid;
wl_native_module(k_native_module_subtraction_uid, "subtraction")
wl_operator(k_native_operator_subtraction)
void native_module_subtraction(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_multiplication_uid;
wl_native_module(k_native_module_multiplication_uid, "multiplication")
wl_operator(k_native_operator_multiplication)
void native_module_multiplication(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_division_uid;
wl_native_module(k_native_module_division_uid, "division")
wl_operator(k_native_operator_division)
void native_module_division(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_modulo_uid;
wl_native_module(k_native_module_modulo_uid, "modulo")
wl_operator(k_native_operator_modulo)
void native_module_modulo(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_concatenation_uid;
wl_native_module(k_native_module_concatenation_uid, "concatenation")
wl_operator(k_native_operator_concatenation)
void native_module_concatenation(const c_native_module_string wl_in_const &a,
	const c_native_module_string wl_in_const &b, c_native_module_string wl_out_return &result);

extern const s_native_module_uid k_native_module_not_uid;
wl_native_module(k_native_module_not_uid, "not")
wl_operator(k_native_operator_not)
void native_module_not(bool wl_in a, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_real_equal_uid;
wl_native_module(k_native_module_real_equal_uid, "equal$real")
wl_operator(k_native_operator_equal)
void native_module_real_equal(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_real_not_equal_uid;
wl_native_module(k_native_module_real_not_equal_uid, "not_equal$real")
wl_operator(k_native_operator_not_equal)
void native_module_real_not_equal(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_equal_uid;
wl_native_module(k_native_module_bool_equal_uid, "equal$bool")
wl_operator(k_native_operator_equal)
void native_module_bool_equal(bool wl_in a, bool wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_not_equal_uid;
wl_native_module(k_native_module_bool_not_equal_uid, "not_equal$bool")
wl_operator(k_native_operator_not_equal)
void native_module_bool_not_equal(bool wl_in a, bool wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_string_equal_uid;
wl_native_module(k_native_module_string_equal_uid, "equal$string")
wl_operator(k_native_operator_equal)
void native_module_string_equal(const c_native_module_string wl_in_const &a,
	const c_native_module_string wl_in_const &b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_string_not_equal_uid;
wl_native_module(k_native_module_string_not_equal_uid, "not_equal$string")
wl_operator(k_native_operator_not_equal)
void native_module_string_not_equal(const c_native_module_string wl_in_const &a,
	const c_native_module_string wl_in_const &b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_greater_uid;
wl_native_module(k_native_module_greater_uid, "greater")
wl_operator(k_native_operator_greater)
void native_module_greater(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_less_uid;
wl_native_module(k_native_module_less_uid, "less")
wl_operator(k_native_operator_less)
void native_module_less(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_greater_equal_uid;
wl_native_module(k_native_module_greater_equal_uid, "greater_equal")
wl_operator(k_native_operator_greater_equal)
void native_module_greater_equal(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_less_equal_uid;
wl_native_module(k_native_module_less_equal_uid, "less_equal")
wl_operator(k_native_operator_less_equal)
void native_module_less_equal(real32 wl_in a, real32 wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_and_uid;
wl_native_module(k_native_module_and_uid, "and")
wl_operator(k_native_operator_and)
void native_module_and(bool wl_in a, bool wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_or_uid;
wl_native_module(k_native_module_or_uid, "or")
wl_operator(k_native_operator_or)
void native_module_or(bool wl_in a, bool wl_in b, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_real_select_uid;
wl_native_module(k_native_module_real_select_uid, "select$real")
void native_module_real_select(bool wl_in condition, real32 wl_in true_value, real32 wl_in false_value,
	real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_select_uid;
wl_native_module(k_native_module_bool_select_uid, "select$bool")
void native_module_bool_select(bool wl_in condition, bool wl_in true_value, bool wl_in false_value,
	bool wl_out_return &result);

extern const s_native_module_uid k_native_module_string_select_uid;
wl_native_module(k_native_module_string_select_uid, "select$string")
void native_module_string_select(bool wl_in condition, const c_native_module_string wl_in_const &true_value,
	const c_native_module_string wl_in_const &false_value, c_native_module_string wl_out_return &result);

extern const s_native_module_uid k_native_module_real_array_dereference_uid;
wl_native_module(k_native_module_real_array_dereference_uid, "array_dereference$real")
wl_operator(k_native_operator_array_dereference)
void native_module_real_array_dereference(const c_native_module_real_array wl_in_const &a, real32 wl_in b,
	real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_real_array_count_uid;
wl_native_module(k_native_module_real_array_count_uid, "array_count$real")
void native_module_real_array_count(const c_native_module_real_array wl_in_const &a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_real_array_combine_uid;
wl_native_module(k_native_module_real_array_combine_uid, "array_combine$real")
wl_operator(k_native_operator_addition)
void native_module_real_array_combine(const c_native_module_real_array wl_in_const &a,
	const c_native_module_real_array wl_in_const &b, c_native_module_real_array wl_out_return &result);

extern const s_native_module_uid k_native_module_real_array_repeat_uid;
wl_native_module(k_native_module_real_array_repeat_uid, "array_repeat$real")
wl_operator(k_native_operator_multiplication)
void native_module_real_array_repeat(const c_native_module_real_array wl_in_const &a, real32 wl_in_const b,
	c_native_module_real_array wl_out_return &result);

extern const s_native_module_uid k_native_module_real_array_repeat_rev_uid;
wl_native_module(k_native_module_real_array_repeat_rev_uid, "array_repeat_rev$real")
wl_operator(k_native_operator_multiplication)
void native_module_real_array_repeat_rev(real32 wl_in_const a, const c_native_module_real_array wl_in_const &b,
	c_native_module_real_array wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_array_dereference_uid;
wl_native_module(k_native_module_bool_array_dereference_uid, "array_dereference$bool")
wl_operator(k_native_operator_array_dereference)
void native_module_bool_array_dereference(const c_native_module_bool_array wl_in_const &a, real32 wl_in b,
	bool wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_array_count_uid;
wl_native_module(k_native_module_bool_array_count_uid, "array_count$bool")
void native_module_bool_array_count(const c_native_module_bool_array wl_in_const &a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_array_combine_uid;
wl_native_module(k_native_module_bool_array_combine_uid, "array_combine$bool")
wl_operator(k_native_operator_addition)
void native_module_bool_array_combine(const c_native_module_bool_array wl_in_const &a,
	const c_native_module_bool_array wl_in_const &b, c_native_module_bool_array wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_array_repeat_uid;
wl_native_module(k_native_module_bool_array_repeat_uid, "array_repeat$bool")
wl_operator(k_native_operator_multiplication)
void native_module_bool_array_repeat(const c_native_module_bool_array wl_in_const &a, real32 wl_in_const b,
	c_native_module_bool_array wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_array_repeat_rev_uid;
wl_native_module(k_native_module_bool_array_repeat_rev_uid, "array_repeat_rev$bool")
wl_operator(k_native_operator_multiplication)
void native_module_bool_array_repeat_rev(real32 wl_in_const a, const c_native_module_bool_array wl_in_const &b,
	c_native_module_bool_array wl_out_return &result);

extern const s_native_module_uid k_native_module_string_array_dereference_uid;
wl_native_module(k_native_module_string_array_dereference_uid, "array_dereference$string")
void native_module_string_array_dereference(const c_native_module_string_array wl_in_const &a, real32 wl_in b,
	c_native_module_string wl_out_return &result);

extern const s_native_module_uid k_native_module_string_array_count_uid;
wl_native_module(k_native_module_string_array_count_uid, "array_count$string")
void native_module_string_array_count(const c_native_module_string_array wl_in_const &a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_string_array_combine_uid;
wl_native_module(k_native_module_string_array_combine_uid, "array_combine$string")
wl_operator(k_native_operator_addition)
void native_module_string_array_combine(const c_native_module_string_array wl_in_const &a,
	const c_native_module_string_array wl_in_const &b, c_native_module_string_array wl_out_return &result);

extern const s_native_module_uid k_native_module_string_array_repeat_uid;
wl_native_module(k_native_module_string_array_repeat_uid, "array_repeat$string")
wl_operator(k_native_operator_multiplication)
void native_module_string_array_repeat(const c_native_module_string_array wl_in_const &a, real32 wl_in_const b,
	c_native_module_string_array wl_out_return &result);

extern const s_native_module_uid k_native_module_string_array_repeat_rev_uid;
wl_native_module(k_native_module_string_array_repeat_rev_uid, "array_repeat_rev$string")
wl_operator(k_native_operator_multiplication)
void native_module_string_array_repeat_rev(real32 wl_in_const a, const c_native_module_string_array wl_in_const &b,
	c_native_module_string_array wl_out_return &result);

extern const s_native_module_uid k_native_module_real_enforce_const_uid;
wl_native_module(k_native_module_real_enforce_const_uid, "enforce_const$real")
// $TODO this is broken! optimizer deletes it before it is enforced!
void native_module_real_enforce_const(real32 wl_in_const a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_bool_enforce_const_uid;
wl_native_module(k_native_module_bool_enforce_const_uid, "enforce_const$bool")
// $TODO this is broken! optimizer deletes it before it is enforced!
void native_module_bool_enforce_const(bool wl_in_const a, bool wl_out_return &result);

extern const s_native_module_uid k_native_module_string_enforce_const_uid;
wl_native_module(k_native_module_string_enforce_const_uid, "enforce_const$string")
// $TODO this is broken! optimizer deletes it before it is enforced!
void native_module_string_enforce_const(const c_native_module_string wl_in_const &a,
	c_native_module_string wl_out_return &result);

wl_optimization_rule(noop$real(x) -> x);
wl_optimization_rule(noop$bool(x) -> x);
wl_optimization_rule(noop$string(x) -> x);
wl_optimization_rule(negation(negation(x)) -> x);

wl_optimization_rule(addition(const x, y) -> addition(y, x));
wl_optimization_rule(addition(x, negation(y)) -> subtraction(x, y));
wl_optimization_rule(addition(negation(x), y) -> subtraction(y, x));
wl_optimization_rule(addition(negation(x), const y)->subtraction(y, x));
wl_optimization_rule(addition(x, 0.0f) -> x);
wl_optimization_rule(subtraction(x, const y) -> addition(x, negation(y)));
wl_optimization_rule(subtraction(const x, negation(y)) -> addition(y, x));
wl_optimization_rule(subtraction(x, negation(y)) -> addition(x, y));
wl_optimization_rule(subtraction(0.0f, x) -> negation(x));

wl_optimization_rule(multiplication(const x, y) -> multiplication(y, x));
wl_optimization_rule(multiplication(x, 0.0f) -> 0.0f);
wl_optimization_rule(multiplication(x, 1.0f) -> x);
wl_optimization_rule(multiplication(x, -1.0f) -> negation(x));
wl_optimization_rule(division(x, 1.0f) -> x);
wl_optimization_rule(division(x, -1.0f) -> negation(x));
wl_optimization_rule(division(negation(x), const y) -> division(x, negation(y)));
wl_optimization_rule(division(const x, negation(y)) -> division(negation(x), y));

wl_optimization_rule(not(not(x)) -> x);

wl_optimization_rule(equal$real(const x, y) -> equal$real(y, x));
wl_optimization_rule(not(equal$real(x, y)) -> not_equal$real(x, y));
wl_optimization_rule(not(equal$real(x, const y)) -> not_equal$real(x, y));
wl_optimization_rule(equal$bool(const x, y) -> equal$bool(y, x));
wl_optimization_rule(not(equal$bool(x, y)) -> not_equal$bool(x, y));
wl_optimization_rule(not(equal$bool(x, const y)) -> not_equal$bool(x, y));
wl_optimization_rule(equal$bool(x, false) -> not(x));
wl_optimization_rule(equal$bool(x, true) -> x);

wl_optimization_rule(not_equal$real(const x, y) -> not_equal$real(y, x));
wl_optimization_rule(not(not_equal$real(x, y)) -> equal$real(x, y));
wl_optimization_rule(not(not_equal$real(x, const y)) -> equal$real(x, y));
wl_optimization_rule(not_equal$bool(const x, y) -> not_equal$bool(y, x));
wl_optimization_rule(not(not_equal$bool(x, y)) -> equal$bool(x, y));
wl_optimization_rule(not(not_equal$bool(x, const y)) -> equal$bool(x, y));
wl_optimization_rule(not_equal$bool(x, false) -> x);
wl_optimization_rule(not_equal$bool(x, true) -> not(x));

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

wl_optimization_rule(and(const x, y) -> and(y, x));
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

// Strings are always const so no select optimization rules needed

wl_optimization_rule(array_dereference$real(const x, const y) -> x[y]);
wl_optimization_rule(array_dereference$bool(const x, const y) -> x[y]);
wl_optimization_rule(array_dereference$string(const x, const y) -> x[y]);

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_BASIC_H__
