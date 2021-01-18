#include "instrument/native_module_registration.h"

namespace time_native_modules {

	void period(
		wl_argument(in const real, duration),
		wl_argument(return out real, result));

	void scrape_native_modules() {
		static constexpr uint32 k_time_library_id = 6;
		wl_native_module_library(k_time_library_id, "time", 0);

		wl_native_module(0xfa05392c, "period")
			.set_call_signature<decltype(period)>();

		wl_end_active_library_native_module_registration();
	}

}
