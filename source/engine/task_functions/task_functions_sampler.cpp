#include "engine/task_functions/task_functions_sampler.h"
#include "execution_graph/native_modules/native_modules_sampler.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/sample/sample.h"
#include "engine/sample/sample_library.h"
#include "engine/task_functions/task_functions_sampler_sinc_window.h"
#include "engine/events/event_interface.h"
#include "engine/events/event_data_types.h"

// $TODO switch to s_static_array
// $TODO tracking time with floats in seconds seems imprecise... track in samples instead maybe?
// $TODO add initial_delay parameter

static const uint32 k_task_functions_sampler_library_id = 2;

static const s_task_function_uid k_task_function_sampler_in_out_uid = s_task_function_uid::build(k_task_functions_sampler_library_id, 0);
static const s_task_function_uid k_task_function_sampler_inout_uid = s_task_function_uid::build(k_task_functions_sampler_library_id, 1);

static const s_task_function_uid k_task_function_sampler_loop_in_in_out_uid = s_task_function_uid::build(k_task_functions_sampler_library_id, 10);
static const s_task_function_uid k_task_function_sampler_loop_inout_in_uid = s_task_function_uid::build(k_task_functions_sampler_library_id, 11);
static const s_task_function_uid k_task_function_sampler_loop_in_inout_uid = s_task_function_uid::build(k_task_functions_sampler_library_id, 12);

struct s_buffer_operation_sampler {
	static size_t query_memory();
	static void initialize(c_event_interface *event_interface, c_sample_library_requester *sample_requester,
		s_buffer_operation_sampler *context, const char *sample, e_sample_loop_mode loop_mode, bool phase_shift_enabled,
		real32 channel);
	static void voice_initialize(s_buffer_operation_sampler *context);

	static void in_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, c_real_buffer_out out);
	static void inout(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out);

	// Constant versions
	static void const_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, real32 speed, c_real_buffer_out out);

	static void loop_in_in_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate,
		c_real_buffer_or_constant_in speed, c_real_buffer_or_constant_in phase, c_real_buffer_out out);
	static void loop_inout_in(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out, c_real_buffer_or_constant_in phase);
	static void loop_in_inout(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, c_real_buffer_inout phase_out);

	// Constant versions
	static void loop_const_in_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate,
		real32 speed, c_real_buffer_or_constant_in phase, c_real_buffer_out out);
	static void loop_in_const_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate,
		c_real_buffer_or_constant_in speed, real32 phase, c_real_buffer_out out);
	static void loop_const_const_out(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate,
		real32 speed, real32 phase, c_real_buffer_out out);
	static void loop_inout_const(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out, real32 phase);
	static void loop_const_inout(
		c_event_interface *event_interface, const char *sample_name,
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, real32 speed, c_real_buffer_inout phase_out);

	uint32 sample_handle;
	uint32 channel;
	real32 time;
	bool reached_end;
	bool sample_failure_reported;
};

// To avoid rewriting the algorithm in multiple variations, we create a base template class and implement the variants
// using https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern.

template<typename t_derived>
struct s_sampler_algorithm {
	// "Overridden" methods:

	//c_real_buffer_out get_out() const;
	//real32 *get_out_pointer() const;

	//void begin_loop();
	//bool is_valid() const;
	//void advance();
	//void run_loop();

	//c_real32_4 get_advance() const;
	//c_real32_4 get_speed_adjusted_sample_rate_0() const;

	// Fields available to derived implementations:
	real32 stream_sample_rate;
	real32 sample_rate_0;

	// Utility functions for subclasses
	static c_real32_4 compute_advance(const c_real32_4 &speed_val, real32 stream_sample_rate) {
		return speed_val / c_real32_4(stream_sample_rate);
	}

	static c_real32_4 compute_speed_adjusted_sample_rate_0(const c_real32_4 &speed_val, real32 sample_rate_0) {
		return c_real32_4(sample_rate_0) * speed_val;
	}

	void run(c_event_interface *event_interface, const char *sample_name, c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context, size_t buffer_size, uint32 sample_rate) {
		// Allows us to call "overridden" methods without inheritance
		t_derived *this_derived = static_cast<t_derived *>(this);

		const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
		if (handle_failed_sample(sample, context->channel, this_derived->get_out(),
			event_interface, sample_name, context->sample_failure_reported) ||
			handle_reached_end(context->reached_end, this_derived->get_out())) {
			return;
		}

		wl_assert(sample->get_loop_mode() == k_sample_loop_mode_none);

		bool is_mipmap = sample->is_mipmap();
		const c_sample *sample_0 = is_mipmap ? sample->get_mipmap_sample(0) : sample;
		real32 length, loop_start_time, loop_end_time;
		get_sample_time_data(sample_0, length, loop_start_time, loop_end_time);

		stream_sample_rate = static_cast<real32>(sample_rate);
		sample_rate_0 = static_cast<real32>(sample_0->get_sample_rate());

		size_t samples_written = 0;
		size_t buffer_samples_remaining = buffer_size;
		for (this_derived->begin_loop(); this_derived->is_valid(); this_derived->advance()) {
			this_derived->run_loop();
			real32 *out_ptr = this_derived->get_out_pointer();
			c_real32_4 advance = this_derived->get_advance();
			c_real32_4 speed_adjusted_sample_rate_0 = this_derived->get_speed_adjusted_sample_rate_0();

			// Increment the time first, storing each intermediate time value
			ALIGNAS_SSE real32 time[k_sse_block_elements];
			size_t increment_count = increment_time(length, context, advance, buffer_samples_remaining, time);
			wl_assert(increment_count > 0);

			if (is_mipmap) {
				fetch_mipmapped_samples(sample, context->channel, stream_sample_rate, speed_adjusted_sample_rate_0,
					increment_count, time, out_ptr);
			} else {
				for (size_t i = 0; i < increment_count; i++) {
					out_ptr[i] = fetch_sample(sample, context->channel, time[i]);
				}
			}

			samples_written += increment_count;
			if (context->reached_end) {
				break;
			}
		}

		// If the sample ended before the end of the buffer, fill the rest with 0
		size_t samples_remaining = buffer_size - samples_written;
		memset(this_derived->get_out_pointer(), 0, samples_remaining * sizeof(real32));
		this_derived->get_out()->set_constant(false);
	}
};

template<typename t_derived>
struct s_sampler_loop_algorithm {
	// "Overridden" methods:

	//c_real_buffer_out get_out() const;
	//real32 *get_out_pointer() const;

	//void begin_loop();
	//bool is_valid() const;
	//void advance();
	//void run_loop();

	//c_real32_4 get_advance() const;
	//c_real32_4 get_speed_adjusted_sample_rate_0() const;
	//c_real32_4 get_time_offset() const;

	// Fields available to derived implementations:
	real32 stream_sample_rate;
	real32 sample_rate_0;
	c_real32_4 phase_time_multiplier;

	// Utility functions for subclasses
	static c_real32_4 compute_advance(const c_real32_4 &speed_val, real32 stream_sample_rate) {
		return speed_val / c_real32_4(stream_sample_rate);
	}

	static c_real32_4 compute_speed_adjusted_sample_rate_0(const c_real32_4 &speed_val, real32 sample_rate_0) {
		return c_real32_4(sample_rate_0) * speed_val;
	}

	static c_real32_4 compute_time_offset(const c_real32_4 &phase_val, const c_real32_4 &phase_time_multiplier) {
		c_real32_4 clamped_phase = min(max(phase_val, c_real32_4(0.0f)), c_real32_4(1.0f));
		return clamped_phase * phase_time_multiplier;
	}

	void run(c_event_interface *event_interface, const char *sample_name, c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context, size_t buffer_size, uint32 sample_rate) {
		// Allows us to call "overridden" methods without inheritance
		t_derived *this_derived = static_cast<t_derived *>(this);

		const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
		if (handle_failed_sample(sample, context->channel, this_derived->get_out(),
			event_interface, sample_name, context->sample_failure_reported)) {
			return;
		}

		// Bidi loops are preprocessed so at this point they act as normal loops
		wl_assert(sample->get_loop_mode() != k_sample_loop_mode_none);
		wl_assert(!context->reached_end);

		bool is_mipmap = sample->is_mipmap();
		const c_sample *sample_0 = is_mipmap ? sample->get_mipmap_sample(0) : sample;
		real32 length, loop_start_time, loop_end_time;
		get_sample_time_data(sample_0, length, loop_start_time, loop_end_time);
		phase_time_multiplier = c_real32_4(loop_end_time - loop_start_time);

		stream_sample_rate = static_cast<real32>(sample_rate);
		sample_rate_0 = static_cast<real32>(sample_0->get_sample_rate());

		size_t buffer_samples_remaining = buffer_size;
		for (this_derived->begin_loop(); this_derived->is_valid(); this_derived->advance()) {
			this_derived->run_loop();
			real32 *out_ptr = this_derived->get_out_pointer();
			c_real32_4 advance = this_derived->get_advance();
			c_real32_4 speed_adjusted_sample_rate_0 = this_derived->get_speed_adjusted_sample_rate_0();
			c_real32_4 time_offset = this_derived->get_time_offset();

			// Increment the time first, storing each intermediate time value
			ALIGNAS_SSE real32 time[k_sse_block_elements];
			size_t increment_count = increment_time_looping(loop_start_time, loop_end_time, context, advance,
				buffer_samples_remaining, time);
			wl_assert(increment_count > 0);

			c_real32_4 phased_time = c_real32_4(time) + time_offset;
			phased_time.store(time);

			if (is_mipmap) {
				fetch_mipmapped_samples(sample, context->channel, stream_sample_rate, speed_adjusted_sample_rate_0,
					increment_count, time, out_ptr);
			} else {
				for (size_t i = 0; i < increment_count; i++) {
					out_ptr[i] = fetch_sample(sample, context->channel, time[i]);
				}
			}
		}

		this_derived->get_out()->set_constant(false);
	}
};

// Common utility functions used in all versions of the sampler:

// Fills the output buffer with 0s if the sample failed to load or if the channel is invalid
static bool handle_failed_sample(const c_sample *sample, uint32 channel, c_real_buffer_out out,
	c_event_interface *event_interface, const char *sample_name, bool &inout_failure_reported);

// Fills the output buffer with 0s if the end has been reached
static bool handle_reached_end(bool reached_end, c_real_buffer_out out);

// Converts sample data (frames) to timing data (seconds)
static void get_sample_time_data(const c_sample *sample,
	real32 &out_length, real32 &out_loop_start, real32 &out_loop_end);

// Increments the sample's current time up to 4 times based on the speed provided and returns the number of times
// incremented, which can be less than 4 if the sample ends or if the end of the buffer is reached. The time before each
// increment is stored in out_time. Handles looping/wrapping.
static size_t increment_time(real32 length, s_buffer_operation_sampler *context,
	const c_real32_4 &advance, size_t &inout_buffer_samples_remaining, real32(&out_time)[k_sse_block_elements]);
static size_t increment_time_looping(real32 loop_start_time, real32 loop_end_time, s_buffer_operation_sampler *context,
	const c_real32_4 &advance, size_t &inout_buffer_samples_remaining, real32(&out_time)[k_sse_block_elements]);

// Fast approximation to log2. Result is divided into integer and fraction parts.
static c_int32_4 fast_log2(const c_real32_4 &x, c_real32_4 &out_frac);

// Returns the two mipmap levels to blend between for a given speed-adjusted sample rate, as well as the blend ratio. If
// the two returned mip levels are the same, no blending is required.
static void choose_mipmap_levels(
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, uint32 mip_count,
	c_int32_4 &out_mip_index_a, c_int32_4 &out_mip_index_b, c_real32_4 &out_mip_blend_ratio);

// Calculates the given number of mipmapped samples (max 4) and stores them at the given location. Note: if less than 4
// samples are requested, the remaining ones up to 4 will be filled with garbage data; it is expected that these will
// later be overwritten, or are past the end of the buffer but exist for padding.
static void fetch_mipmapped_samples(const c_sample *sample, uint32 channel,
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, size_t count,
	const real32(&time)[k_sse_block_elements], real32 *out_ptr);

// Calculates the interpolated sample at the given time. The input sample should not be a mipmap.
static real32 fetch_sample(const c_sample *sample, uint32 channel, real32 time);

// Treats a and b as a contiguous block of 8 values and shifts left by the given amount; returns only the first 4
static c_real32_4 shift_left_1(const c_real32_4 &a, const c_real32_4 &b);
static c_real32_4 shift_left_2(const c_real32_4 &a, const c_real32_4 &b);
static c_real32_4 shift_left_3(const c_real32_4 &a, const c_real32_4 &b);

size_t s_buffer_operation_sampler::query_memory() {
	return sizeof(s_buffer_operation_sampler);
}

void s_buffer_operation_sampler::initialize(c_event_interface *event_interface,
	c_sample_library_requester *sample_requester,
	s_buffer_operation_sampler *context, const char *sample, e_sample_loop_mode loop_mode, bool phase_shift_enabled,
	real32 channel) {
	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));

	context->sample_handle = sample_requester->request_sample(sample, loop_mode, phase_shift_enabled);
	if (channel < 0.0f ||
		std::floor(channel) != channel) {
		context->sample_handle = c_sample_library::k_invalid_handle;
		event_interface->submit(EVENT_ERROR << "Invalid sample channel '" << channel << "'");
	}
	context->channel = static_cast<uint32>(channel);
	context->time = 0.0f;
	context->reached_end = false;
}

void s_buffer_operation_sampler::voice_initialize(s_buffer_operation_sampler *context) {
	context->time = 0.0f;
	context->reached_end = false;
}

void s_buffer_operation_sampler::in_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, c_real_buffer_out out) {
	validate_buffer(speed);
	validate_buffer(out);

	if (speed.is_constant()) {
		// If speed is constant, simply use the constant-speed version
		real32 speed_constant = speed.get_constant();
		const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed_constant, out);
		return;
	}

	struct s_sampler_subalgorithm : public s_sampler_algorithm<s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;

		s_sampler_subalgorithm(size_t buffer_size, c_real_buffer_in speed, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_ptr = speed->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_sse_block_elements; speed_ptr += k_sse_block_elements; }

		void run_loop() {
			c_real32_4 speed_val(speed_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
	};

	s_sampler_subalgorithm(buffer_size, speed.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::inout(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out) {
	validate_buffer(speed_out);

	if (speed_out->is_constant()) {
		// If speed is constant, simply use the constant-speed version
		// We can store the first value of the buffer and then convert it to a pure output buffer
		real32 speed_constant = *speed_out->get_data<real32>();
		const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed_constant, speed_out);
		return;
	}

	struct s_sampler_subalgorithm : public s_sampler_algorithm<s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;

		s_sampler_subalgorithm(size_t buffer_size, c_real_buffer_inout speed_out) {
			out = speed_out;
			speed_out_ptr = out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_sse_block_elements);
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() { speed_out_ptr += k_sse_block_elements; }

		void run_loop() {
			c_real32_4 speed_val(speed_out_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
	};

	s_sampler_subalgorithm(buffer_size, speed_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::const_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, real32 speed, c_real_buffer_out out) {
	validate_buffer(out);

	struct s_sampler_subalgorithm : public s_sampler_algorithm<s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		real32 speed_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;

		s_sampler_subalgorithm(size_t buffer_size, real32 speed, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_val = speed;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(c_real32_4(speed_val), stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_sse_block_elements; }
		void run_loop() {}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
	};

	s_sampler_subalgorithm(buffer_size, speed, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_in_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, c_real_buffer_or_constant_in phase,
	c_real_buffer_out out) {
	validate_buffer(speed);
	validate_buffer(phase);
	validate_buffer(out);

	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_const_const_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase_constant, out);
		} else {
			loop_const_in_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase, out);
		}
		return;
	} else {
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_in_const_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed, phase_constant, out);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			c_real_buffer_in speed, c_real_buffer_in phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_ptr = speed->get_data<real32>();
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_sse_block_elements;
			speed_ptr += k_sse_block_elements;
			phase_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 speed_val(speed_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
			c_real32_4 phase_val(phase_ptr);
			time_offset_val = compute_time_offset(phase_val, phase_time_multiplier);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_inout_in(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out, c_real_buffer_or_constant_in phase) {
	validate_buffer(phase);
	validate_buffer(speed_out);

	// Identify constant optimizations
	if (speed_out->is_constant()) {
		real32 speed_constant = *speed_out->get_data<real32>();
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_const_const_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase_constant, speed_out);
		} else {
			loop_const_in_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase, speed_out);
		}
		return;
	} else {
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_inout_const(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_out, phase_constant);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			c_real_buffer_inout speed_out, c_real_buffer_in phase) {
			out = speed_out;
			speed_out_ptr = speed_out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_sse_block_elements);
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() {
			speed_out_ptr += k_sse_block_elements;
			phase_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 speed_val(speed_out_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
			c_real32_4 phase_val(phase_ptr);
			time_offset_val = compute_time_offset(phase_val, phase_time_multiplier);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed_out, phase.get_buffer()).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_inout(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, c_real_buffer_inout phase_out) {
	validate_buffer(speed);
	validate_buffer(phase_out);

	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		if (phase_out->is_constant()) {
			real32 phase_constant = *phase_out->get_data<real32>();
			loop_const_const_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase_constant, phase_out);
		} else {
			loop_const_inout(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed_constant, phase_out);
		}
		return;
	} else {
		if (phase_out->is_constant()) {
			real32 phase_constant = *phase_out->get_data<real32>();
			loop_in_const_out(event_interface, sample_name, sample_accessor,
				context, buffer_size, sample_rate, speed, phase_constant, phase_out);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *phase_out_ptr;
		real32 *phase_out_ptr_end;
		const real32 *speed_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			c_real_buffer_in speed, c_real_buffer_inout phase_out) {
			out = phase_out;
			phase_out_ptr = phase_out->get_data<real32>();
			phase_out_ptr_end = phase_out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_ptr = speed->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return phase_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return phase_out_ptr < phase_out_ptr_end; }
		void advance() {
			phase_out_ptr += k_sse_block_elements;
			speed_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 speed_val(speed_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
			c_real32_4 phase_val(phase_out_ptr);
			time_offset_val = compute_time_offset(phase_val, phase_time_multiplier);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_const_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_or_constant_in speed, real32 phase, c_real_buffer_out out) {
	validate_buffer(out);
	validate_buffer(speed);

	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		loop_const_const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed_constant, phase, out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			c_real_buffer_in speed, real32 phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_ptr = speed->get_data<real32>();
			phase_val = phase;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			time_offset_val = compute_time_offset(c_real32_4(phase_val), phase_time_multiplier);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_sse_block_elements;
			speed_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 speed_val(speed_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_inout_const(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_real_buffer_inout speed_out, real32 phase) {
	validate_buffer(speed_out);

	// Identify constant optimizations
	if (speed_out->is_constant()) {
		real32 speed_constant = *speed_out->get_data<real32>();
		loop_const_const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed_constant, phase, speed_out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			c_real_buffer_inout speed_out, real32 phase) {
			out = speed_out;
			speed_out_ptr = speed_out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_sse_block_elements);
			phase_val = phase;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {
			time_offset_val = compute_time_offset(c_real32_4(phase_val), phase_time_multiplier);
		}

		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() {
			speed_out_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 speed_val(speed_out_ptr);
			advance_val = compute_advance(speed_val, stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed_out, phase).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_in_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, real32 speed, c_real_buffer_or_constant_in phase, c_real_buffer_out out) {
	validate_buffer(phase);
	validate_buffer(out);

	// Identify constant optimizations
	if (phase.is_constant()) {
		real32 phase_constant = phase.get_constant();
		loop_const_const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed, phase_constant, out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		real32 speed_val;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			real32 speed, c_real_buffer_in phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_val = speed;
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(c_real32_4(speed_val), stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_sse_block_elements;
			phase_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 phase_val(phase_ptr);
			time_offset_val = compute_time_offset(phase_val, phase_time_multiplier);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_inout(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, real32 speed, c_real_buffer_inout phase_out) {
	validate_buffer(phase_out);

	// Identify constant optimizations
	if (phase_out->is_constant()) {
		real32 phase_constant = *phase_out->get_data<real32>();
		loop_const_const_out(event_interface, sample_name, sample_accessor,
			context, buffer_size, sample_rate, speed, phase_constant, phase_out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *phase_out_ptr;
		real32 *phase_out_ptr_end;
		real32 speed_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			real32 speed, c_real_buffer_inout phase_out) {
			out = phase_out;
			phase_out_ptr = phase_out->get_data<real32>();
			phase_out_ptr_end = phase_out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_val = speed;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return phase_out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(c_real32_4(speed_val), stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
		}

		bool is_valid() const { return phase_out_ptr < phase_out_ptr_end; }
		void advance() {
			phase_out_ptr += k_sse_block_elements;
		}

		void run_loop() {
			c_real32_4 phase_val(phase_out_ptr);
			time_offset_val = compute_time_offset(phase_val, phase_time_multiplier);
		}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_const_out(
	c_event_interface *event_interface, const char *sample_name,
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, real32 speed, real32 phase, c_real_buffer_out out) {
	validate_buffer(out);

	struct s_sampler_loop_subalgorithm : public s_sampler_loop_algorithm<s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		real32 speed_val;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		c_real32_4 time_offset_val;

		s_sampler_loop_subalgorithm(size_t buffer_size,
			real32 speed, real32 phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			speed_val = speed;
			phase_val = phase;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(c_real32_4(speed_val), stream_sample_rate);
			speed_adjusted_sample_rate_0_val = compute_speed_adjusted_sample_rate_0(speed_val, sample_rate_0);
			time_offset_val = compute_time_offset(c_real32_4(phase_val), phase_time_multiplier);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_sse_block_elements; }
		void run_loop() {}

		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed_adjusted_sample_rate_0() const { return speed_adjusted_sample_rate_0_val; }
		c_real32_4 get_time_offset() const { return time_offset_val; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

static bool handle_failed_sample(const c_sample *sample, uint32 channel, c_real_buffer_out out,
	c_event_interface *event_interface, const char *sample_name, bool &inout_failure_reported) {
	// If the sample failed, fill with 0
	if (!sample) {
		*out->get_data<real32>() = 0.0f;
		out->set_constant(true);

		if (!inout_failure_reported) {
			inout_failure_reported = true;
			event_interface->submit(EVENT_ERROR << "Failed to load sample '" << c_dstr(sample_name) << "'");
		}
	}

	return sample == nullptr;
}

static bool handle_reached_end(bool reached_end, c_real_buffer_out out) {
	if (reached_end) {
		*out->get_data<real32>() = 0.0f;
		out->set_constant(true);
	}

	return reached_end;
}

static void get_sample_time_data(const c_sample *sample,
	real32 &out_length, real32 &out_loop_start, real32 &out_loop_end) {
	uint32 first_sampling_frame = sample->get_first_sampling_frame();
	c_real32_4 time_data(
		static_cast<real32>(sample->get_sampling_frame_count()),
		static_cast<real32>(sample->get_loop_start() - first_sampling_frame),
		static_cast<real32>(sample->get_loop_end() - first_sampling_frame),
		0.0f);
	time_data = time_data / c_real32_4(static_cast<real32>(sample->get_sample_rate()));
	ALIGNAS_SSE real32 time_data_array[k_sse_block_elements];
	time_data.store(time_data_array);

	out_loop_start = time_data_array[0];
	out_loop_start = time_data_array[1];
	out_loop_end = time_data_array[2];
}

static size_t increment_time(real32 length, s_buffer_operation_sampler *context,
	const c_real32_4 &advance, size_t &inout_buffer_samples_remaining, real32(&out_time)[k_sse_block_elements]) {
	// We haven't vectorized this, so extract the reals
	ALIGNAS_SSE real32 advance_array[k_sse_block_elements];
	advance.store(advance_array);

	wl_assert(!context->reached_end);
	ZERO_STRUCT(&out_time);

	// We will return the actual number of times incremented so we don't over-sample
	size_t increment_count = 0;
	for (uint32 i = 0; i < k_sse_block_elements && inout_buffer_samples_remaining > 0; i++) {
		increment_count++;
		inout_buffer_samples_remaining--;
		out_time[i] = context->time;

		// Increment the time - might be able to use SSE HADDs for this, but getting increment_count and reached_end
		// would probably be tricky
		context->time += advance_array[i];
		if (context->time >= length) {
			context->reached_end = true;
			break;
		}
	}

	// We should only ever increment less than 4 times if we reach the end of the buffer or the end of the sample
	if (increment_count < k_sse_block_elements) {
		wl_assert(context->reached_end || inout_buffer_samples_remaining == 0);
	}

	return increment_count;
}

static size_t increment_time_looping(real32 loop_start_time, real32 loop_end_time, s_buffer_operation_sampler *context,
	const c_real32_4 &advance, size_t &inout_buffer_samples_remaining, real32(&out_time)[k_sse_block_elements]) {
	// We haven't vectorized this, so extract the reals
	ALIGNAS_SSE real32 advance_array[k_sse_block_elements];
	advance.store(advance_array);

	wl_assert(!context->reached_end);
	ZERO_STRUCT(&out_time);

	// We will return the actual number of times incremented so we don't over-sample
	size_t increment_count = 0;
	for (uint32 i = 0; i < k_sse_block_elements && inout_buffer_samples_remaining > 0; i++) {
		increment_count++;
		inout_buffer_samples_remaining--;
		out_time[i] = context->time;

		// Increment the time - might be able to use SSE HADDs for this, but getting increment_count and reached_end
		// would probably be tricky
		context->time += advance_array[i];
		if (context->time >= loop_end_time) {
			// Return to the loop start point
			real32 wrap_offset = std::remainder(context->time - loop_start_time, loop_end_time - loop_start_time);
			context->time = loop_start_time + wrap_offset;
		}
	}

	// We should only ever increment less than 4 times if we reach the end of the buffer
	if (increment_count < k_sse_block_elements) {
		wl_assert(inout_buffer_samples_remaining == 0);
	}

	return increment_count;
}

// Splits into integer and fraction
static c_int32_4 fast_log2(const c_real32_4 &x, c_real32_4 &out_frac) {
	// x = M * 2^E
	// log2(x) = log2(M) + log2(2^E)
	//         = log2(M) + E
	//         = log2(M) + E
	// Since the mantissa is in the range [1,2) (for non-denormalized floats) which maps continuously to [0,1), we split
	// our result such that:
	// integer_part    = E
	// fractional_part = log2(M)
	// We approximate fractional_part using a 2nd-degree polynomial; see find_fast_log2_coefficients for details.

	// Cast to raw bits
	c_int32_4 x_e = x.int32_4_from_bits();
	// Shift and mask to obtain the exponent; subtract 127 for bias
	// [sign 1][exp 8][mant 23]
	x_e = (x_e.shift_right_unsigned(23) & c_int32_4(0x000000ff)) - c_int32_4(127);

	// We now want to set the exponent to 0 so that we obtain the float value M * 2^0 = M. With bias, this is 127.
	c_int32_4 x_m_bits = x.int32_4_from_bits();
	// Clear the sign and exponent, then OR in 127
	x_m_bits = (x_m_bits & 0x007fffff) | 0x3f800000;

	// Apply the polynomial approximation
	c_real32_4 x_m = x_m_bits.real32_4_from_bits();
	out_frac = (c_real32_4(-0.346555382f) * x_m + c_real32_4(2.03966618f)) * x_m + c_real32_4(-1.69311082f);
	return x_e;
}

#if 0
void test_fast_log2() {
	for (real32 f = 0.0f; f < 10.0f; f += 0.05f) {
		c_real32_4 x(f);
		c_real32_4 frac_part;
		c_int32_4 int_part = fast_log2(x, frac_part);

		ALIGNAS_SSE real32 frac_out[4];
		ALIGNAS_SSE int32 int_out[4];

		frac_part.store(frac_out);
		int_part.store(int_out);

		real32 log2_approx = static_cast<real32>(int_out[0]) + frac_out[0];
		real32 log2_true = std::log2(f);

		real32 diff = std::abs(log2_approx - log2_true);
		std::cout << diff << "\n";
	}
}
#endif // 0

static void choose_mipmap_levels(
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, uint32 mip_count,
	c_int32_4 &out_mip_index_a, c_int32_4 &out_mip_index_b, c_real32_4 &out_mip_blend_ratio) {
	// We wish to find a mipmap level to sample from which avoids exceeding the nyquist frequency. We can't just return
	// a single mip level though, or pitch-bending would cause "pops" when we switched between levels. Instead, we
	// pick two mip levels, a and b, such that level a is the lowest mip level which has no frequencies exceeding the
	// stream's nyquist frequency, and b is the level above that, and blend between them.

	// Our mip levels are set up such that a mip level with a sample rate X is guaranteed to have no frequencies above
	// (X/2)hz. Therefore, we search for two mip levels a and b with sample rates surrounding half the stream's sample
	// rate, which is equivalent to finding the two mip levels directly below the stream's sample rate.
	c_real32_4 sample_stream_ratio = speed_adjusted_sample_rate_0 / c_real32_4(stream_sample_rate * 0.5f);

	// Ratios below 1 mean that the stream's sample rate is fast enough for the highest mip level to always be valid.
	// Therefore, limit the ratio to 1.
	sample_stream_ratio = max(sample_stream_ratio, c_real32_4(1.0f));

	// Taking the log2 of the ratio will give us the first mip index with a sample rate of at least that of the stream
	// in the integer component, and the fractional component will give us the interpolation factor between that mip
	// level and the next one, guaranteed to have a sample rate less than that of the stream. However, in this case,
	// since we pre-halved input sample rate of the stream, the mip index returned is actually safe to directly use.
	// Since we clamped the ratio to 1, this will always be at least 0.
	out_mip_index_a = fast_log2(sample_stream_ratio, out_mip_blend_ratio);

	// Clamp to the number of mips actually available
	c_int32_4 max_mip_index(cast_integer_verify<int32>(mip_count - 1));
	out_mip_index_a = min(out_mip_index_a, max_mip_index);
	out_mip_index_b = min(out_mip_index_a + c_int32_4(1), max_mip_index);
}

static void fetch_mipmapped_samples(const c_sample *sample, uint32 channel,
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, size_t count,
	const real32(&time)[k_sse_block_elements], real32 *out_ptr) {
	wl_assert(count > 0 && count <= k_sse_block_elements);

	// Compute the mip indices and blend factors for these 4 samples (if we incremented less than 4 times, some
	// are unused)
	c_int32_4 mip_index_a;
	c_int32_4 mip_index_b;
	c_real32_4 mip_blend_ratio;
	choose_mipmap_levels(stream_sample_rate, speed_adjusted_sample_rate_0, sample->get_mipmap_count(),
		mip_index_a, mip_index_b, mip_blend_ratio);

	ALIGNAS_SSE int32 mip_index_a_array[k_sse_block_elements];
	ALIGNAS_SSE int32 mip_index_b_array[k_sse_block_elements];
	mip_index_a.store(mip_index_a_array);
	mip_index_b.store(mip_index_b_array);

	ALIGNAS_SSE real32 samples_a_array[k_sse_block_elements];
	ALIGNAS_SSE real32 samples_b_array[k_sse_block_elements];

	for (size_t i = 0; i < count; i++) {
		if (mip_index_a_array[i] == mip_index_b_array[i]) {
			samples_a_array[i] = samples_b_array[i] =
				fetch_sample(sample->get_mipmap_sample(mip_index_a_array[i]), channel, time[i]);
		} else {
			samples_a_array[i] = fetch_sample(sample->get_mipmap_sample(mip_index_a_array[i]), channel, time[i]);
			samples_b_array[i] = fetch_sample(sample->get_mipmap_sample(mip_index_b_array[i]), channel, time[i]);
		}
	}

	// Interpolate and write to the array. If increment_count < 4, we will be writing some extra (garbage
	// values) to the array, but that's okay - it's either greater than buffer size, or the remainder if the
	// buffer will be filled with 0 because we've reached the end of the loop.
	c_real32_4 samples_a(samples_a_array);
	c_real32_4 samples_b(samples_b_array);
	c_real32_4 samples_interpolated = samples_a + (samples_b - samples_a) * mip_blend_ratio;
	samples_interpolated.store(out_ptr);
}

/*
Windowed sinc sampling chart
Serves as a visual representation for why our padding size must be (window size)/2
window size = 5
padding = (5/2) = 2
+ = sample
--- = time between samples
s = first sampling sample
e = first non-sampling sample (i.e. first end padding sample)
A = earliest time we are allowed to sample
B = latest time we are allowed to sample from (B can get arbitrarily close to e)
x = samples required for computing A or B
s														e
+---+---A---+---+---+---+---+---+---+---+---+---+---+---+---+--B+---+---
+-------x-------+									+-------x-------+
+-------x-------+									+-------x-------+
+-------x-------+									+-------x-------+
+-------x-------+									+-------x-------+

Consider computing the following interpolated sample S:

+-------+-------+-------+-----S-+-------+-------+-------+-------+
+---------------A-------------a-+
+---------------B-----b---------+
+-------------c-C---------------+
+-----d---------D---------------+

The value of S is: samples[A]*sinc[a-A] + samples[B]*sinc[b-B] + samples[C]*sinc[c-C] + samples[D]*sinc[d-D]
where sample[X] is the value of the sample at time X, and sinc[y] is the windowed sinc value at position y, which can
range from -window_size/2 to window_size/2. Notice that for a given offset S-floor(S), the list of sinc values required
are always the same. Therefore, we precompute the required sinc values for N possible sample fractions, then interpolate
between them.
*/

static real32 fetch_sample(const c_sample *sample, uint32 channel, real32 time) {
	wl_assert(!sample->is_mipmap());

	// Determine which sample we want and split it into fractional and integer parts
	real32 sample_index = time * static_cast<real32>(sample->get_sample_rate());
	real32 sample_index_rounded_down;
	real32 sample_index_frac_part = std::modf(sample_index, &sample_index_rounded_down);
	uint32 sample_index_int_part = static_cast<uint32>(sample_index_rounded_down);
	sample_index_int_part += sample->get_first_sampling_frame();

	wl_assert(sample_index_int_part >= (k_sinc_window_size / 2 - 1));
	uint32 start_window_sample_index = sample_index_int_part - (k_sinc_window_size / 2 - 1);
	uint32 end_window_sample_index = start_window_sample_index + k_sinc_window_size - 1;

	// We use SSE so we need to read in 16-byte-aligned blocks. Therefore if our start sample is unaligned we must
	// perform some shifting.

	uint32 start_sse_index = align_size_down(start_window_sample_index, k_sse_block_elements);
	uint32 end_sse_index = align_size(end_window_sample_index, k_sse_block_elements);
	c_wrapped_array_const<real32> sample_data = sample->get_channel_sample_data(channel);
	const real32 *sample_ptr = &sample_data[start_sse_index];
	// Don't dereference the array directly because we may want a pointer to the very end, which is not a valid element
	wl_assert(end_sse_index <= sample_data.get_count());
	const real32 *sample_end_ptr = sample_data.get_pointer() + end_sse_index;

	uint32 sse_offset = start_window_sample_index - start_sse_index;
	wl_assert(VALID_INDEX(sse_offset, k_sse_block_elements));

	// Determine which windowed sinc lookup table to use
	real32 sinc_window_table_index = sample_index_frac_part * static_cast<real32>(k_sinc_window_sample_resolution);
	real32 sinc_window_table_index_rounded_down;
	real32 sinc_window_table_index_frac_part =
		std::modf(sinc_window_table_index, &sinc_window_table_index_rounded_down);
	uint32 sinc_window_table_index_int_part = static_cast<uint32>(sinc_window_table_index_rounded_down);
	wl_assert(VALID_INDEX(sinc_window_table_index_int_part, k_sinc_window_sample_resolution));
	const s_sinc_window_coefficients *sinc_window_ptr = k_sinc_window_coefficients[sinc_window_table_index_int_part];

	c_real32_4 slope_multiplier(sinc_window_table_index_frac_part);
	c_real32_4 result_4(0.0f);

	switch (sse_offset) {
	case 0:
	{
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_sse_block_elements, sinc_window_ptr++) {
			c_real32_4 samples(sample_ptr);

			c_real32_4 sinc_window_values(sinc_window_ptr->values);
			c_real32_4 sinc_window_slopes(sinc_window_ptr->slopes);
			result_4 = result_4 + (samples * sinc_window_values) + (slope_multiplier * sinc_window_slopes);
		}
		break;
	}

	case 1:
	{
		c_real32_4 prev_block(sample_ptr);
		sample_ptr += k_sse_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_sse_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = shift_left_1(prev_block, curr_block);
			prev_block = curr_block;

			c_real32_4 sinc_window_values(sinc_window_ptr->values);
			c_real32_4 sinc_window_slopes(sinc_window_ptr->slopes);
			result_4 = result_4 + (samples * sinc_window_values) + (slope_multiplier * sinc_window_slopes);
		}
		break;
	}

	case 2:
	{
		c_real32_4 prev_block(sample_ptr);
		sample_ptr += k_sse_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_sse_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = shift_left_2(prev_block, curr_block);
			prev_block = curr_block;

			c_real32_4 sinc_window_values(sinc_window_ptr->values);
			c_real32_4 sinc_window_slopes(sinc_window_ptr->slopes);
			result_4 = result_4 + (samples * sinc_window_values) + (slope_multiplier * sinc_window_slopes);
		}
		break;
	}

	case 3:
	{
		c_real32_4 prev_block(sample_ptr);
		sample_ptr += k_sse_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_sse_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = shift_left_3(prev_block, curr_block);
			prev_block = curr_block;

			c_real32_4 sinc_window_values(sinc_window_ptr->values);
			c_real32_4 sinc_window_slopes(sinc_window_ptr->slopes);
			result_4 = result_4 + (samples * sinc_window_values) + (slope_multiplier * sinc_window_slopes);
		}
		break;
	}

	default:
		wl_vhalt("SSE offset not in range 0-3?");
	}

	// Perform the final sum and return the result
	c_real32_4 result_4_sum = result_4.sum_elements();
	ALIGNAS_SSE real32 result[k_sse_block_elements];
	result_4_sum.store(result);
	return result[0];
}

static c_real32_4 shift_left_1(const c_real32_4 &a, const c_real32_4 &b) {
	// [.xyz][w...] => [z.w.]
	// [.xyz][z.w.] => [xyzw]
	c_real32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<1, 2, 0, 2>(a, c);
}

static c_real32_4 shift_left_2(const c_real32_4 &a, const c_real32_4 &b) {
	// [..xy][zw..] => [xyzw]
	return shuffle<2, 3, 0, 1>(a, b);
}

static c_real32_4 shift_left_3(const c_real32_4 &a, const c_real32_4 &b) {
	// [...x][yzw.] => [x.y.]
	// [x.y.][yzw.] => [xyzw]
	c_real32_4 c = shuffle<3, 0, 0, 0>(a, b);
	return shuffle<0, 2, 1, 2>(c, b);
}

static size_t task_memory_query_sampler(const s_task_function_context &context) {
	return s_buffer_operation_sampler::query_memory();
}

static void task_initializer_sampler(const s_task_function_context &context) {
	const char *name = context.arguments[0].get_string_constant_in();
	real32 channel = context.arguments[1].get_real_constant_in();
	s_buffer_operation_sampler::initialize(
		context.event_interface,
		context.sample_requester,
		static_cast<s_buffer_operation_sampler *>(context.task_memory),
		name, k_sample_loop_mode_none, false, channel);
}

static void task_voice_initializer_sampler(const s_task_function_context &context) {
	s_buffer_operation_sampler::voice_initialize(static_cast<s_buffer_operation_sampler *>(context.task_memory));
}

static void task_function_sampler_in_out(const s_task_function_context &context) {
	s_buffer_operation_sampler::in_out(
		context.event_interface, context.arguments[0].get_string_constant_in(),
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[2].get_real_buffer_or_constant_in(),
		context.arguments[3].get_real_buffer_out());
}

static void task_function_sampler_inout(const s_task_function_context &context) {
	s_buffer_operation_sampler::inout(
		context.event_interface, context.arguments[0].get_string_constant_in(),
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[2].get_real_buffer_inout());
}

static void task_initializer_sampler_loop_in_in_out(const s_task_function_context &context) {
	const char *name = context.arguments[0].get_string_constant_in();
	real32 channel = context.arguments[1].get_real_constant_in();
	bool bidi = context.arguments[2].get_bool_constant_in();
	real32 phase = context.arguments[4].is_constant() ?
		context.arguments[4].get_real_buffer_or_constant_in().get_constant() : 0.0f;
	phase = clamp(phase, 0.0f, 1.0f);
	e_sample_loop_mode loop_mode = bidi ? k_sample_loop_mode_bidi_loop : k_sample_loop_mode_loop;
	s_buffer_operation_sampler::initialize(
		context.event_interface,
		context.sample_requester,
		static_cast<s_buffer_operation_sampler *>(context.task_memory),
		name, loop_mode, (phase != 0.0f), channel);
}

static void task_function_sampler_loop_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_sampler::loop_in_in_out(
		context.event_interface, context.arguments[0].get_string_constant_in(),
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[3].get_real_buffer_or_constant_in(),
		context.arguments[4].get_real_buffer_or_constant_in(),
		context.arguments[5].get_real_buffer_out());
}

static void task_initializer_sampler_loop_inout_in(const s_task_function_context &context) {
	// Same as the in_in_out version, because the out argument is shared with time, not phase
	task_initializer_sampler_loop_in_in_out(context);
}

static void task_function_sampler_loop_inout_in(const s_task_function_context &context) {
	s_buffer_operation_sampler::loop_inout_in(
		context.event_interface, context.arguments[0].get_string_constant_in(),
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[3].get_real_buffer_inout(),
		context.arguments[4].get_real_buffer_or_constant_in());
}

static void task_initializer_sampler_loop_in_inout(const s_task_function_context &context) {
	const char *name = context.arguments[0].get_string_constant_in();
	real32 channel = context.arguments[1].get_real_constant_in();
	bool bidi = context.arguments[2].get_bool_constant_in();
	e_sample_loop_mode loop_mode = bidi ? k_sample_loop_mode_bidi_loop : k_sample_loop_mode_loop;
	s_buffer_operation_sampler::initialize(
		context.event_interface,
		context.sample_requester,
		static_cast<s_buffer_operation_sampler *>(context.task_memory),
		name, loop_mode, true, channel);
}

static void task_function_sampler_loop_in_inout(const s_task_function_context &context) {
	s_buffer_operation_sampler::loop_in_inout(
		context.event_interface, context.arguments[0].get_string_constant_in(),
		context.sample_accessor, static_cast<s_buffer_operation_sampler *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[3].get_real_buffer_or_constant_in(),
		context.arguments[4].get_real_buffer_inout());
}

void register_task_functions_sampler() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sampler_in_out_uid,
				"sampler_in_out",
				task_memory_query_sampler, task_initializer_sampler, task_voice_initializer_sampler, task_function_sampler_in_out,
				s_task_function_argument_list::build(TDT(in, string), TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sampler_inout_uid,
				"sampler_inout",
				task_memory_query_sampler, task_initializer_sampler, task_voice_initializer_sampler, task_function_sampler_inout,
				s_task_function_argument_list::build(TDT(in, string), TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_sampler_inout_uid, "vvb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 2)),

			s_task_function_mapping::build(k_task_function_sampler_in_out_uid, "vvv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 3)),

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_sampler_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sampler_loop_in_in_out_uid,
				"sampler_loop_in_in_out",
				task_memory_query_sampler, task_initializer_sampler_loop_in_in_out, task_voice_initializer_sampler, task_function_sampler_loop_in_in_out,
				s_task_function_argument_list::build(TDT(in, string), TDT(in, real), TDT(in, bool), TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sampler_loop_inout_in_uid,
				"sampler_loop_inout_in",
				task_memory_query_sampler, task_initializer_sampler_loop_inout_in, task_voice_initializer_sampler, task_function_sampler_loop_inout_in,
				s_task_function_argument_list::build(TDT(in, string), TDT(in, real), TDT(in, bool), TDT(inout, real), TDT(in, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sampler_loop_in_inout_uid,
				"sampler_loop_in_inout",
				task_memory_query_sampler, task_initializer_sampler_loop_in_inout, task_voice_initializer_sampler, task_function_sampler_loop_in_inout,
				s_task_function_argument_list::build(TDT(in, string), TDT(in, real), TDT(in, bool), TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_sampler_loop_inout_in_uid, "vvvbv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 2, 3, 4, 3)),

			s_task_function_mapping::build(k_task_function_sampler_loop_in_inout_uid, "vvvvb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 3, 4, 4)),

			s_task_function_mapping::build(k_task_function_sampler_loop_in_in_out_uid, "vvvvv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 3, 4, 5)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_sampler_loop_uid, c_task_function_mapping_list::construct(mappings));
	}
}