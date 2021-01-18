#include "instrument/instrument_globals.h"
#include "instrument/native_module_registration.h"

namespace stream_native_modules {

	void get_sample_rate(
		const s_native_module_context &context,
		wl_argument(return out const real, result)) {
		if (context.instrument_globals->sample_rate == 0) {
			context.diagnostic_interface->error(
				"Sample rate is only queryable if the instrument global '#sample_rate' is provided");
			*result = 0.0f;
		} else {
			*result = static_cast<real32>(context.instrument_globals->sample_rate);
		}
	}

	void scrape_native_modules() {
		static constexpr uint32 k_stream_library_id = 8;
		wl_native_module_library(k_stream_library_id, "stream", 0);

		wl_native_module(0x2e1c0616, "get_sample_rate")
			.set_compile_time_call<get_sample_rate>();

		wl_end_active_library_native_module_registration();
	}

}
