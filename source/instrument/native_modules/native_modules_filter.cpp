#include "instrument/native_module_registration.h"

namespace filter_native_modules {

	void fir_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real[], coefficients)) {
		if (coefficients->get_array().empty()) {
			context.diagnostic_interface->error("Empty coefficients array");
		}
	}

	void fir(
		wl_argument(in const real[], coefficients),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void iir_sos_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in ref real[], coefficients)) {
		if (coefficients->get_array().size() % 5 != 0) {
			context.diagnostic_interface->error("Coefficients array length must be a multiple of 5");
		}
	}

	// "coefficients" is an array of sets of 5 elements: b0, b1, b2, a1, a2. a0 is assumed to be 1.
	void iir_sos(
		wl_argument(in ref real[], coefficients),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void allpass_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real[], delays),
		wl_argument(in const real[], gains)) {
		if (delays->get_array().size() != gains->get_array().size()) {
			context.diagnostic_interface->error("Different number of delays and gains provided");
			return;
		}

		if (delays->get_array().empty()) {
			context.diagnostic_interface->error("Empty delays array");
			return;
		}

		for (real32 delay : delays->get_array()) {
			if (std::isnan(delay) || std::isinf(delay) || delay < 0.0f) {
				context.diagnostic_interface->error("Invalid delay '%f'", delay);
				return;
			}
		}
	}

	// Delays are specified in samples
	void allpass(
		wl_argument(in const real[], delays),
		wl_argument(in const real[], gains),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void scrape_native_modules() {
		static constexpr uint32 k_filter_library_id = 5;
		wl_native_module_library(k_filter_library_id, "filter", 0);

		wl_native_module(0x5cfed7b9, "fir")
			.set_call_signature<decltype(fir)>()
			.set_validate_arguments<fir_validate_arguments>();

		wl_native_module(0x1f69ab2c, "iir_sos")
			.set_call_signature<decltype(iir_sos)>()
			.set_validate_arguments<iir_sos_validate_arguments>();

		wl_native_module(0xdf892b29, "allpass")
			.set_call_signature<decltype(allpass)>()
			.set_validate_arguments<allpass_validate_arguments>();

		wl_end_active_library_native_module_registration();
	}

}
