#ifndef WAVELANG_NATIVE_MODULES_TIME_H__
#define WAVELANG_NATIVE_MODULES_TIME_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

static const uint32 k_native_modules_time_library_id = 5;
extern const s_native_module_uid k_native_module_time_period_uid;

void register_native_modules_time();

#endif // WAVELANG_NATIVE_MODULES_TIME_H__