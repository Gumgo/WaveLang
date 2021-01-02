#include "execution_graph/native_module_registration.h"


namespace filter_native_modules {

	// Biquad filter assumes normalized a0 coefficient
	void biquad(
		wl_argument(in real, a1),
		wl_argument(in real, a2),
		wl_argument(in real, b0),
		wl_argument(in real, b1),
		wl_argument(in real, b2),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void scrape_native_modules() {
		static constexpr uint32 k_filter_library_id = 5;
		wl_native_module_library(k_filter_library_id, "filter", 0);

		wl_native_module(0x683cea52, "biquad")
			.set_call_signature<decltype(biquad)>();

		wl_end_active_library_native_module_registration();
	}

}
