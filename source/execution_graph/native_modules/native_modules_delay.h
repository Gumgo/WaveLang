#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__

#include "common/common.h"

#include "execution_graph/native_module.h"

const uint32 k_native_modules_delay_library_id = 3;
wl_library(k_native_modules_delay_library_id, "delay");

extern const s_native_module_uid k_native_module_delay_uid;
wl_native_module(k_native_module_delay_uid, "delay")
wl_runtime_only
void native_module_delay_delay(real32 wl_in_const duration, real32 wl_in signal, real32 wl_out_return &result);

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__
