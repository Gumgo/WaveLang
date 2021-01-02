#include "execution_graph/native_module_registration.h"

namespace sampler_native_modules {

	void sampler(
		wl_argument(in const string, name),
		wl_argument(in const real, channel),
		wl_argument(in real, speed),
		wl_argument(return out real, result));

	void sampler_loop(
		wl_argument(in const string, name),
		wl_argument(in const real, channel),
		wl_argument(in const bool, bidi),
		wl_argument(in real, speed),
		wl_argument(in real, phase),
		wl_argument(return out real, result));

	void sampler_wavetable(
		wl_argument(in const real[], harmonic_weights),
		wl_argument(in real, frequency),
		wl_argument(in real, phase),
		wl_argument(return out real, result));

	void scrape_native_modules() {
		static constexpr uint32 k_sampler_library_id = 3;
		wl_native_module_library(k_sampler_library_id, "sampler", 0);

		wl_native_module(0x929e25f0, "sampler")
			.set_call_signature<decltype(sampler)>();

		wl_native_module(0x8c1293e3, "sampler_loop")
			.set_call_signature<decltype(sampler_loop)>();

		wl_native_module(0x5616fda5, "sampler_wavetable")
			.set_call_signature<decltype(sampler_wavetable)>();

		wl_end_active_library_native_module_registration();
	}

}
