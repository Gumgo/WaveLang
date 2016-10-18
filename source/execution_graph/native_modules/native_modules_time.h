#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_TIME_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_TIME_H__

#include "common/common.h"

#include "execution_graph/native_module.h"

const uint32 k_native_modules_time_library_id = 5;
wl_library(k_native_modules_time_library_id, "time");

extern const s_native_module_uid k_native_module_time_period_uid;
wl_native_module(k_native_module_time_period_uid, "time_period")
wl_runtime_only
void native_module_time_time_period(real32 wl_in_const duration, real32 wl_out &result);

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_TIME_H__
