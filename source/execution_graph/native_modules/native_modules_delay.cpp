#include "execution_graph/native_module_registration.h"

namespace delay_native_modules {

	void delay(
		wl_argument(in const real, duration),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void memory_real(
		wl_argument(in real, value),
		wl_argument(in bool, write),
		wl_argument(return out real, result));

	void memory_bool(
		wl_argument(in bool, value),
		wl_argument(in bool, write),
		wl_argument(return out bool, result));

	void scrape_native_modules() {
		static constexpr uint32 k_delay_library_id = 4;
		wl_native_module_library(k_delay_library_id, "delay", 0);

		wl_native_module(0x95f17597, "delay")
			.set_call_signature<decltype(delay)>();

		wl_native_module(0x2dcd7ea7, "memory$real")
			.set_call_signature<decltype(memory_real)>();

		wl_native_module(0xe5cd6275, "memory$bool")
			.set_call_signature<decltype(memory_bool)>();

		wl_end_active_library_native_module_registration();
	}

}
