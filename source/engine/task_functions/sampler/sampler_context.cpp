#include "engine/events/event_data_types.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/sampler/sampler_context.h"

#include <cmath>

void s_sampler_shared_context::initialize_file(
	c_event_interface *event_interface,
	c_sample_library *sample_library,
	const char *sample,
	e_sample_loop_mode loop_mode,
	bool phase_shift_enabled,
	real32 channel_real) {
	wl_assert(valid_enum_index(loop_mode));

	s_file_sample_parameters parameters;
	parameters.filename = sample;
	parameters.loop_mode = loop_mode;
	parameters.phase_shift_enabled = phase_shift_enabled;
	sample_handle = sample_library->request_sample(parameters);
	if (channel_real < 0.0f
		|| std::floor(channel_real) != channel_real) {
		sample_handle = h_sample::invalid();
		event_interface->submit(EVENT_ERROR << "Invalid sample channel '" << channel_real << "'");
	}

	channel = static_cast<uint32>(channel_real);
}

void s_sampler_shared_context::initialize_wavetable(
	c_event_interface *event_interface,
	c_sample_library *sample_library,
	c_real_constant_array harmonic_weights,
	bool phase_shift_enabled) {
	bool valid = true;

	if (harmonic_weights.get_count() == 0) {
		valid = false;
	}

	if (valid) {
		s_wavetable_sample_parameters parameters;
		parameters.harmonic_weights = harmonic_weights;
		parameters.phase_shift_enabled = phase_shift_enabled;
		sample_handle = sample_library->request_sample(parameters);
	} else {
		sample_handle = h_sample::invalid();
	}

	channel = 0;
}

void s_sampler_voice_context::initialize(s_sampler_shared_context *sampler_shared_context) {
	shared_context = shared_context;
}

void s_sampler_voice_context::activate() {
	sample_index = 0.0;
	reached_end = false;
}

const c_sample *s_sampler_voice_context::get_sample_or_fail_gracefully(
	const c_sample_library *sample_library,
	c_real_buffer *result,
	c_event_interface *event_interface,
	const char *sample_name) {
	bool failed = false;

	const c_sample *sample = sample_library->get_sample(shared_context->sample_handle, shared_context->channel);

	// If the sample failed, fill the buffer with 0
	if (!sample) {
		if (!shared_context->sample_failure_reported) {
			shared_context->sample_failure_reported = true;

			// Determine whether the same failed to load or an invalid channel was specified
			if (!sample_library->get_sample(shared_context->sample_handle, 0)) {
				event_interface->submit(EVENT_ERROR << "Failed to load sample '" << c_dstr(sample_name) << "'");
			} else {
				event_interface->submit(
					EVENT_ERROR << "Invalid sample channel '" << shared_context->channel
					<< "' for sample '" << c_dstr(sample_name) << "'");
			}
		}

		result->assign_constant(0.0f);
	}


	return sample;
}

bool s_sampler_voice_context::handle_reached_end(c_real_buffer *result) {
	if (reached_end) {
		result->assign_constant(0.0f);
	}

	return reached_end;
}

real64 s_sampler_voice_context::increment_time(real64 length_samples, real32 advance) {
	wl_assert(!reached_end);
	real64 result = sample_index;
	sample_index += advance;
	if (sample_index >= length_samples) {
		reached_end = true;
	}

	return result;
}

real64 s_sampler_voice_context::increment_time_looping(
	real64 loop_start_sample,
	real64 loop_end_sample,
	real32 advance) {
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
