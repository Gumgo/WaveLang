#ifndef WAVELANG_NATIVE_MODULES_SAMPLER_H__
#define WAVELANG_NATIVE_MODULES_SAMPLER_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

static const uint32 k_native_modules_sampler_library_id = 2;
extern const s_native_module_uid k_native_module_sampler_uid;
extern const s_native_module_uid k_native_module_sampler_loop_uid;

void register_native_modules_sampler();

#endif // WAVELANG_NATIVE_MODULES_SAMPLER_H__