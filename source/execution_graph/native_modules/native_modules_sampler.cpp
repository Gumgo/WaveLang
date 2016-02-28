#include "execution_graph/native_modules/native_modules_sampler.h"
#include "execution_graph/native_module_registry.h"

const s_native_module_uid k_native_module_sampler_uid		= s_native_module_uid::build(k_native_modules_sampler_library_id, 0);
const s_native_module_uid k_native_module_sampler_loop_uid	= s_native_module_uid::build(k_native_modules_sampler_library_id, 1);

void register_native_modules_sampler() {
	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_sampler_uid, NATIVE_PREFIX "sampler",
		true, s_native_module_argument_list::build(
			NMA(constant, string),	// Name
			NMA(constant, real),	// Channel
			NMA(in, real),			// Speed
			NMA(out, real)),		// Result
		nullptr));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_sampler_loop_uid, NATIVE_PREFIX "sampler_loop",
		true, s_native_module_argument_list::build(
			NMA(constant, string),	// Name
			NMA(constant, real),	// Channel
			NMA(constant, bool),	// Bidi
			NMA(in, real),			// Speed
			NMA(in, real),			// Phase
			NMA(out, real)),		// Result
		nullptr));

	// Register optimizations
	// (none currently)

}