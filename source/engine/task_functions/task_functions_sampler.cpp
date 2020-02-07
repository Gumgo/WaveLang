#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/sample/sample.h"
#include "engine/sample/sample_library.h"
#include "engine/task_functions/task_functions_sampler.h"
#include "engine/task_functions/sampler/fetch_sample.h"
#include "engine/task_functions/sampler/sampler_context.h"

class c_event_interface;

// $TODO add initial_delay parameter

struct s_buffer_operation_sampler : public s_sampler_context {
	static void in_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_or_constant_in speed,
		c_real_buffer_out out);
	static void inout(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_inout speed_out);

	// Constant versions
	static void const_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		real32 speed,
		c_real_buffer_out out);

	static void loop_in_in_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_or_constant_in speed,
		c_real_buffer_or_constant_in phase,
		c_real_buffer_out out);
	static void loop_inout_in(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_inout speed_out,
		c_real_buffer_or_constant_in phase);
	static void loop_in_inout(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_or_constant_in speed,
		c_real_buffer_inout phase_out);

	// Constant versions
	static void loop_const_in_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		real32 speed,
		c_real_buffer_or_constant_in phase,
		c_real_buffer_out out);
	static void loop_in_const_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_or_constant_in speed,
		real32 phase,
		c_real_buffer_out out);
	static void loop_const_const_out(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		real32 speed,
		real32 phase,
		c_real_buffer_out out);
	static void loop_inout_const(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_inout speed_out,
		real32 phase);
	static void loop_const_inout(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate,
		real32 speed,
		c_real_buffer_inout phase_out);
};

// Extracts sample data
static void get_sample_time_data(
	const c_sample *sample,
	real64 &out_length_samples,
	real64 &out_loop_start_sample,
	real64 &out_loop_end_sample);

// To avoid rewriting the algorithm in multiple variations, we create a base template class and implement the variants
// using https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern.

// $TODO it would be great if we could use constexpr-if for k_loop, but it's not supported yet
template<bool k_loop, typename t_derived>
struct s_sampler_algorithm {
	// "Overridden" methods:

	//c_real_buffer_out get_out() const;
	//real32 *get_out_pointer() const;

	//void begin_loop();
	//bool is_valid() const;
	//void advance();
	//void run_loop();

	//bool is_zero_speed_and_constant_phase() const;
	//c_real32_4 get_advance() const;
	//c_real32_4 get_speed() const;
	//real64 get_sample_offset(size_t index) const;

	// Fields available to derived implementations:
	real32 stream_sample_rate;
	real32 sample_rate_0;
	real64 phase_to_sample_offset_multiplier;

	// Utility functions for subclasses
	c_real32_4 compute_advance(const c_real32_4 &speed_val) const {
		return speed_val * c_real32_4(sample_rate_0) / c_real32_4(stream_sample_rate);
	}

	c_real32_4 compute_speed_adjusted_sample_rate_0(const c_real32_4 &speed_val) const {
		return c_real32_4(sample_rate_0) * speed_val;
	}

	void compute_sample_offset(
		const c_real32_4 &phase_val, s_static_array<real64, k_simd_block_elements> &out_sample_offset) {
		c_real32_4 clamped_phase = min(max(phase_val, c_real32_4(0.0f)), c_real32_4(1.0f));
		ALIGNAS_SIMD s_static_array<real32, k_simd_block_elements> phase;
		clamped_phase.store(phase.get_elements());
		for (size_t i = 0; i < k_simd_block_elements; i++) {
			out_sample_offset[i] = phase[i] * phase_to_sample_offset_multiplier;
		}
	}

	void run(
		c_event_interface *event_interface,
		const char *sample_name,
		c_sample_library_accessor *sample_accessor,
		s_buffer_operation_sampler *context,
		size_t buffer_size,
		uint32 sample_rate) {
		// Allows us to call "overridden" methods without inheritance
		t_derived *this_derived = static_cast<t_derived *>(this);

		const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
		if (context->handle_failed_sample(sample, this_derived->get_out(), event_interface, sample_name) ||
			context->handle_reached_end(this_derived->get_out())) {
			return;
		}

		if (k_loop) {
			// Bidi loops are preprocessed so at this point they act as normal loops
			wl_assert(sample->get_loop_mode() == k_sample_loop_mode_loop ||
				sample->get_loop_mode() == k_sample_loop_mode_bidi_loop);
			wl_assert(!context->reached_end);
		} else {
			wl_assert(sample->get_loop_mode() == k_sample_loop_mode_none);
		}

		bool is_wavetable = sample->is_wavetable();
		real64 length_samples, loop_start_sample, loop_end_sample;
		get_sample_time_data(sample, length_samples, loop_start_sample, loop_end_sample);
		phase_to_sample_offset_multiplier = loop_end_sample - loop_start_sample;

		stream_sample_rate = static_cast<real32>(sample_rate);
		sample_rate_0 = static_cast<real32>(sample->get_sample_rate());

		bool is_result_constant = this_derived->is_zero_speed_and_constant_phase();

		size_t samples_written = 0;
		size_t buffer_samples_remaining = buffer_size;
		for (this_derived->begin_loop(); this_derived->is_valid(); this_derived->advance()) {
			this_derived->run_loop();
			real32 *out_ptr = this_derived->get_out_pointer();
			c_real32_4 advance = this_derived->get_advance();

			// Increment the time first, storing each intermediate time value
			ALIGNAS_SIMD s_static_array<real64, k_simd_block_elements> samples;
			size_t increment_count = sampler_context_increment_time<k_loop>(
				*context, loop_start_sample, loop_end_sample, advance, buffer_samples_remaining, samples);
			wl_assert(increment_count > 0);

			if (k_loop) {
				for (size_t i = 0; i < k_simd_block_elements; i++) {
					// Apply sample offset for phase
					samples[i] += this_derived->get_sample_offset(i);
				}
			}

			if (is_wavetable) {
				fetch_wavetable_samples(
					sample,
					context->channel,
					stream_sample_rate,
					sample_rate_0,
					this_derived->get_speed(),
					increment_count,
					samples, out_ptr);
			} else {
				for (size_t i = 0; i < increment_count; i++) {
					out_ptr[i] = fetch_sample(sample, context->channel, samples[i]);
				}
			}

			samples_written += increment_count;
			if (is_result_constant || (!k_loop && context->reached_end)) {
				break;
			}
		}

		if (is_result_constant) {
			if (samples_written == 0) {
				// We reached the end without writing any samples
				wl_assert(!k_loop);
				*this_derived->get_out_pointer() = 0.0f;
			}

			this_derived->get_out()->set_constant(true);
		} else {
			size_t samples_remaining = buffer_size - samples_written;

			if (k_loop) {
				wl_assert(samples_remaining == 0);
			} else {
				// If the sample ended before the end of the buffer, fill the rest with 0
				memset(this_derived->get_out_pointer(), 0, samples_remaining * sizeof(real32));
			}

			// If samples_written is 0, the buffer is filled with 0
			this_derived->get_out()->set_constant(samples_written == 0);
		}
	}
};

void s_buffer_operation_sampler::in_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_or_constant_in speed,
	c_real_buffer_out out) {
	if (speed.is_constant()) {
		// If speed is constant, simply use the constant-speed version
		real32 speed_constant = speed.get_constant();
		const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed_constant,
			out);
		return;
	}

	struct s_sampler_subalgorithm : public s_sampler_algorithm<false, s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		c_real32_4 speed_val;
		c_real32_4 advance_val;

		s_sampler_subalgorithm(size_t buffer_size, c_real_buffer_in speed, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_ptr = speed->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_simd_block_elements; speed_ptr += k_simd_block_elements; }

		void run_loop() {
			speed_val.load(speed_ptr);
			advance_val = compute_advance(speed_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return 0.0; }
	};

	s_sampler_subalgorithm(buffer_size, speed.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::inout(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_inout speed_out) {
	validate_buffer(speed_out);

	if (speed_out->is_constant()) {
		// If speed is constant, simply use the constant-speed version
		// We can store the first value of the buffer and then convert it to a pure output buffer
		real32 speed_constant = *speed_out->get_data<real32>();
		const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed_constant,
			speed_out);
		return;
	}

	struct s_sampler_subalgorithm : public s_sampler_algorithm<false, s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		c_real32_4 speed_val;
		c_real32_4 advance_val;

		s_sampler_subalgorithm(size_t buffer_size, c_real_buffer_inout speed_out) {
			out = speed_out;
			speed_out_ptr = out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_simd_block_elements);
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() { speed_out_ptr += k_simd_block_elements; }

		void run_loop() {
			speed_val.load(speed_out_ptr);
			advance_val = compute_advance(speed_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return 0.0; }
	};

	s_sampler_subalgorithm(buffer_size, speed_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::const_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	real32 speed,
	c_real_buffer_out out) {
	validate_buffer(out);

	struct s_sampler_subalgorithm : public s_sampler_algorithm<false, s_sampler_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		c_real32_4 speed_val;
		c_real32_4 advance_val;
		bool zero_speed;

		s_sampler_subalgorithm(size_t buffer_size, real32 speed, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_val = c_real32_4(speed);
			zero_speed = speed == 0.0f;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(speed_val);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_simd_block_elements; }
		void run_loop() {}

		bool is_zero_speed_and_constant_phase() const { return zero_speed; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return 0.0; }
	};

	s_sampler_subalgorithm(buffer_size, speed, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_in_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_or_constant_in speed,
	c_real_buffer_or_constant_in phase,
	c_real_buffer_out out) {
	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_const_const_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase_constant,
				out);
		} else {
			loop_const_in_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase,
				out);
		}
		return;
	} else {
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_in_const_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed,
				phase_constant,
				out);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, c_real_buffer_in speed, c_real_buffer_in phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_ptr = speed->get_data<real32>();
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_simd_block_elements;
			speed_ptr += k_simd_block_elements;
			phase_ptr += k_simd_block_elements;
		}

		void run_loop() {
			speed_val.load(speed_ptr);
			advance_val = compute_advance(speed_val);
			c_real32_4 phase_val(phase_ptr);
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_inout_in(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_inout speed_out,
	c_real_buffer_or_constant_in phase) {
	// Identify constant optimizations
	if (speed_out->is_constant()) {
		real32 speed_constant = *speed_out->get_data<real32>();
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_const_const_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase_constant,
				speed_out);
		} else {
			loop_const_in_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase,
				speed_out);
		}
		return;
	} else {
		if (phase.is_constant()) {
			real32 phase_constant = phase.get_constant();
			loop_inout_const(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_out,
				phase_constant);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size,
			c_real_buffer_inout speed_out,
			c_real_buffer_in phase) {
			out = speed_out;
			speed_out_ptr = speed_out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_simd_block_elements);
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() {
			speed_out_ptr += k_simd_block_elements;
			phase_ptr += k_simd_block_elements;
		}

		void run_loop() {
			speed_val.load(speed_out_ptr);
			advance_val = compute_advance(speed_val);
			c_real32_4 phase_val(phase_ptr);
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed_out, phase.get_buffer()).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_inout(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_or_constant_in speed,
	c_real_buffer_inout phase_out) {
	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		if (phase_out->is_constant()) {
			real32 phase_constant = *phase_out->get_data<real32>();
			loop_const_const_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase_constant,
				phase_out);
		} else {
			loop_const_inout(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed_constant,
				phase_out);
		}
		return;
	} else {
		if (phase_out->is_constant()) {
			real32 phase_constant = *phase_out->get_data<real32>();
			loop_in_const_out(
				event_interface,
				sample_name,
				sample_accessor,
				context,
				buffer_size,
				sample_rate,
				speed,
				phase_constant,
				phase_out);
		}
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *phase_out_ptr;
		real32 *phase_out_ptr_end;
		const real32 *speed_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, c_real_buffer_in speed, c_real_buffer_inout phase_out) {
			out = phase_out;
			phase_out_ptr = phase_out->get_data<real32>();
			phase_out_ptr_end = phase_out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_ptr = speed->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return phase_out_ptr; }

		void begin_loop() {}
		bool is_valid() const { return phase_out_ptr < phase_out_ptr_end; }
		void advance() {
			phase_out_ptr += k_simd_block_elements;
			speed_ptr += k_simd_block_elements;
		}

		void run_loop() {
			speed_val.load(speed_ptr);
			advance_val = compute_advance(speed_val);
			c_real32_4 phase_val(phase_out_ptr);
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_in_const_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_or_constant_in speed,
	real32 phase,
	c_real_buffer_out out) {
	// Identify constant optimizations
	if (speed.is_constant()) {
		real32 speed_constant = speed.get_constant();
		loop_const_const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed_constant,
			phase,
			out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		const real32 *speed_ptr;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, c_real_buffer_in speed, real32 phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_ptr = speed->get_data<real32>();
			phase_val = phase;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_simd_block_elements;
			speed_ptr += k_simd_block_elements;
		}

		void run_loop() {
			speed_val.load(speed_ptr);
			advance_val = compute_advance(speed_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed.get_buffer(), phase, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_inout_const(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_inout speed_out,
	real32 phase) {
	validate_buffer(speed_out);

	// Identify constant optimizations
	if (speed_out->is_constant()) {
		real32 speed_constant = *speed_out->get_data<real32>();
		loop_const_const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed_constant,
			phase,
			speed_out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *speed_out_ptr;
		real32 *speed_out_ptr_end;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, c_real_buffer_inout speed_out, real32 phase) {
			out = speed_out;
			speed_out_ptr = speed_out->get_data<real32>();
			speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_simd_block_elements);
			phase_val = phase;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return speed_out_ptr; }

		void begin_loop() {
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_valid() const { return speed_out_ptr < speed_out_ptr_end; }
		void advance() {
			speed_out_ptr += k_simd_block_elements;
		}

		void run_loop() {
			speed_val.load(speed_out_ptr);
			advance_val = compute_advance(speed_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed_out, phase).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_in_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	real32 speed,
	c_real_buffer_or_constant_in phase,
	c_real_buffer_out out) {
	// Identify constant optimizations
	if (phase.is_constant()) {
		real32 phase_constant = phase.get_constant();
		loop_const_const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed,
			phase_constant,
			out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		c_real32_4 speed_val;
		const real32 *phase_ptr;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, real32 speed, c_real_buffer_in phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_val = c_real32_4(speed);
			phase_ptr = phase->get_data<real32>();
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(speed_val);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() {
			out_ptr += k_simd_block_elements;
			phase_ptr += k_simd_block_elements;
		}

		void run_loop() {
			c_real32_4 phase_val(phase_ptr);
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase.get_buffer(), out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_inout(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	real32 speed,
	c_real_buffer_inout phase_out) {
	validate_buffer(phase_out);

	// Identify constant optimizations
	if (phase_out->is_constant()) {
		real32 phase_constant = *phase_out->get_data<real32>();
		loop_const_const_out(
			event_interface,
			sample_name,
			sample_accessor,
			context,
			buffer_size,
			sample_rate,
			speed,
			phase_constant,
			phase_out);
		return;
	}

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *phase_out_ptr;
		real32 *phase_out_ptr_end;
		c_real32_4 speed_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;

		s_sampler_loop_subalgorithm(
			size_t buffer_size, real32 speed, c_real_buffer_inout phase_out) {
			out = phase_out;
			phase_out_ptr = phase_out->get_data<real32>();
			phase_out_ptr_end = phase_out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_val = c_real32_4(speed);
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return phase_out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(speed_val);
		}

		bool is_valid() const { return phase_out_ptr < phase_out_ptr_end; }
		void advance() {
			phase_out_ptr += k_simd_block_elements;
		}

		void run_loop() {
			c_real32_4 phase_val(phase_out_ptr);
			compute_sample_offset(phase_val, sample_offset_val);
		}

		bool is_zero_speed_and_constant_phase() const { return false; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase_out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
}

void s_buffer_operation_sampler::loop_const_const_out(
	c_event_interface *event_interface,
	const char *sample_name,
	c_sample_library_accessor *sample_accessor,
	s_buffer_operation_sampler *context,
	size_t buffer_size,
	uint32 sample_rate,
	real32 speed,
	real32 phase,
	c_real_buffer_out out) {
	validate_buffer(out);

	struct s_sampler_loop_subalgorithm : public s_sampler_algorithm<true, s_sampler_loop_subalgorithm> {
		// Fields
		c_real_buffer_out out;
		real32 *out_ptr;
		real32 *out_ptr_end;
		c_real32_4 speed_val;
		real32 phase_val;
		c_real32_4 advance_val;
		c_real32_4 speed_adjusted_sample_rate_0_val;
		s_static_array<real64, k_simd_block_elements> sample_offset_val;
		bool is_zero_speed;

		s_sampler_loop_subalgorithm(size_t buffer_size, real32 speed, real32 phase, c_real_buffer_out out) {
			this->out = out;
			out_ptr = out->get_data<real32>();
			out_ptr_end = out_ptr + align_size(buffer_size, k_simd_block_elements);
			speed_val = c_real32_4(speed);
			phase_val = phase;
			is_zero_speed = speed == 0.0f;
		}

		c_real_buffer_out get_out() const { return out; }
		real32 *get_out_pointer() const { return out_ptr; }

		void begin_loop() {
			advance_val = compute_advance(speed_val);
			compute_sample_offset(c_real32_4(phase_val), sample_offset_val);
		}

		bool is_valid() const { return out_ptr < out_ptr_end; }
		void advance() { out_ptr += k_simd_block_elements; }
		void run_loop() {}

		bool is_zero_speed_and_constant_phase() const { return is_zero_speed; }
		c_real32_4 get_advance() const { return advance_val; }
		c_real32_4 get_speed() const { return speed_val; }
		real64 get_sample_offset(size_t index) const { return sample_offset_val[index]; }
	};

	s_sampler_loop_subalgorithm(buffer_size, speed, phase, out).
		run(event_interface, sample_name, sample_accessor, context, buffer_size, sample_rate);
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
		return s_buffer_operation_sampler::query_memory();
	}

	void sampler_initializer(
		const s_task_function_context &context,
		const char *name,
		real32 channel) {
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->initialize_file(
			context.event_interface,
			context.sample_requester,
			name, k_sample_loop_mode_none, false, channel);
	}

	void sampler_voice_initializer(const s_task_function_context &context) {
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->voice_initialize();
	}

	void sampler_in_out(
		const s_task_function_context &context,
		const char *name,
		c_real_const_buffer_or_constant speed,
		c_real_buffer *result) {
		s_buffer_operation_sampler::in_out(
			context.event_interface,
			name,
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			speed,
			result);
	}

	void sampler_inout(
		const s_task_function_context &context,
		const char *name,
		c_real_buffer *speed_result) {
		s_buffer_operation_sampler::inout(
			context.event_interface,
			name,
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			speed_result);
	}

	void sampler_loop_initializer(
		const s_task_function_context &context,
		const char *name,
		real32 channel,
		bool bidi,
		c_real_const_buffer_or_constant phase) {
		bool phase_shift_enabled = !phase.is_constant() || (clamp(phase.get_constant(), 0.0f, 1.0f) != 0.0f);
		e_sample_loop_mode loop_mode = bidi ? k_sample_loop_mode_bidi_loop : k_sample_loop_mode_loop;
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->initialize_file(
			context.event_interface,
			context.sample_requester,
			name, loop_mode, phase_shift_enabled, channel);
	}

	void sampler_loop_in_in_out(
		const s_task_function_context &context,
		const char *name,
		c_real_const_buffer_or_constant speed,
		c_real_const_buffer_or_constant phase,
		c_real_buffer *result) {
		s_buffer_operation_sampler::loop_in_in_out(
			context.event_interface,
			name,
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			speed,
			phase,
			result);
	}

	void sampler_loop_inout_in(
		const s_task_function_context &context,
		const char *name,
		c_real_buffer *speed_result,
		c_real_const_buffer_or_constant phase) {
		s_buffer_operation_sampler::loop_inout_in(
			context.event_interface,
			name,
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			speed_result,
			phase);
	}

	void sampler_loop_in_inout_initializer(
		const s_task_function_context &context,
		const char *name,
		real32 channel,
		bool bidi) {
		e_sample_loop_mode loop_mode = bidi ? k_sample_loop_mode_bidi_loop : k_sample_loop_mode_loop;
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->initialize_file(
			context.event_interface,
			context.sample_requester,
			name, loop_mode, true, channel);
	}

	void sampler_loop_in_inout(
		const s_task_function_context &context,
		const char *name,
		c_real_const_buffer_or_constant speed,
		c_real_buffer *phase_result) {
		s_buffer_operation_sampler::loop_in_inout(
			context.event_interface,
			name,
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			speed,
			phase_result);
	}

	void sampler_wavetable_initializer(
		const s_task_function_context &context,
		c_real_array harmonic_weights,
		real32 sample_count,
		c_real_const_buffer_or_constant phase) {
		bool phase_shift_enabled = !phase.is_constant() || (clamp(phase.get_constant(), 0.0f, 1.0f) != 0.0f);
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->initialize_wavetable(
			context.event_interface,
			context.sample_requester,
			harmonic_weights, sample_count, phase_shift_enabled);
	}

	void sampler_wavetable_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant frequency,
		c_real_const_buffer_or_constant phase,
		c_real_buffer *result) {
		s_buffer_operation_sampler::loop_in_in_out(
			context.event_interface,
			"<generated_wavetable>",
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			frequency,
			phase,
			result);
	}

	void sampler_wavetable_inout_in(
		const s_task_function_context &context,
		c_real_buffer *frequency_result,
		c_real_const_buffer_or_constant phase) {
		s_buffer_operation_sampler::loop_inout_in(
			context.event_interface,
			"<generated_wavetable>",
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			frequency_result,
			phase);
	}

	void sampler_wavetable_in_inout_initializer(
		const s_task_function_context &context,
		c_real_array harmonic_weights,
		real32 sample_count) {
		static_cast<s_buffer_operation_sampler *>(context.task_memory)->initialize_wavetable(
			context.event_interface,
			context.sample_requester,
			harmonic_weights, sample_count, false);
	}

	void sampler_wavetable_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant frequency,
		c_real_buffer *phase_result) {
		s_buffer_operation_sampler::loop_in_inout(
			context.event_interface,
			"<generated_wavetable>",
			context.sample_accessor,
			static_cast<s_buffer_operation_sampler *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			frequency,
			phase_result);
	}

}
