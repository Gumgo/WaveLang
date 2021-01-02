#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_function_registration.h"
#include "engine/task_functions/sampler/fetch_sample.h"
#include "engine/task_functions/sampler/sample.h"
#include "engine/task_functions/sampler/sample_library.h"
#include "engine/task_functions/sampler/sampler_context.h"

class c_event_interface;

// $TODO add initial_delay parameter

// Extracts sample data
static void get_sample_time_data(
	const c_sample *sample,
	real64 &length_samples_out,
	real64 &loop_start_sample_out,
	real64 &loop_end_sample_out);

static void run_sampler(
	const c_sample_library *sample_library,
	const s_task_function_context &context,
	const char *name,
	const c_real_buffer *speed,
	c_real_buffer *result) {
	s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
	const c_sample *sample =
		sampler_context->get_sample_or_fail_gracefully(sample_library, result, context.event_interface, name);
	if (!sample || sampler_context->handle_reached_end(result)) {
		return;
	}

	wl_assert(!sample->is_looping());
	wl_assert(!sample->is_wavetable());

	real64 length_samples, loop_start_sample, loop_end_sample;
	get_sample_time_data(sample, length_samples, loop_start_sample, loop_end_sample);

	real32 stream_sample_rate = static_cast<real32>(context.sample_rate);
	real32 base_sample_rate = static_cast<real32>(sample->get_sample_rate());
	real32 advance_multiplier = base_sample_rate / stream_sample_rate;

	bool is_result_constant = speed->is_constant() && speed->get_constant() == 0.0f;
	size_t samples_written = 0;

	// Could SIMD-optimize, but the buffer advance logic is a bit tricky
	iterate_buffers<1, false>(context.buffer_size, speed, result,
		[&](size_t i, real32 speed_value, real32 &result_value) {
			speed_value = sanitize_inf_nan(speed_value);
			real32 advance = speed_value * advance_multiplier;
			real64 sample_index = sampler_context->increment_time(loop_end_sample, advance);
			result_value = fetch_sample(sample, sample_index);

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
		zero_type(result->get_data(), samples_remaining);

		// If samples_written is 0, the buffer is filled with 0
		result->set_is_constant(samples_written == 0);
	}
}

template<bool k_is_wavetable>
static void run_sampler_loop(
	const c_sample_library *sample_library,
	const s_task_function_context &context,
	const char *name,
	const c_real_buffer *speed,
	const c_real_buffer *phase,
	c_real_buffer *result) {
	s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
	const c_sample *sample =
		sampler_context->get_sample_or_fail_gracefully(sample_library, result, context.event_interface, name);
	if (!sample || sampler_context->handle_reached_end(result)) {
		return;
	}

	wl_assert(sample->is_looping());
	wl_assert(!sampler_context->reached_end);
	wl_assert(k_is_wavetable == sample->is_wavetable());

	real64 length_samples, loop_start_sample, loop_end_sample;
	get_sample_time_data(sample, length_samples, loop_start_sample, loop_end_sample);
	real64 phase_to_sample_offset_multiplier = loop_end_sample - loop_start_sample;

	real32 stream_sample_rate = static_cast<real32>(context.sample_rate);
	real32 base_sample_rate = static_cast<real32>(sample->get_sample_rate());
	real32 advance_multiplier = base_sample_rate / stream_sample_rate;

	bool is_result_constant = speed->is_constant() && speed->get_constant() == 0.0f && phase->is_constant();

	size_t samples_written = 0;
	size_t buffer_samples_remaining = context.buffer_size;

	// Could SIMD-optimize, but the buffer advance logic is a bit tricky
	iterate_buffers<1, false>(context.buffer_size, speed, phase, result,
		[&](size_t i, real32 speed_value, real32 phase_value, real32 &result_value) {
			speed_value = sanitize_inf_nan(speed_value);
			phase_value = sanitize_inf_nan(phase_value);
			real32 advance = speed_value * advance_multiplier;
			real64 sample_index = sampler_context->increment_time_looping(loop_start_sample, loop_end_sample, advance);

			real32 clamped_phase = clamp(phase_value, 0.0f, 1.0f);
			real64 sample_offset = clamped_phase * phase_to_sample_offset_multiplier;
			sample_index += sample_offset;

			if constexpr (k_is_wavetable) {
				result_value = fetch_wavetable_sample(
					sample,
					stream_sample_rate,
					base_sample_rate,
					speed_value,
					sample_index);
			} else {
				result_value = fetch_sample(sample, sample_index);
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
	real64 &length_samples_out,
	real64 &loop_start_sample_out,
	real64 &loop_end_sample_out) {
	length_samples_out = static_cast<real64>(sample->get_entry(0)->samples.get_count());
	loop_start_sample_out = static_cast<real64>(sample->get_loop_start());
	loop_end_sample_out = static_cast<real64>(sample->get_loop_end());
}

namespace sampler_task_functions {

	void *sampler_library_engine_initializer() {
		c_sample_library *sample_library = new c_sample_library();
		sample_library->initialize("./");
		return sample_library;
	}

	void sampler_library_engine_deinitializer(void *library_context) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(library_context);
		delete sample_library;
	}

	void sampler_library_tasks_pre_initializer(void *library_context) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(library_context);
		sample_library->clear_requested_samples();
	}

	void sampler_library_tasks_post_initializer(void *library_context) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(library_context);
		sample_library->update_loaded_samples();
	}

	size_t sampler_memory_query(const s_task_function_context &context) {
		return sizeof(s_sampler_context);
	}

	void sampler_initializer(
		const s_task_function_context &context,
		wl_task_argument(const char *, name),
		wl_task_argument(real32, channel)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		sampler_context->initialize_file(
			context.event_interface,
			sample_library,
			name,
			e_sample_loop_mode::k_none,
			false,
			channel);
	}

	void sampler_voice_initializer(const s_task_function_context &context) {
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		sampler_context->voice_initialize();
	}

	void sampler(
		const s_task_function_context &context,
		wl_task_argument(const char *, name),
		wl_task_argument(real32, channel),
		wl_task_argument(const c_real_buffer *, speed),
		wl_task_argument(c_real_buffer *, result)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		run_sampler(sample_library, context, name, speed, result);
	}

	void sampler_loop_initializer(
		const s_task_function_context &context,
		wl_task_argument(const char *, name),
		wl_task_argument(real32, channel),
		wl_task_argument(bool, bidi),
		wl_task_argument(const c_real_buffer *, phase)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		bool phase_shift_enabled = !phase->is_constant() || (clamp(phase->get_constant(), 0.0f, 1.0f) != 0.0f);
		e_sample_loop_mode loop_mode = bidi ? e_sample_loop_mode::k_bidi_loop : e_sample_loop_mode::k_loop;
		sampler_context->initialize_file(
			context.event_interface,
			sample_library,
			name,
			loop_mode,
			phase_shift_enabled,
			channel);
	}

	void sampler_loop(
		const s_task_function_context &context,
		wl_task_argument(const char *, name),
		wl_task_argument(real32, channel),
		wl_task_argument(bool, bidi),
		wl_task_argument(const c_real_buffer *, speed),
		wl_task_argument(const c_real_buffer *, phase),
		wl_task_argument(c_real_buffer *, result)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		const c_sample *sample = sample_library->get_sample(
			sampler_context->sample_handle,
			sampler_context->channel);
		run_sampler_loop<false>(sample_library, context, name, speed, phase, result);
	}

	void sampler_wavetable_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, harmonic_weights),
		wl_task_argument(const c_real_buffer *, phase)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		s_sampler_context *sampler_context = static_cast<s_sampler_context *>(context.task_memory);
		bool phase_shift_enabled = !phase->is_constant() || (clamp(phase->get_constant(), 0.0f, 1.0f) != 0.0f);
		sampler_context->initialize_wavetable(
			context.event_interface,
			sample_library,
			*harmonic_weights,
			phase_shift_enabled);
	}

	void sampler_wavetable(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, harmonic_weights),
		wl_task_argument(const c_real_buffer *, frequency),
		wl_task_argument(const c_real_buffer *, phase),
		wl_task_argument(c_real_buffer *, result)) {
		c_sample_library *sample_library = static_cast<c_sample_library *>(context.library_context);
		run_sampler_loop<true>(sample_library, context, "<generated_wavetable>", frequency, phase, result);
	}

	void scrape_task_functions() {
		static constexpr uint32 k_sampler_library_id = 3;
		wl_task_function_library(k_sampler_library_id, "sampler", 0)
			.set_engine_initializer(sampler_library_engine_initializer)
			.set_engine_deinitializer(sampler_library_engine_deinitializer)
			.set_tasks_pre_initializer(sampler_library_tasks_pre_initializer)
			.set_tasks_post_initializer(sampler_library_tasks_post_initializer);

		wl_task_function(0x8cd13477, "sampler")
			.set_function<sampler>()
			.set_memory_query<sampler_memory_query>()
			.set_initializer<sampler_initializer>()
			.set_voice_initializer<sampler_voice_initializer>();

		wl_task_function(0xdf906f59, "sampler_loop")
			.set_function<sampler_loop>()
			.set_memory_query<sampler_memory_query>()
			.set_initializer<sampler_loop_initializer>()
			.set_voice_initializer<sampler_voice_initializer>();

		wl_task_function(0x7429e1e3, "sampler_wavetable")
			.set_function<sampler_wavetable>()
			.set_memory_query<sampler_memory_query>()
			.set_initializer<sampler_wavetable_initializer>()
			.set_voice_initializer<sampler_voice_initializer>();

		wl_end_active_library_task_function_registration();
	}

}
