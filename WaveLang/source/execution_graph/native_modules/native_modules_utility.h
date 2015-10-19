#ifndef WAVELANG_NATIVE_MODULES_UTILITY_H__
#define WAVELANG_NATIVE_MODULES_UTILITY_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

static const uint32 k_native_modules_utility_library_id = 1;
extern const s_native_module_uid k_native_module_abs_uid;
extern const s_native_module_uid k_native_module_floor_uid;
extern const s_native_module_uid k_native_module_ceil_uid;
extern const s_native_module_uid k_native_module_round_uid;
extern const s_native_module_uid k_native_module_min_uid;
extern const s_native_module_uid k_native_module_max_uid;
extern const s_native_module_uid k_native_module_exp_uid;
extern const s_native_module_uid k_native_module_log_uid;
extern const s_native_module_uid k_native_module_sqrt_uid;
extern const s_native_module_uid k_native_module_pow_uid;

void register_native_modules_utility();

#endif // WAVELANG_NATIVE_MODULES_UTILITY_H__