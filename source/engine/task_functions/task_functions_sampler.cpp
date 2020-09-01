#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/sample/sample.h"
#include "engine/sample/sample_library.h"
#include "engine/task_functions/task_functions_sampler.h"
#include "engine/task_functions/sampler/fetch_sample.h"
#include "engine/task_functions/sampler/sampler_context.h"

class c_event_interface;

// $TODO add initial_delay parameter

// Extracts sample data
static void get_sample_time_data(
	const c_sample *sample,
	real64 &out_length_samples,
	real64 &out_loop_start_sample,
	real64 &out_loop_end_sample);

static void run_sampler(
	const s_task_function_context &context,
	const char *name,
	const c_real_buffer *speed,
	c_real_buffer *result) {
	s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
	const c_sample *sample = context.sample_accessor->get_sample(sampler_context->sample_handle);
	if (sampler_context->handle_failed_sample(sample, result, context.event_interface, name) ||
		sampler_context->handle_reached_end(result)) {
		return;
	}

	wl_assert(sample->get_loop_mode() == e_sample_loop_mode::k_none);
	wl_assert(!sample->is_wavetable());

	real64 length_samples, loop_start_sample, loop_end_sample;
	get_sample_time_data(sample, length_samples, loop_start_sample, loop_end_sample);
	real64 phase_to_sample_offset_multiplier = loop_end_sample - loop_start_sample;

	real32 stream_sample_rate = static_cast<real32>(context.sample_rate);
	real32 sample_rate_0 = static_cast<real32>(sample->get_sample_rate());
	real32 advance_multiplier = sample_rate_0 / stream_sample_rate;

	bool is_result_constant = speed->is_constant() && speed->get_constant() == 0.0f;
	size_t samples_written = 0;

	// Could SIMD-optimize, but the buffer advance logic is a bit tricky
	iterate_buffers<1, false>(context.buffer_size, speed, result,
		[&](size_t i, real32 speed_value, real32 &result_value) {
			real32 advance = speed_value * advance_multiplier;
			real64 sample_index = sampler_context->increment_time(loop_end_sample, advance);
			result_value = fetch_sample(sample, sampler_context->channel, sample_index);

			samples_written++;
			return !is_result_constant && !sampler_context->reached_end;
		});

	if (is_result_constant) {
		if (samples_written == 0) {
			// We reached the end without writing any samples
			result->assign_constant(0.0f);
		} else {
			result->extend_constant();
		}
	} else {
		size_t samples_remaining = context.buffer_size - samples_written;

		// If the sample ended before the end of the buffer, fill the rest with 0
		memset(result->get_data(), 0, samples_remaining * sizeof(real32));

		// If samples_written is 0, the buffer is filled with 0
		result->set_is_constant(samples_written == 0);
	}
}

template<bool k_is_wavetable>
static void run_sampler_loop(
	const s_task_function_context &context,
	const char *name,
	const c_real_buffer *speed,
	const c_real_buffer *phase,
	c_real_buffer *result) {
	s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
	const c_sample *sample = context.sample_accessor->get_sample(sampler_context->sample_handle);
	if (sampler_context->handle_failed_sample(sample, result, context.event_interface, name) ||
		sampler_context->handle_reached_end(result)) {
		return;
	}

	// Bidi loops are preprocessed so at this point they act as normal loops
	wl_assert(sample->get_loop_mode() == e_sample_loop_mode::k_loop
		|| sample->get_loop_mode() == e_sample_loop_mode::k_bidi_loop);
	wl_assert(!sampler_context->reached_end);
	wl_assert(k_is_wavetable == sample->is_wavetable());

	real64 length_samples, loop_start_sample, loop_end_sample;
	get_sample_time_data(sample, length_samples, loop_start_sample, loop_end_sample);
	real64 phase_to_sample_offset_multiplier = loop_end_sample - loop_start_sample;

	real32 stream_sample_rate = static_cast<real32>(context.sample_rate);
	real32 sample_rate_0 = static_cast<real32>(sample->get_sample_rate());
	real32 advance_multiplier = sample_rate_0 / stream_sample_rate;

	bool is_result_constant = speed->is_constant() && speed->get_constant() == 0.0f && phase->is_constant();

	size_t samples_written = 0;
	size_t buffer_samples_remaining = context.buffer_size;

	// Could SIMD-optimize, but the buffer advance logic is a bit tricky
	iterate_buffers<1, false>(context.buffer_size, speed, phase, result,
		[&](size_t i, real32 speed_value, real32 phase_value, real32 &result_value) {
			real32 advance = speed_value * advance_multiplier;
			real64 sample_index = sampler_context->increment_time_looping(loop_start_sample, loop_end_sample, advance);

			real32 clamped_phase = clamp(phase_value, 0.0f, 1.0f);
			real64 sample_offset = clamped_phase * phase_to_sample_offset_multiplier;
			sample_index += sample_offset;

			if constexpr (k_is_wavetable) {
				result_value = fetch_wavetable_sample(
					sample,
					sampler_context->channel,
					stream_sample_rate,
					sample_rate_0,
					speed_value,
					sample_index);
			} else {
				result_value = fetch_sample(sample, sampler_context->channel, sample_index);
			}

			samples_written++;
			return !is_result_constant;
		});

	wl_assert(samples_written > 0);
	if (is_result_constant) {
		result->extend_constant();
	} else {
		wl_assert(samples_written == context.buffer_size);
		result->set_is_constant(false);
	}
}

static void get_sample_time_data(
	const c_sample *sample,
	real64 &out_length_samples,
	real64 &out_loop_start_sample,
	real64 &out_loop_end_sample) {
	uint32 first_sampling_frame = sample->get_first_sampling_frame();
	out_length_samples = static_cast<real64>(sample->get_sampling_frame_count());
	out_loop_start_sample = static_cast<real64>(sample->get_loop_start() - first_sampling_frame);
	out_loop_end_sample = static_cast<real64>(sample->get_loop_end() - first_sampling_frame);
}

namespace sampler_task_functions {

	size_t sampler_memory_query(const s_task_function_context &context) {
		return sizeof(s_sampler_context);
	}

	void sampler_initializer(
		const s_task_function_context &context,
		const char *name,
		real32 channel) {
		static_cast<s_sampler_context *>(context.task_memory)->initialize_file(
			context.event_interface,
			context.sample_requester,
			name, e_sample_loop_mode::k_none, false, channel);
	}

	void sampler_voice_initializer(const s_task_function_context &context) {
		static_cast<s_sampler_context *>(context.task_memory)->voice_initialize();
	}

	void sampler(
		const s_task_function_context &context,
		const char *name,
		const c_real_buffer *speed,
		c_real_buffer *result) {
		run_sampler(context, name, speed, result);
	}

	void sampler_loop_initializer(
		const s_task_function_context &context,
		const char *name,
		real32 channel,
		bool bidi,
		const c_real_buffer *phase) {
		bool phase_shift_enabled = !phase->is_constant() || (clamp(phase->get_constant(), 0.0f, 1.0f) != 0.0f);
		e_sample_loop_mode loop_mode = bidi ? e_sample_loop_mode::k_bidi_loop : e_sample_loop_mode::k_loop;
		static_cast<s_sampler_context *>(context.task_memory)->initialize_file(
			context.event_interface,
			context.sample_requester,
			name,
			loop_mode,
			phase_shift_enabled,
			channel);
	}

	void sampler_loop(
		const s_task_function_context &context,
		const char *name,
		const c_real_buffer *speed,
		const c_real_buffer *phase,
		c_real_buffer *result) {
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		const c_sample *sample = context.sample_accessor->get_sample(sampler_context->sample_handle);
		if (sample->is_wavetable()) {
			// $TODO remove this branch, it only exists due to the __native_sin, etc. named wavetable hacks.
			run_sampler_loop<true>(context, name, speed, phase, result);
		} else {
			run_sampler_loop<false>(context, name, speed, phase, result);
		}
	}

	void sampler_wavetable_initializer(
		const s_task_function_context &context,
		c_real_constant_array harmonic_weights,
		real32 sample_count,
		const c_real_buffer *phase) {
		bool phase_shift_enabled = !phase->is_constant() || (clamp(phase->get_constant(), 0.0f, 1.0f) != 0.0f);
		static_cast<s_sampler_context *>(context.task_memory)->initialize_wavetable(
			context.event_interface,
			context.sample_requester,
			harmonic_weights,
			sample_count,
			phase_shift_enabled);
	}

	void sampler_wavetable(
		const s_task_function_context &context,
		const c_real_buffer *frequency,
		const c_real_buffer *phase,
		c_real_buffer *result) {
		run_sampler_loop<true>(context, "<generated_wavetable>", frequency, phase, result);
	}

}
