#ifndef WAVELANG_NATIVE_MODULES_DELAY_H__
#define WAVELANG_NATIVE_MODULES_DELAY_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

static const uint32 k_native_modules_delay_library_id = 3;
extern const s_native_module_uid k_native_module_delay_uid;

void register_native_modules_delay();

#endif // WAVELANG_NATIVE_MODULES_DELAY_H__