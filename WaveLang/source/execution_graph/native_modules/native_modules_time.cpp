#include "execution_graph/native_modules/native_modules_time.h"
#include "execution_graph/native_module_registry.h"

const s_native_module_uid k_native_module_time_period_uid = s_native_module_uid::build(k_native_modules_time_library_id, 0);

void register_native_modules_time() {
	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_time_period_uid, NATIVE_PREFIX "time_period",
		true, s_native_module_argument_list::build(NMA(constant, real), NMA(out, real)),
		nullptr));
}