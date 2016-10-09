#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_modules_utility.h"

// $TODO add trig functions

static const char *k_native_modules_utility_library_name = "utility";
const s_native_module_uid k_native_module_abs_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 0);
const s_native_module_uid k_native_module_floor_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 1);
const s_native_module_uid k_native_module_ceil_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 2);
const s_native_module_uid k_native_module_round_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 3);
const s_native_module_uid k_native_module_min_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 4);
const s_native_module_uid k_native_module_max_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 5);
const s_native_module_uid k_native_module_exp_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 6);
const s_native_module_uid k_native_module_log_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 7);
const s_native_module_uid k_native_module_sqrt_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 8);
const s_native_module_uid k_native_module_pow_uid	= s_native_module_uid::build(k_native_modules_utility_library_id, 9);

static void native_module_abs(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::abs(arguments[0].get_real());
}

static void native_module_floor(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::floor(arguments[0].get_real());
}

static void native_module_ceil(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::ceil(arguments[0].get_real());
}

static void native_module_round(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::round(arguments[0].get_real());
}

static void native_module_min(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 3);
	arguments[2] = std::fmin(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_max(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 3);
	arguments[2] = std::fmax(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_exp(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::exp(arguments[0].get_real());
}

static void native_module_log(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::log(arguments[0].get_real());
}

static void native_module_sqrt(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 2);
	arguments[1] = std::sqrt(arguments[0].get_real());
}

static void native_module_pow(c_native_module_compile_time_argument_list arguments) {
	wl_assert(arguments.get_count() == 3);
	arguments[2] = std::pow(arguments[0].get_real(), arguments[1].get_real());
}

void register_native_modules_utility() {
	c_native_module_registry::register_native_module_library(
		k_native_modules_utility_library_id, k_native_modules_utility_library_name);

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_abs_uid, NATIVE_PREFIX "abs",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_abs));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_floor_uid, NATIVE_PREFIX "floor",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_floor));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_ceil_uid, NATIVE_PREFIX "ceil",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_ceil));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_round_uid, NATIVE_PREFIX "round",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_round));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_min_uid, NATIVE_PREFIX "min",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(in, real, "y"), NMA(out, real, "")),
		native_module_min));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_max_uid, NATIVE_PREFIX "max",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(in, real, "y"), NMA(out, real, "")),
		native_module_max));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_exp_uid, NATIVE_PREFIX "exp",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_exp));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_log_uid, NATIVE_PREFIX "log",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_log));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_sqrt_uid, NATIVE_PREFIX "sqrt",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(out, real, "")),
		native_module_sqrt));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_pow_uid, NATIVE_PREFIX "pow",
		true, s_native_module_argument_list::build(NMA(in, real, "x"), NMA(in, real, "y"), NMA(out, real, "")),
		native_module_pow));
}