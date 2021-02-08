#include "instrument/native_module_registration.h"
#include "instrument/native_modules/resampler/resampler.h"
#include "instrument/resampler/resampler.h"

static constexpr real32 k_quality_low = 0.0f;
static constexpr real32 k_quality_high = 1.0f;

void resample_real_validate_arguments(
	const s_native_module_context &context,
	bool is_upsampling,
	uint32 resample_factor,
	real32 quality) {
	if (get_resampler_filter_from_quality(is_upsampling, resample_factor, quality) == e_resampler_filter::k_invalid) {
		context.diagnostic_interface->error("Invalid quality '%f'", quality);
	}
}

int32 resample_real_get_latency(bool is_upsampling, uint32 resample_factor, real32 quality) {
	e_resampler_filter resampler_filter = get_resampler_filter_from_quality(is_upsampling, resample_factor, quality);
	if (is_upsampling) {
		// The latency is in terms of the downsampled sample rate
		const s_resampler_parameters &parameters = get_resampler_parameters(resampler_filter);
		return parameters.latency;
	} else {
		// The latency is in terms of the downsampled sample rate. Since we're sampling from the upsampled signal, the
		// reported resampler latency is in terms of the upsampled sample rate. To convert, we divide (rounding up) by
		// the downsample factor. By rounding up, we may add up to resample_factor - 1 additional samples of delay to
		// the upsampled signal.
		const s_resampler_parameters &parameters = get_resampler_parameters(resampler_filter);
		return (parameters.latency + resample_factor - 1) / resample_factor;
	}
}

namespace resampler_native_modules {

	void upsample2x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, true, 2, *quality);
	}

	int32 upsample2x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(true, 2, *quality);
	}

	void upsample2x_real(
		wl_argument(in real, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real@2x, upsampled_signal));

	void upsample3x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, true, 3, *quality);
	}

	int32 upsample3x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(true, 3, *quality);
	}

	void upsample3x_real(
		wl_argument(in real, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real@3x, upsampled_signal));

	void upsample4x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, true, 4, *quality);
	}

	int32 upsample4x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(true, 4, *quality);
	}

	void upsample4x_real(
		wl_argument(in real, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real@4x, upsampled_signal));

	void downsample2x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, false, 2, *quality);
	}

	int32 downsample2x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(false, 2, *quality);
	}

	void downsample2x_real(
		wl_argument(in real@2x, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real, downsampled_signal));

	void downsample3x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, false, 3, *quality);
	}

	int32 downsample3x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(false, 3, *quality);
	}

	void downsample3x_real(
		wl_argument(in real@3x, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real, downsampled_signal));

	void downsample4x_real_validate_arguments(
		const s_native_module_context &context,
		wl_argument(in const real, quality)) {
		resample_real_validate_arguments(context, false, 4, *quality);
	}

	int32 downsample4x_real_get_latency(
		wl_argument(in const real, quality)) {
		return resample_real_get_latency(false, 4, *quality);
	}

	void downsample4x_real(
		wl_argument(in real@4x, signal),
		wl_argument(in const real, quality),
		wl_argument(return out real, downsampled_signal));

	void upsample2x_bool(
		wl_argument(in bool, signal),
		wl_argument(return out bool@2x, upsampled_signal));

	void upsample3x_bool(
		wl_argument(in bool, signal),
		wl_argument(return out bool@3x, upsampled_signal));

	void upsample4x_bool(
		wl_argument(in bool, signal),
		wl_argument(return out bool@4x, upsampled_signal));

	void downsample2x_bool(
		wl_argument(in bool@2x, signal),
		wl_argument(return out bool, downsampled_signal));

	void downsample3x_bool(
		wl_argument(in bool@3x, signal),
		wl_argument(return out bool, downsampled_signal));

	void downsample4x_bool(
		wl_argument(in bool@4x, signal),
		wl_argument(return out bool, downsampled_signal));

	void scrape_native_modules() {
		static constexpr uint32 k_resampler_library_id = 10;
		wl_native_module_library(k_resampler_library_id, "resampler", 0);

		wl_native_module(0xd6c7639f, "upsample2x$real")
			.set_call_signature<decltype(upsample2x_real)>()
			.set_validate_arguments<upsample2x_real_validate_arguments>()
			.set_get_latency<upsample2x_real_get_latency>();

		wl_native_module(0x3a9d7f98, "upsample3x$real")
			.set_call_signature<decltype(upsample3x_real)>()
			.set_validate_arguments<upsample3x_real_validate_arguments>()
			.set_get_latency<upsample3x_real_get_latency>();

		wl_native_module(0xc10e1b4a, "upsample4x$real")
			.set_call_signature<decltype(upsample4x_real)>()
			.set_validate_arguments<upsample4x_real_validate_arguments>()
			.set_get_latency<upsample4x_real_get_latency>();

		wl_native_module(0xb2503502, "downsample2x$real")
			.set_call_signature<decltype(downsample2x_real)>()
			.set_validate_arguments<downsample2x_real_validate_arguments>()
			.set_get_latency<downsample2x_real_get_latency>();

		wl_native_module(0xbbef5b63, "downsample3x$real")
			.set_call_signature<decltype(downsample3x_real)>()
			.set_validate_arguments<downsample3x_real_validate_arguments>()
			.set_get_latency<downsample3x_real_get_latency>();

		wl_native_module(0x4c15ce35, "downsample4x$real")
			.set_call_signature<decltype(downsample4x_real)>()
			.set_validate_arguments<downsample4x_real_validate_arguments>()
			.set_get_latency<downsample4x_real_get_latency>();

		wl_native_module(0xa09fc17f, "upsample2x$bool")
			.set_call_signature<decltype(upsample2x_bool)>();

		wl_native_module(0x629a1312, "upsample3x$bool")
			.set_call_signature<decltype(upsample3x_bool)>();

		wl_native_module(0x8715cded, "upsample4x$bool")
			.set_call_signature<decltype(upsample4x_bool)>();

		wl_native_module(0x6a1c8dcf, "downsample2x$bool")
			.set_call_signature<decltype(downsample2x_bool)>();

		wl_native_module(0xa2e14dc6, "downsample3x$bool")
			.set_call_signature<decltype(downsample3x_bool)>();

		wl_native_module(0x9e0e58da, "downsample4x$bool")
			.set_call_signature<decltype(downsample4x_bool)>();

		// $TODO do we want to add decimate functions for reals? This would simply be "take every Nth sample" with no
		// filtering. It would be a cheap alternative for situations where a filter (e.g. a steep IIR) is already being
		// applied.

		wl_optimization_rule(upsample2x$real(0, const q) -> 0);
		wl_optimization_rule(upsample3x$real(0, const q) -> 0);
		wl_optimization_rule(upsample4x$real(0, const q) -> 0);
		wl_optimization_rule(downsample2x$real(0, const q) -> 0);
		wl_optimization_rule(downsample3x$real(0, const q) -> 0);
		wl_optimization_rule(downsample4x$real(0, const q) -> 0);
		wl_optimization_rule(upsample2x$bool(const x, const q) -> x);
		wl_optimization_rule(upsample3x$bool(const x, const q) -> x);
		wl_optimization_rule(upsample4x$bool(const x, const q) -> x);
		wl_optimization_rule(downsample2x$bool(const x, const q) -> x);
		wl_optimization_rule(downsample3x$bool(const x, const q) -> x);
		wl_optimization_rule(downsample4x$bool(const x, const q) -> x);

		wl_end_active_library_native_module_registration();
	}

}
