#ifndef WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_SAMPLER_CONTEXT_H__
#define WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_SAMPLER_CONTEXT_H__

#include "common/common.h"

#include "engine/buffer_operations/buffer_operations.h"
#include "engine/math/math.h"
#include "engine/sample/sample.h"

class c_event_interface;
class c_sample_library_requester;

struct s_sampler_context {
	static size_t query_memory();
	void initialize(c_event_interface *event_interface, c_sample_library_requester *sample_requester,
		const char *sample, e_sample_loop_mode loop_mode, bool phase_shift_enabled, real32 channel_real);
	void voice_initialize();

	// Common utility functions used in all versions of the sampler:

	// Fills the output buffer with 0s if the sample failed to load or if the channel is invalid
	bool handle_failed_sample(const c_sample *sample, c_real_buffer_out out,
		c_event_interface *event_interface, const char *sample_name);

	// Fills the output buffer with 0s if the end has been reached
	bool handle_reached_end(c_real_buffer_out out);

	// increment_time/increment_time_looping:
	// Increments the sample's current time up to 4 times based on the speed provided and returns the number of times
	// incremented, which can be less than 4 if the sample ends or if the end of the buffer is reached. The time before
	// each increment is stored in out_time. Handles looping/wrapping.

	size_t increment_time(real64 length_samples, const c_real32_4 &advance,
		size_t &inout_buffer_samples_remaining, s_static_array<real64, k_simd_block_elements> &out_samples);

	size_t increment_time_looping(real64 loop_start_sample, real64 loop_end_sample, const c_real32_4 &advance,
		size_t &inout_buffer_samples_remaining, s_static_array<real64, k_simd_block_elements> &out_samples);

	// Store the sample index in real64 for improved precision so we can accurately handle both short and long samples
	uint32 sample_handle;
	uint32 channel;
	real64 sample_index;
	bool reached_end;
	bool sample_failure_reported;
};

#endif // WAVELANG_ENGINE_TASK_FUNCTIONS_SAMPLER_SAMPLER_CONTEXT_H__
