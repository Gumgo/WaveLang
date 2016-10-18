#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_UTILITY_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_UTILITY_H__

#include "common/common.h"

#include "execution_graph/native_module.h"

const uint32 k_native_modules_utility_library_id = 1;
wl_library(k_native_modules_utility_library_id, "utility");

extern const s_native_module_uid k_native_module_abs_uid;
wl_native_module(k_native_module_abs_uid, "abs")
void native_module_abs(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_floor_uid;
wl_native_module(k_native_module_floor_uid, "floor")
void native_module_floor(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_ceil_uid;
wl_native_module(k_native_module_ceil_uid, "ceil")
void native_module_ceil(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_round_uid;
wl_native_module(k_native_module_round_uid, "round")
void native_module_round(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_min_uid;
wl_native_module(k_native_module_min_uid, "min")
void native_module_min(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_max_uid;
wl_native_module(k_native_module_max_uid, "max")
void native_module_max(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_exp_uid;
wl_native_module(k_native_module_exp_uid, "exp")
void native_module_exp(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_log_uid;
wl_native_module(k_native_module_log_uid, "log")
void native_module_log(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_sqrt_uid;
wl_native_module(k_native_module_sqrt_uid, "sqrt")
void native_module_sqrt(real32 wl_in a, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_pow_uid;
wl_native_module(k_native_module_pow_uid, "pow")
void native_module_pow(real32 wl_in a, real32 wl_in b, real32 wl_out_return &result);

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_UTILITY_H__
