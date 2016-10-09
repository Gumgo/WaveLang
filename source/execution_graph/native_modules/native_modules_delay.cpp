#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_modules_delay.h"

static const char *k_native_modules_delay_library_name = "delay";
const s_native_module_uid k_native_module_delay_uid = s_native_module_uid::build(k_native_modules_delay_library_id, 0);

void register_native_modules_delay() {
	c_native_module_registry::register_native_module_library(
		k_native_modules_delay_library_id, k_native_modules_delay_library_name);

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_delay_uid, NATIVE_PREFIX "delay",
		true, s_native_module_argument_list::build(NMA(constant, real, "duration"), NMA(in, real, "signal"), NMA(out, real, "")),
		nullptr));
}