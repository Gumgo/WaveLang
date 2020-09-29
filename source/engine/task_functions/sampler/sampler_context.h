#pragma once

#include "common/common.h"
#include "common/math/math.h"

#include "engine/sample/sample.h"
#include "engine/task_function.h"

class c_event_interface;
class c_sample_library_requester;

struct s_sampler_context {
	void initialize_file(
		c_event_interface *event_interface,
		c_sample_library_requester *sample_requester,
		const char *sample,
		e_sample_loop_mode loop_mode,
		bool phase_shift_enabled,
		real32 channel_real);
	void initialize_wavetable(
		c_event_interface *event_interface,
		c_sample_library_requester *sample_requester,
		c_real_constant_array harmonic_weights,
		bool phase_shift_enabled);
	void voice_initialize();

	// Common utility functions used in all versions of the sampler:

	// Fills the output buffer with 0s if the sample failed to load or if the channel is invalid
	const c_sample *get_sample_or_fail_gracefully(
		const c_sample_library_accessor *sample_accessor,
		c_real_buffer *result,
		c_event_interface *event_interface,
		const char *sample_name);

	// Fills the output buffer with 0s if the end has been reached
	bool handle_reached_end(c_real_buffer *result);

	// Returns the next sample index and increments using the advance value provided. Handles end detection.
	real64 increment_time(real64 length_samples, real32 advance);

	// Returns the next sample index and increments using the advance value provided. Handles looping.
	real64 increment_time_looping(real64 loop_start_sample, real64 loop_end_sample, real32 advance);

	// Store the sample index in real64 for improved precision so we can accurately handle both short and long samples
	h_sample sample_handle;
	uint32 channel;
	real64 sample_index;
	bool reached_end;
	bool sample_failure_reported;
};
