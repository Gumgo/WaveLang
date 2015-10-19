#include "execution_graph/native_modules/native_modules_sampler.h"
#include "execution_graph/native_module_registry.h"

const s_native_module_uid k_native_module_sampler_uid					= s_native_module_uid::build(k_native_modules_sampler_library_id, 0);
const s_native_module_uid k_native_module_sampler_loop_uid				= s_native_module_uid::build(k_native_modules_sampler_library_id, 1);
const s_native_module_uid k_native_module_sampler_loop_phase_shift_uid	= s_native_module_uid::build(k_native_modules_sampler_library_id, 2);

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
			NMA(out, real)),		// Result
		nullptr));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_sampler_loop_phase_shift_uid, NATIVE_PREFIX "sampler_loop_phase_shift",
		true, s_native_module_argument_list::build(
			NMA(constant, string),	// Name
			NMA(constant, real),	// Channel
			NMA(constant, bool),	// Bidi
			NMA(in, real),			// Speed
			NMA(in, real),			// Phase
			NMA(out, real)),		// Result
		nullptr));

	// Register optimizations
	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_sampler_loop_phase_shift_uid, NMO_C(0), NMO_C(1), NMO_C(2), NMO_C(3), NMO_RV(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_sampler_loop_uid, NMO_C(0), NMO_C(1), NMO_C(2), NMO_C(3)))));

	c_native_module_registry::register_optimization_rule(s_native_module_optimization_rule::build(
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_sampler_loop_phase_shift_uid, NMO_C(0), NMO_C(1), NMO_C(2), NMO_V(0), NMO_RV(0))),
		s_native_module_optimization_pattern::build(NMO_NM(k_native_module_sampler_loop_uid, NMO_C(0), NMO_C(1), NMO_C(2), NMO_V(0)))));
}