#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_modules_utility.h"

// $TODO add trig functions

static uint32 g_next_module_id = 0;

const s_native_module_uid k_native_module_abs_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_abs(real32 a, real32 &result) {
	result = std::abs(a);
}

const s_native_module_uid k_native_module_floor_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_floor(real32 a, real32 &result) {
	result = std::floor(a);
}

const s_native_module_uid k_native_module_ceil_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_ceil(real32 a, real32 &result) {
	result = std::ceil(a);
}

const s_native_module_uid k_native_module_round_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_round(real32 a, real32 &result) {
	result = std::round(a);
}

const s_native_module_uid k_native_module_min_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_min(real32 a, real32 b, real32 &result) {
	result = std::fmin(a, b);
}

const s_native_module_uid k_native_module_max_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_max(real32 a, real32 b, real32 &result) {
	result = std::fmax(a, b);
}

const s_native_module_uid k_native_module_exp_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_exp(real32 a, real32 &result) {
	result = std::exp(a);
}

const s_native_module_uid k_native_module_log_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_log(real32 a, real32 &result) {
	result = std::log(a);
}

const s_native_module_uid k_native_module_sqrt_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_sqrt(real32 a, real32 &result) {
	result = std::sqrt(a);
}

const s_native_module_uid k_native_module_pow_uid = s_native_module_uid::build(k_native_modules_utility_library_id, g_next_module_id++);
void native_module_pow(real32 a, real32 b, real32 &result) {
	result = std::pow(a, b);
}
