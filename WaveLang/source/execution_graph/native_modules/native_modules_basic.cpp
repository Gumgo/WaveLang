#include "execution_graph/native_modules/native_modules_basic.h"
#include "execution_graph/native_module_registry.h"

const s_native_module_uid k_native_module_noop_real_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 0);
const s_native_module_uid k_native_module_noop_bool_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 1);
const s_native_module_uid k_native_module_noop_string_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 2);
const s_native_module_uid k_native_module_negation_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 3);
const s_native_module_uid k_native_module_addition_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 4);
const s_native_module_uid k_native_module_subtraction_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 5);
const s_native_module_uid k_native_module_multiplication_uid		= s_native_module_uid::build(k_native_modules_basic_library_id, 6);
const s_native_module_uid k_native_module_division_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 7);
const s_native_module_uid k_native_module_modulo_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 8);
const s_native_module_uid k_native_module_concatenation_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 9);
const s_native_module_uid k_native_module_not_uid					= s_native_module_uid::build(k_native_modules_basic_library_id, 10);
const s_native_module_uid k_native_module_real_equal_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 11);
const s_native_module_uid k_native_module_real_not_equal_uid		= s_native_module_uid::build(k_native_modules_basic_library_id, 12);
const s_native_module_uid k_native_module_bool_equal_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 13);
const s_native_module_uid k_native_module_bool_not_equal_uid		= s_native_module_uid::build(k_native_modules_basic_library_id, 14);
const s_native_module_uid k_native_module_string_equal_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 15);
const s_native_module_uid k_native_module_string_not_equal_uid		= s_native_module_uid::build(k_native_modules_basic_library_id, 16);
const s_native_module_uid k_native_module_greater_uid				= s_native_module_uid::build(k_native_modules_basic_library_id, 17);
const s_native_module_uid k_native_module_less_uid					= s_native_module_uid::build(k_native_modules_basic_library_id, 18);
const s_native_module_uid k_native_module_greater_equal_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 19);
const s_native_module_uid k_native_module_less_equal_uid			= s_native_module_uid::build(k_native_modules_basic_library_id, 20);
const s_native_module_uid k_native_module_and_uid					= s_native_module_uid::build(k_native_modules_basic_library_id, 21);
const s_native_module_uid k_native_module_or_uid					= s_native_module_uid::build(k_native_modules_basic_library_id, 22);
const s_native_module_uid k_native_module_real_static_select_uid	= s_native_module_uid::build(k_native_modules_basic_library_id, 23);
const s_native_module_uid k_native_module_string_static_select_uid	= s_native_module_uid::build(k_native_modules_basic_library_id, 24);

static const char *k_native_operator_names[] = {
	NATIVE_PREFIX "noop",
	NATIVE_PREFIX "negation",
	NATIVE_PREFIX "addition",
	NATIVE_PREFIX "subtraction",
	NATIVE_PREFIX "multiplication",
	NATIVE_PREFIX "division",
	NATIVE_PREFIX "modulo",
	NATIVE_PREFIX "concatenation",
	NATIVE_PREFIX "not",
	NATIVE_PREFIX "equal",
	NATIVE_PREFIX "not_equal",
	NATIVE_PREFIX "less",
	NATIVE_PREFIX "greater",
	NATIVE_PREFIX "less_equal",
	NATIVE_PREFIX "greater_equal",
	NATIVE_PREFIX "and",
	NATIVE_PREFIX "or"
};

static_assert(NUMBEROF(k_native_operator_names) == k_native_operator_count, "Native operator count mismatch");

static void native_module_noop_real(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = arguments[0].get_real();
}

static void native_module_noop_bool(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = arguments[0].get_bool();
}

static void native_module_noop_string(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = arguments[0].get_string();
}

static void native_module_negation(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = -arguments[0].get_real();
}

static void native_module_addition(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() + arguments[1].get_real();
}

static void native_module_subtraction(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() - arguments[1].get_real();
}

static void native_module_multiplication(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() * arguments[1].get_real();
}

static void native_module_division(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() / arguments[1].get_real();
}

static void native_module_modulo(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::fmod(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_concatenation(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() + arguments[1].get_string();
}

static void native_module_not(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = !arguments[0].get_bool();
}

static void native_module_real_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() == arguments[1].get_real();
}

static void native_module_real_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() != arguments[1].get_real();
}

static void native_module_bool_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() == arguments[1].get_bool();
}

static void native_module_bool_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() != arguments[1].get_bool();
}

static void native_module_string_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() == arguments[1].get_string();
}

static void native_module_string_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() != arguments[1].get_string();
}

static void native_module_greater(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() > arguments[1].get_real();
}

static void native_module_less(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() < arguments[1].get_real();
}

static void native_module_greater_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() >= arguments[1].get_real();
}

static void native_module_less_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() <= arguments[1].get_real();
}

static void native_module_and(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() && arguments[1].get_bool();
}

static void native_module_or(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() || arguments[1].get_bool();
}

static void native_module_real_static_select(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 4);
	if (arguments[0].get_bool()) {
		arguments[3] = arguments[1].get_real();
	} else {
		arguments[3] = arguments[2].get_real();
	}
}

static void native_module_string_static_select(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 4);
	if (arguments[0].get_bool()) {
		arguments[3] = arguments[1].get_string();
	} else {
		arguments[3] = arguments[2].get_string();
	}
}

void register_native_modules_basic() {
	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_noop_real_uid, k_native_operator_names[k_native_operator_noop],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(out, real)),
		native_module_noop_real));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_noop_bool_uid, k_native_operator_names[k_native_operator_noop],
		true, s_native_module_argument_list::build(NMA(in, bool), NMA(out, bool)),
		native_module_noop_bool));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_noop_string_uid, k_native_operator_names[k_native_operator_noop],
		true, s_native_module_argument_list::build(NMA(in, string), NMA(out, string)),
		native_module_noop_string));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_negation_uid, k_native_operator_names[k_native_operator_negation],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(out, real)),
		native_module_negation));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_addition_uid, k_native_operator_names[k_native_operator_addition],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_addition));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_subtraction_uid, k_native_operator_names[k_native_operator_subtraction],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_subtraction));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_multiplication_uid, k_native_operator_names[k_native_operator_multiplication],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_multiplication));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_division_uid, k_native_operator_names[k_native_operator_division],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_division));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_modulo_uid, k_native_operator_names[k_native_operator_modulo],
		true, s_native_module_argument_list::build(NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_modulo));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_concatenation_uid, k_native_operator_names[k_native_operator_concatenation],
		true, s_native_module_argument_list::build(NMA(in, string), NMA(in, string), NMA(out, string)),
		native_module_concatenation));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_not_uid, k_native_operator_names[k_native_operator_not],
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(out, bool)),
		native_module_not));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_real_equal_uid, k_native_operator_names[k_native_operator_equal],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_real_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_real_not_equal_uid, k_native_operator_names[k_native_operator_not_equal],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_real_not_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_bool_equal_uid, k_native_operator_names[k_native_operator_equal],
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(constant, bool), NMA(out, bool)),
		native_module_bool_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_bool_not_equal_uid, k_native_operator_names[k_native_operator_not_equal],
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(constant, bool), NMA(out, bool)),
		native_module_bool_not_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_string_equal_uid, k_native_operator_names[k_native_operator_equal],
		true, s_native_module_argument_list::build(NMA(constant, string), NMA(constant, string), NMA(out, bool)),
		native_module_string_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_string_not_equal_uid, k_native_operator_names[k_native_operator_not_equal],
		true, s_native_module_argument_list::build(NMA(constant, string), NMA(constant, string), NMA(out, bool)),
		native_module_string_not_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_greater_uid, k_native_operator_names[k_native_operator_greater],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_greater));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_less_uid, k_native_operator_names[k_native_operator_less],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_less));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_greater_equal_uid, k_native_operator_names[k_native_operator_greater_equal],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_greater_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_less_equal_uid, k_native_operator_names[k_native_operator_less_equal],
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(constant, real), NMA(out, bool)),
		native_module_less_equal));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_and_uid, k_native_operator_names[k_native_operator_and],
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(constant, bool), NMA(out, bool)),
		native_module_and));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_or_uid, k_native_operator_names[k_native_operator_or],
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(constant, bool), NMA(out, bool)),
		native_module_or));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_real_static_select_uid, NATIVE_PREFIX "static_select",
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(in, real), NMA(in, real), NMA(out, real)),
		native_module_real_static_select));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_string_static_select_uid, NATIVE_PREFIX "static_select",
		true, s_native_module_argument_list::build(NMA(constant, bool), NMA(in, string), NMA(in, string), NMA(out, string)),
		native_module_string_static_select));

	// Register optimizations
	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_noop_real_uid, NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_noop_real_uid, NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_noop_string_uid, NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_negation_uid, NMO_NM(k_native_module_negation_uid, NMO_V(0)))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_C(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_NM(k_native_module_negation_uid, NMO_V(1)))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_V(0), NMO_V(1)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_NM(k_native_module_negation_uid, NMO_V(0)), NMO_V(1))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_V(1), NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_NM(k_native_module_negation_uid, NMO_V(0)), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_C(0), NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_RV(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_V(0), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_NM(k_native_module_negation_uid, NMO_C(0))))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_C(0), NMO_NM(k_native_module_negation_uid, NMO_V(0)))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_C(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_V(0), NMO_NM(k_native_module_negation_uid, NMO_V(1)))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_addition_uid, NMO_V(0), NMO_V(1)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_subtraction_uid, NMO_RV(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_negation_uid, NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_multiplication_uid, NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_multiplication_uid, NMO_V(0), NMO_C(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_multiplication_uid, NMO_V(0), NMO_RV(0))),
		s_native_module_optimization_pattern::build(NMO_RV(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_multiplication_uid, NMO_V(0), NMO_RV(1))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_multiplication_uid, NMO_V(0), NMO_RV(-1))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_negation_uid, NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_V(0), NMO_RV(1))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_V(0), NMO_RV(-1))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_negation_uid, NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_NM(k_native_module_negation_uid, NMO_V(0)), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_V(0), NMO_NM(k_native_module_negation_uid, NMO_C(0))))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_C(0), NMO_NM(k_native_module_negation_uid, NMO_V(0)))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_division_uid, NMO_NM(k_native_module_negation_uid, NMO_C(0)), NMO_V(0)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(true), NMO_V(0), NMO_V(1))),
			s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(false), NMO_V(0), NMO_V(1))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(true), NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_C(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(false), NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(true), NMO_V(0), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_real_static_select_uid, NMO_BV(false), NMO_V(0), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_C(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(true), NMO_V(0), NMO_V(1))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(false), NMO_V(0), NMO_V(1))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(true), NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_C(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(false), NMO_C(0), NMO_V(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(true), NMO_V(0), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_V(0))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_string_static_select_uid, NMO_BV(false), NMO_V(0), NMO_C(0))),
		s_native_module_optimization_pattern::build(NMO_C(0))));

	// Register all operators
	for (uint32 op = 0; op < k_native_operator_count; op++) {
		c_native_module_registry::register_native_operator(
			static_cast<e_native_operator>(op),
			k_native_operator_names[op]);
	}
}