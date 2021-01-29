#include "instrument/native_module_registration.h"

namespace delay_native_modules {

	void delay_samples_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, duration)) {
		if (duration < 0.0f || std::isnan(duration) || std::isinf(duration) || static_cast<int32>(duration) < 0) {
			context.diagnostic_interface->error("Invalid delay samples duration '%f'", duration);
		}
	}

	void delay_samples_real(
		wl_argument(in const real, duration),
		wl_argument(in ref real, signal),
		wl_argument(return out ref real, result));

	void delay_samples_bool(
		wl_argument(in const real, duration),
		wl_argument(in ref bool, signal),
		wl_argument(return out ref bool, result));

	void delay_samples_initial_value_real(
		wl_argument(in const real, duration),
		wl_argument(in ref real, signal),
		wl_argument(in const real, initial_value),
		wl_argument(return out ref real, result));

	void delay_samples_initial_value_bool(
		wl_argument(in const real, duration),
		wl_argument(in ref bool, signal),
		wl_argument(in const bool, initial_value),
		wl_argument(return out ref bool, result));

	void delay_seconds_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, duration)) {
		if (duration < 0.0f || std::isnan(duration) || std::isinf(duration)) {
			context.diagnostic_interface->error("Invalid delay seconds duration '%f'", duration);
		}
	}

	void delay_seconds_real(
		wl_argument(in const real, duration),
		wl_argument(in ref real, signal),
		wl_argument(return out ref real, result));

	void delay_seconds_bool(
		wl_argument(in const real, duration),
		wl_argument(in ref bool, signal),
		wl_argument(return out ref bool, result));

	void delay_seconds_initial_value_real(
		wl_argument(in const real, duration),
		wl_argument(in ref real, signal),
		wl_argument(in const real, initial_value),
		wl_argument(return out ref real, result));

	void delay_seconds_initial_value_bool(
		wl_argument(in const real, duration),
		wl_argument(in ref bool, signal),
		wl_argument(in const bool, initial_value),
		wl_argument(return out ref bool, result));

	// $TODO $NATIVE_MODULES maybe call this "hold" instead?
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

		wl_native_module(0xeac74e43, "delay_samples$real")
			.set_intrinsic(e_native_module_intrinsic::k_delay_samples_real)
			.set_call_signature<decltype(delay_samples_real)>()
			.set_validate_arguments<delay_samples_validate_arguments>();

		wl_native_module(0xa5611aa6, "delay_samples$bool")
			.set_intrinsic(e_native_module_intrinsic::k_delay_samples_bool)
			.set_call_signature<decltype(delay_samples_bool)>()
			.set_validate_arguments<delay_samples_validate_arguments>();

		wl_native_module(0xfcd71bac, "delay_samples$initial_value_real")
			.set_call_signature<decltype(delay_samples_initial_value_real)>()
			.set_validate_arguments<delay_samples_validate_arguments>();

		wl_native_module(0xbb159914, "delay_samples$initial_value_bool")
			.set_call_signature<decltype(delay_samples_initial_value_bool)>()
			.set_validate_arguments<delay_samples_validate_arguments>();

		wl_native_module(0x5f34d0ea, "delay_seconds$real")
			.set_call_signature<decltype(delay_seconds_real)>()
			.set_validate_arguments<delay_seconds_validate_arguments>();

		wl_native_module(0x062a04e2, "delay_seconds$bool")
			.set_call_signature<decltype(delay_seconds_bool)>()
			.set_validate_arguments<delay_seconds_validate_arguments>();

		wl_native_module(0x48331326, "delay_seconds$initial_value_real")
			.set_call_signature<decltype(delay_seconds_initial_value_real)>()
			.set_validate_arguments<delay_seconds_validate_arguments>();

		wl_native_module(0xf84249ca, "delay_seconds$initial_value_bool")
			.set_call_signature<decltype(delay_seconds_initial_value_bool)>()
			.set_validate_arguments<delay_seconds_validate_arguments>();

		wl_native_module(0x2dcd7ea7, "memory$real")
			.set_call_signature<decltype(memory_real)>();

		wl_native_module(0xe5cd6275, "memory$bool")
			.set_call_signature<decltype(memory_bool)>();

		// Remove 0-duration delays
		wl_optimization_rule(delay_samples$real(0, x) -> x);
		wl_optimization_rule(delay_samples$initial_value_real(0, x) -> x);
		wl_optimization_rule(delay_samples$bool(0, x) -> x);
		wl_optimization_rule(delay_samples$initial_value_bool(0, x) -> x);
		wl_optimization_rule(delay_seconds$real(0, x) -> x);
		wl_optimization_rule(delay_seconds$initial_value_real(0, x) -> x);
		wl_optimization_rule(delay_seconds$bool(0, x) -> x);
		wl_optimization_rule(delay_seconds$initial_value_bool(0, x) -> x);

		// Delays with a constant signal matching the initial value can be removed
		wl_optimization_rule(delay_samples$real(const a, 0) -> 0);
		wl_optimization_rule(delay_samples$bool(const a, false) -> false);
		wl_optimization_rule(delay_seconds$real(const a, 0) -> 0);
		wl_optimization_rule(delay_seconds$bool(const a, false) -> false);

		// Summed delays combine into one
		wl_optimization_rule(
			delay_samples$real(const a, delay_samples$real(const b, x))
			-> delay_samples$real(core.addition(a, b), x));
		wl_optimization_rule(
			delay_samples$bool(const a, delay_samples$bool(const b, x))
			-> delay_samples$bool(core.addition(a, b), x));
		wl_optimization_rule(
			delay_seconds$real(const a, delay_seconds$real(const b, x))
			-> delay_seconds$real(core.addition(a, b), x));
		wl_optimization_rule(
			delay_seconds$bool(const a, delay_seconds$bool(const b, x))
			-> delay_seconds$bool(core.addition(a, b), x));

		wl_end_active_library_native_module_registration();
	}

}
