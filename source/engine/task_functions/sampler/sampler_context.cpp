#include "engine/events/event_data_types.h"
#include "engine/events/event_interface.h"
#include "engine/sample/sample_library.h"
#include "engine/task_functions/sampler/sampler_context.h"

#include <cmath>

void s_sampler_context::initialize_file(
	c_event_interface *event_interface,
	c_sample_library_requester *sample_requester,
	const char *sample,
	e_sample_loop_mode loop_mode,
	bool phase_shift_enabled,
	real32 channel_real) {
	wl_assert(valid_enum_index(loop_mode));

	s_file_sample_parameters parameters;
	parameters.filename = sample;
	parameters.loop_mode = loop_mode;
	parameters.phase_shift_enabled = phase_shift_enabled;
	sample_handle = sample_requester->request_sample(parameters);
	if (channel_real < 0.0f ||
		std::floor(channel_real) != channel_real) {
		sample_handle = c_sample_library::k_invalid_handle;
		event_interface->submit(EVENT_ERROR << "Invalid sample channel '" << channel_real << "'");
	}
	channel = static_cast<uint32>(channel_real);
	sample_index = 0.0;
	reached_end = false;
}

void s_sampler_context::initialize_wavetable(
	c_event_interface *event_interface,
	c_sample_library_requester *sample_requester,
	c_real_constant_array harmonic_weights,
	real32 sample_count_real,
	bool phase_shift_enabled) {
	bool valid = true;

	if (harmonic_weights.get_count() == 0) {
		valid = false;
	}

	if (sample_count_real < 0.0f ||
		std::floor(sample_count_real) != sample_count_real) {
		event_interface->submit(
			EVENT_ERROR << "Invalid wavetable sample count '" << sample_count_real << "'");
		valid = false;
	}

	uint32 sample_count = static_cast<uint32>(sample_count_real);
	{
		uint32 sample_count_pow2_check = 1;
		while (sample_count_pow2_check < sample_count) {
			sample_count_pow2_check *= 2;
		}

		if (sample_count != sample_count_pow2_check) {
			event_interface->submit(
				EVENT_ERROR << "Wavetable sample count must be a power of 2");
			valid = false;
		}
	}

	if (valid) {
		s_wavetable_sample_parameters parameters;
		parameters.harmonic_weights = harmonic_weights;
		parameters.sample_count = sample_count;
		parameters.phase_shift_enabled = phase_shift_enabled;
		sample_handle = sample_requester->request_sample(parameters);
	} else {
		sample_handle = c_sample_library::k_invalid_handle;
	}

	channel = 0;
	sample_index = 0.0;
	reached_end = false;
}

void s_sampler_context::voice_initialize() {
	sample_index = 0.0;
	reached_end = false;
}

bool s_sampler_context::handle_failed_sample(
	const c_sample *sample,
	c_real_buffer *result,
	c_event_interface *event_interface,
	const char *sample_name) {
	bool failed = false;

	// If the sample failed, fill with 0
	if (!sample) {
		failed = true;

		if (!sample_failure_reported) {
			sample_failure_reported = true;
			event_interface->submit(EVENT_ERROR << "Failed to load sample '" << c_dstr(sample_name) << "'");
		}
	} if (sample->get_channel_count() < channel) {
		failed = true;

		if (!sample_failure_reported) {
			sample_failure_reported = true;
			event_interface->submit(
				EVENT_ERROR << "Invalid sample channel '" << channel << "' for sample '" << c_dstr(sample_name) << "'");
		}
	}

	if (failed) {
		result->assign_constant(0.0f);
	}

	return failed;
}

bool s_sampler_context::handle_reached_end(c_real_buffer *result) {
	if (reached_end) {
		result->assign_constant(0.0f);
	}

	return reached_end;
}

real64 s_sampler_context::increment_time(real64 length_samples, real32 advance) {
	wl_assert(!reached_end);
	real64 result = sample_index;
	sample_index += advance;
	if (sample_index >= length_samples) {
		reached_end = true;
	}

	return result;
}

real64 s_sampler_context::increment_time_looping(real64 loop_start_sample, real64 loop_end_sample, real32 advance) {
	wl_assert(!reached_end);
	real64 result = sample_index;
	sample_index += advance;
	if (sample_index >= loop_end_sample) {
		// Return to the loop start point
		real64 wrap_offset = std::remainder(sample_index - loop_start_sample, loop_end_sample - loop_start_sample);
		if (wrap_offset < 0.0) {
			wrap_offset += (loop_end_sample - loop_start_sample);
		}
		sample_index = loop_start_sample + wrap_offset;
		wl_assert(sample_index >= 0.0);
	}

	return result;
}
