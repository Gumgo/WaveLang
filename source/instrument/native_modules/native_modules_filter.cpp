#include "instrument/native_module_registration.h"
#include "instrument/resampler/resampler.h"

// $TODO consider adding version of comb_feedback and variable_comb_feedback that take seconds as input

static constexpr uint32 k_performance_warning_delay_threshold = 32;

static bool validate_delay(real32 delay, uint32 *integer_delay_out = nullptr) {
	if (integer_delay_out) {
		*integer_delay_out = 0;
	}

	if (std::isnan(delay) || std::isinf(delay) || delay <= 0.0f) {
		return false;
	}

	uint32 integer_delay = static_cast<uint32>(delay);
	if (integer_delay == 0) {
		return false;
	}

	if (integer_delay_out) {
		*integer_delay_out = integer_delay;
	}

	return true;
}

namespace filter_native_modules {

	void fir_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real[], coefficients)) {
		if (coefficients->get_array().empty()) {
			context.diagnostic_interface->error("No coefficients provided");
		}
	}

	void fir(
		wl_argument(in const real[], coefficients),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void iir_sos_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in ref real[], coefficients)) {
		if (coefficients->get_array().empty()) {
			context.diagnostic_interface->error("No coefficients provided");
		} else if (coefficients->get_array().size() % 5 != 0) {
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
		} else if (delays->get_array().empty()) {
			context.diagnostic_interface->error("No delays or gains provided");
		}

		for (real32 delay : delays->get_array()) {
			if (!validate_delay(delay)) {
				context.diagnostic_interface->error("Invalid delay '%f'", delay);
			}
		}
	}

	// Delays are specified in samples
	void allpass(
		wl_argument(in const real[], delays),
		wl_argument(in const real[], gains),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void comb_feedback_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, delay),
		wl_argument(in ref real[], iir_sos_coefficients),
		wl_argument(in const real[], allpass_delays),
		wl_argument(in const real[], allpass_gains)) {
		if (!validate_delay(delay)) {
			context.diagnostic_interface->error("Invalid delay '%f'", delay);
		} else if (delay < k_performance_warning_delay_threshold) {
			context.diagnostic_interface->warning("Delay is very small, performance may suffer");
		}

		if (iir_sos_coefficients->get_array().size() % 5 != 0) {
			context.diagnostic_interface->error("IIR SOS coefficients array length must be a multiple of 5");
		}

		if (allpass_delays->get_array().size() != allpass_gains->get_array().size()) {
			context.diagnostic_interface->error("Different number of allpass delays and gains provided");
		}

		for (real32 allpass_delay : allpass_delays->get_array()) {
			if (!validate_delay(allpass_delay)) {
				context.diagnostic_interface->error("Invalid allpass delay '%f'", allpass_delay);
			}
		}
	}

	void comb_feedback(
		wl_argument(in const real, delay),
		wl_argument(in real, gain),
		wl_argument(in const real[], fir_coefficients),
		wl_argument(in ref real[], iir_sos_coefficients),
		wl_argument(in const real[], allpass_delays),
		wl_argument(in const real[], allpass_gains),
		wl_argument(in real, signal),
		wl_argument(return out real, result));

	void variable_comb_feedback_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, min_delay),
		wl_argument(in const real, max_delay),
		wl_argument(in ref real[], iir_sos_coefficients),
		wl_argument(in const real[], allpass_delays),
		wl_argument(in const real[], allpass_gains)) {
		bool delays_valid = true;

		const s_resampler_parameters &resampler_parameters =
			get_resampler_parameters(e_resampler_filter::k_upsample_low_quality);

		// To sample at index i we need latency samples in the future. Our delay must be at least 1 sample plus the
		// additional latency required.
		uint32 min_allowed_delay = resampler_parameters.latency + 1;

		uint32 min_delay_integer;
		if (!validate_delay(min_delay, &min_delay_integer)) {
			delays_valid = false;
			context.diagnostic_interface->error("Invalid minimum delay '%f'", min_delay);
		} else if (min_delay_integer < min_allowed_delay) {
			delays_valid = false;
			context.diagnostic_interface->error("Minimum delay must be at least %u", min_allowed_delay);
		}

		uint32 max_delay_integer;
		if (!validate_delay(std::ceil(max_delay), &max_delay_integer)) {
			delays_valid = false;
			context.diagnostic_interface->error("Invalid maximum delay '%f'", max_delay);
		}

		if (delays_valid && min_delay_integer > max_delay_integer) {
			delays_valid = false;
			context.diagnostic_interface->error("Minimum delay is greater than maximum delay");
		}

		if (delays_valid && min_delay - min_allowed_delay < k_performance_warning_delay_threshold) {
			context.diagnostic_interface->warning("Minimum delay is very small, performance may suffer");
		}

		if (iir_sos_coefficients->get_array().size() % 5 != 0) {
			context.diagnostic_interface->error("IIR SOS coefficients array length must be a multiple of 5");
		}

		if (allpass_delays->get_array().size() != allpass_gains->get_array().size()) {
			context.diagnostic_interface->error("Different number of allpass delays and gains provided");
		}

		for (real32 allpass_delay : allpass_delays->get_array()) {
			if (!validate_delay(allpass_delay)) {
				context.diagnostic_interface->error("Invalid allpass delay '%f'", allpass_delay);
			}
		}
	}

	void variable_comb_feedback(
		wl_argument(in const real, min_delay),
		wl_argument(in const real, max_delay),
		wl_argument(in real, gain),
		wl_argument(in const real[], fir_coefficients),
		wl_argument(in ref real[], iir_sos_coefficients),
		wl_argument(in const real[], allpass_delays),
		wl_argument(in const real[], allpass_gains),
		wl_argument(in real, signal),
		wl_argument(in real, delay),
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

		wl_native_module(0x53c1ed94, "comb_feedback")
			.set_call_signature<decltype(comb_feedback)>()
			.set_validate_arguments<comb_feedback_validate_arguments>();

		wl_native_module(0x500cf73e, "variable_comb_feedback")
			.set_call_signature<decltype(variable_comb_feedback)>()
			.set_validate_arguments<variable_comb_feedback_validate_arguments>();

		wl_end_active_library_native_module_registration();
	}

}
