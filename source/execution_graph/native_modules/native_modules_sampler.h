#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_SAMPLER_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_SAMPLER_H__

#include "common/common.h"

#include "execution_graph/native_module.h"

const uint32 k_native_modules_sampler_library_id = 2;
wl_library(k_native_modules_sampler_library_id, "sampler");

extern const s_native_module_uid k_native_module_sampler_uid;
wl_native_module(k_native_module_sampler_uid, "sampler")
wl_runtime_only
void native_module_sampler(const c_native_module_string wl_in_const &name, real32 wl_in_const channel,
	real32 wl_in speed, real32 wl_out_return &result);

extern const s_native_module_uid k_native_module_sampler_loop_uid;
wl_native_module(k_native_module_sampler_loop_uid, "sampler_loop")
wl_runtime_only
void native_module_sampler_loop(const c_native_module_string wl_in_const &name, real32 wl_in_const channel,
	real32 wl_in speed, bool wl_in bidi, real32 wl_in phase, real32 wl_out_return &result);

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_SAMPLER_H__
