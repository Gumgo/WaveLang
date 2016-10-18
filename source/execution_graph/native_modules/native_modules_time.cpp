#include "execution_graph/native_modules/native_modules_time.h"

static uint32 g_next_module_id = 0;

const s_native_module_uid k_native_module_time_period_uid = s_native_module_uid::build(k_native_modules_time_library_id, g_next_module_id++);
