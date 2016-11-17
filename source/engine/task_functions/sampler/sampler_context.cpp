#include "engine/events/event_data_types.h"
#include "engine/events/event_interface.h"
#include "engine/sample/sample_library.h"
#include "engine/task_functions/sampler/sampler_context.h"

size_t s_sampler_context::query_memory() {
	return sizeof(s_sampler_context);
}

void s_sampler_context::initialize(c_event_interface *event_interface, c_sample_library_requester *sample_requester,
	const char *sample, e_sample_loop_mode loop_mode, bool phase_shift_enabled, real32 channel_real) {
	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));

	sample_handle = sample_requester->request_sample(sample, loop_mode, phase_shift_enabled);
	if (channel_real < 0.0f ||
		std::floor(channel_real) != channel_real) {
		sample_handle = c_sample_library::k_invalid_handle;
		event_interface->submit(EVENT_ERROR << "Invalid sample channel '" << channel_real << "'");
	}
	channel = static_cast<uint32>(channel_real);
	sample_index = 0.0;
	reached_end = false;
}

void s_sampler_context::voice_initialize() {
	sample_index = 0.0;
	reached_end = false;
}

bool s_sampler_context::handle_failed_sample(const c_sample *sample, c_real_buffer_out out,
	c_event_interface *event_interface, const char *sample_name) {
	bool failed = false;

	// If the sample failed, fill with 0
	if (!sample) {
		failed = true;

		if (!sample_failure_reported) {
			sample_failure_reported = true;
			event_interface->submit(EVENT_ERROR << "Failed to load sample '" << c_dstr(sample_name) << "'");
		}
	} else {
		uint32 channel_count = sample->is_mipmap() ?
			sample->get_mipmap_sample(0)->get_channel_count() :
			sample->get_channel_count();

		if (channel_count < channel) {
			failed = true;

			if (!sample_failure_reported) {
				sample_failure_reported = true;
				event_interface->submit(
					EVENT_ERROR << "Invalid sample channel '" << channel << "' for sample '" << c_dstr(sample_name) << "'");
			}
		}
	}

	if (failed) {
		*out->get_data<real32>() = 0.0f;
		out->set_constant(true);
	}

	return failed;
}

bool s_sampler_context::handle_reached_end(c_real_buffer_out out) {
	if (reached_end) {
		*out->get_data<real32>() = 0.0f;
		out->set_constant(true);
	}

	return reached_end;
}

size_t s_sampler_context::increment_time(real64 length_samples, const c_real32_4 &advance,
	size_t &inout_buffer_samples_remaining, s_static_array<real64, k_sse_block_elements> &out_samples) {
	// We haven't vectorized this, so extract the reals
	ALIGNAS_SSE s_static_array<real32, k_sse_block_elements> advance_array;
	advance.store(advance_array.get_elements());

	wl_assert(!reached_end);
	ZERO_STRUCT(&out_samples);

	// We will return the actual number of times incremented so we don't over-sample
	size_t increment_count = 0;
	for (uint32 i = 0; i < k_sse_block_elements && inout_buffer_samples_remaining > 0; i++) {
		increment_count++;
		inout_buffer_samples_remaining--;
		out_samples[i] = sample_index;

		// Increment the time - might be able to use SSE HADDs for this, but getting increment_count and reached_end
		// would probably be tricky
		sample_index += advance_array[i];
		if (sample_index >= length_samples) {
			reached_end = true;
			break;
		}
	}

	// We should only ever increment less than 4 times if we reach the end of the buffer or the end of the sample
	if (increment_count < k_sse_block_elements) {
		wl_assert(reached_end || inout_buffer_samples_remaining == 0);
	}

	return increment_count;
}

size_t s_sampler_context::increment_time_looping(
	real64 loop_start_sample, real64 loop_end_sample, const c_real32_4 &advance,
	size_t &inout_buffer_samples_remaining, s_static_array<real64, k_sse_block_elements> &out_samples) {
	// We haven't vectorized this, so extract the reals
	ALIGNAS_SSE s_static_array<real32, k_sse_block_elements> advance_array;
	advance.store(advance_array.get_elements());

	wl_assert(!reached_end);
	ZERO_STRUCT(&out_samples);

	// We will return the actual number of times incremented so we don't over-sample
	size_t increment_count = 0;
	for (uint32 i = 0; i < k_sse_block_elements && inout_buffer_samples_remaining > 0; i++) {
		increment_count++;
		inout_buffer_samples_remaining--;
		out_samples[i] = sample_index;

		// Increment the time - might be able to use SSE HADDs for this, but getting increment_count and reached_end
		// would probably be tricky
		sample_index += advance_array[i];
		if (sample_index >= loop_end_sample) {
			// Return to the loop start point
			real64 wrap_offset = std::remainder(sample_index - loop_start_sample, loop_end_sample - loop_start_sample);
			sample_index = loop_start_sample + wrap_offset;
		}
	}

	// We should only ever increment less than 4 times if we reach the end of the buffer
	if (increment_count < k_sse_block_elements) {
		wl_assert(inout_buffer_samples_remaining == 0);
	}

	return increment_count;
}
