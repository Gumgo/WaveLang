#include "engine/buffer_operations/buffer_operations_sampler.h"
#include "engine/sample/sample_library.h"

#define GENERATE_SINC_WINDOW 0

#if PREDEFINED(GENERATE_SINC_WINDOW)
#include <fstream>
#endif // PREDEFINED(GENERATE_SINC_WINDOW)

// The size in samples of the sinc window, inclusive of the first and last sample
static const size_t k_sinc_window_size = 9;

// The number of windowed sinc values computed per sample
// The total number of sinc window samples (excluding padding) is (window_size-1) * resolution
static const size_t k_sinc_window_sample_resolution = 32;

static_assert(k_sinc_window_size % 2 == 1, "Window must be odd in order to be centered on a sample and symmetrical");
static_assert(k_sinc_window_size / 2 <= k_max_sample_padding, "Window half-size cannot exceeds max sample padding");

// To be more cache-friendly, we precompute the slopes and place them adjacent to the values. Otherwise we would need to
// traverse two arrays in parallel. The cost is that we use 2x memory (which could ultimately hurt caching).
struct ALIGNAS_SSE s_sinc_window_coefficients {
	// Four sinc window values, each spaced apart by 1 sample distance
	real32 values[k_sse_block_elements];

	// Slopes corresponding to each value for linear interpolation
	real32 slopes[k_sse_block_elements];
};

#include "sinc_window_coefficients.inl"

static_assert(NUMBEROF(k_sinc_window_coefficients) == k_sinc_window_sample_resolution,
	"Windowed sinc filter coefficient count mismatch");
static_assert(NUMBEROF(k_sinc_window_coefficients[0]) ==
	ALIGN_SIZE(k_sinc_window_size - 1, k_sse_block_elements) / k_sse_block_elements,
	"Windowed sinc filter coefficient count mismatch");

// Common utility functions used in all versions of the sampler:

// Fills the output buffer with 0s if the sample failed to load or if the channel is invalid
static bool handle_failed_sample(const c_sample *sample, uint32 channel, c_buffer_out out);

// Converts sample data (frames) to timing data (seconds)
static void get_sample_time_data(const c_sample *sample,
	real32 &out_length, real32 &out_loop_start, real32 &out_loop_end);

// Increments the sample's current time up to 4 times based on the speed provided and returns the number of times
// incremented, which can be less than 4 if the sample ends or if the end of the buffer is reached. The time before each
// increment is stored in out_time. Handles looping/wrapping.
static size_t increment_time(bool loop, real32 length, real32 loop_start_time, real32 loop_end_time,
	s_buffer_operation_sampler *context, const c_real32_4 &advance, size_t &inout_buffer_samples_remaining,
	real32 (&out_time)[k_sse_block_elements]);

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

void s_buffer_operation_sampler::initialize(c_sample_library_requester *sample_requester,
	s_buffer_operation_sampler *context, const char *sample, e_sample_loop_mode loop_mode, real32 channel) {
	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));

	context->sample_handle = sample_requester->request_sample(sample, loop_mode);
	if (channel < 0.0f ||
		std::floor(channel) != channel) {
		context->sample_handle = c_sample_library::k_invalid_handle;
	}
	context->channel = static_cast<uint32>(channel);
	context->time = 0;
	context->reached_end = false;
}

void s_buffer_operation_sampler::buffer(
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_buffer_out out, c_buffer_in speed) {
	validate_buffer(out);
	validate_buffer(speed);

	if (speed->is_constant()) {
		// If speed is constant, simply use the constant-speed version
		const real32 *speed_ptr = speed->get_data<real32>();
		constant(sample_accessor, context, buffer_size, sample_rate, out, *speed_ptr);
		return;
	}

	const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
	if (handle_failed_sample(sample, context->channel, out)) {
		return;
	}

	// Fill with 0 if we got to the end of the sample and we're not looping
	if (context->reached_end) {
		wl_assert(sample->get_loop_mode() == k_sample_loop_mode_none);
		real32 *out_ptr = out->get_data<real32>();
		c_real32_4(0.0f).store(out_ptr);
		out->set_constant(true);
		return;
	}

	bool is_mipmap = sample->is_mipmap();
	const c_sample *sample_0 = is_mipmap ? sample->get_mipmap_sample(0) : sample;
	// Bidi loops are preprocessed so at this point they act as normal loops
	bool loop = (sample->get_loop_mode() != k_sample_loop_mode_none);
	real32 length;
	real32 loop_start_time;
	real32 loop_end_time;
	get_sample_time_data(sample_0, length, loop_start_time, loop_end_time);

	real32 stream_sample_rate = static_cast<real32>(sample_rate);
	real32 sample_rate_0 = static_cast<real32>(sample_0->get_sample_rate());

	size_t samples_written = 0;
	size_t buffer_samples_remaining = buffer_size;
	real32 *out_ptr = out->get_data<real32>();
	real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
	const real32 *speed_ptr = speed->get_data<real32>();
	for (; out_ptr < out_ptr_end; out_ptr += k_sse_block_elements, speed_ptr += k_sse_block_elements) {
		c_real32_4 speed_val(speed_ptr);
		c_real32_4 advance = speed_val / c_real32_4(stream_sample_rate);
		c_real32_4 speed_adjusted_sample_rate_0 = c_real32_4(sample_rate_0) * speed_val;

		// Increment the time first, storing each intermediate time value
		ALIGNAS_SSE real32 time[k_sse_block_elements];
		size_t increment_count = increment_time(loop, length, loop_start_time, loop_end_time, context, advance,
			buffer_samples_remaining, time);
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
	}

	// If the sample ended before the end of the buffer, fill the rest with 0
	size_t samples_remaining = buffer_size - samples_written;
	memset(out->get_data<real32>() + samples_written, 0, samples_remaining * sizeof(real32));
	out->set_constant(false);
}

void s_buffer_operation_sampler::bufferio(
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_buffer_inout speed_out) {
	validate_buffer(speed_out);

	if (speed_out->is_constant()) {
		// If speed is constant, simply use the constant-speed version
		// We can store the first value of the buffer and then convert it to a pure output buffer
		const real32 *speed_ptr = speed_out->get_data<real32>();
		constant(sample_accessor, context, buffer_size, sample_rate, speed_out, *speed_ptr);
		return;
	}

	const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
	if (handle_failed_sample(sample, context->channel, speed_out)) {
		return;
	}

	// Fill with 0 if we got to the end of the sample and we're not looping
	if (context->reached_end) {
		wl_assert(sample->get_loop_mode() == k_sample_loop_mode_none);
		real32 *out_ptr = speed_out->get_data<real32>();
		c_real32_4(0.0f).store(out_ptr);
		speed_out->set_constant(true);
		return;
	}

	bool is_mipmap = sample->is_mipmap();
	const c_sample *sample_0 = is_mipmap ? sample->get_mipmap_sample(0) : sample;
	// Bidi loops are preprocessed so at this point they act as normal loops
	bool loop = (sample->get_loop_mode() != k_sample_loop_mode_none);
	real32 length;
	real32 loop_start_time;
	real32 loop_end_time;
	get_sample_time_data(sample_0, length, loop_start_time, loop_end_time);

	real32 stream_sample_rate = static_cast<real32>(sample_rate);
	real32 sample_rate_0 = static_cast<real32>(sample_0->get_sample_rate());

	size_t samples_written = 0;
	size_t buffer_samples_remaining = buffer_size;
	real32 *speed_out_ptr = speed_out->get_data<real32>();
	real32 *speed_out_ptr_end = speed_out_ptr + align_size(buffer_size, k_sse_block_elements);
	for (; speed_out_ptr < speed_out_ptr_end; speed_out_ptr += k_sse_block_elements) {
		c_real32_4 speed_val(speed_out_ptr);
		c_real32_4 advance = speed_val / c_real32_4(stream_sample_rate);
		c_real32_4 speed_adjusted_sample_rate_0 = c_real32_4(sample_rate_0) * speed_val;

		// Increment the time first, storing each intermediate time value
		ALIGNAS_SSE real32 time[k_sse_block_elements];
		size_t increment_count = increment_time(loop, length, loop_start_time, loop_end_time, context, advance,
			buffer_samples_remaining, time);
		wl_assert(increment_count > 0);

		if (is_mipmap) {
			fetch_mipmapped_samples(sample, context->channel, stream_sample_rate, speed_adjusted_sample_rate_0,
				increment_count, time, speed_out_ptr);
		} else {
			for (size_t i = 0; i < increment_count; i++) {
				speed_out_ptr[i] = fetch_sample(sample, context->channel, time[i]);
			}
		}

		samples_written += increment_count;
	}

	// If the sample ended before the end of the buffer, fill the rest with 0
	size_t samples_remaining = buffer_size - samples_written;
	memset(speed_out->get_data<real32>() + samples_written, 0, samples_remaining * sizeof(real32));
	speed_out->set_constant(false);
}

void s_buffer_operation_sampler::constant(
	c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
	size_t buffer_size, uint32 sample_rate, c_buffer_out out, real32 speed) {
	validate_buffer(out);

	const c_sample *sample = sample_accessor->get_sample(context->sample_handle);
	if (handle_failed_sample(sample, context->channel, out)) {
		return;
	}

	// Fill with 0 if we got to the end of the sample and we're not looping
	if (context->reached_end) {
		wl_assert(sample->get_loop_mode() == k_sample_loop_mode_none);
		real32 *out_ptr = out->get_data<real32>();
		c_real32_4(0.0f).store(out_ptr);
		out->set_constant(true);
		return;
	}

	bool is_mipmap = sample->is_mipmap();
	const c_sample *sample_0 = is_mipmap ? sample->get_mipmap_sample(0) : sample;
	// Bidi loops are preprocessed so at this point they act as normal loops
	bool loop = (sample->get_loop_mode() != k_sample_loop_mode_none);
	real32 length;
	real32 loop_start_time;
	real32 loop_end_time;
	get_sample_time_data(sample_0, length, loop_start_time, loop_end_time);

	real32 stream_sample_rate = static_cast<real32>(sample_rate);
	c_real32_4 speed_val(speed);
	c_real32_4 advance = speed / c_real32_4(stream_sample_rate);
	c_real32_4 speed_adjusted_sample_rate_0 = c_real32_4(static_cast<real32>(sample_0->get_sample_rate()));
	speed_adjusted_sample_rate_0 = speed_adjusted_sample_rate_0 * speed_val;

	size_t samples_written = 0;
	size_t buffer_samples_remaining = buffer_size;
	real32 *out_ptr = out->get_data<real32>();
	real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
	for (; out_ptr < out_ptr_end; out_ptr += k_sse_block_elements) {
		// Increment the time first, storing each intermediate time value
		ALIGNAS_SSE real32 time[k_sse_block_elements];
		size_t increment_count = increment_time(loop, length, loop_start_time, loop_end_time, context, advance,
			buffer_samples_remaining, time);
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
	}

	// If the sample ended before the end of the buffer, fill the rest with 0
	size_t samples_remaining = buffer_size - samples_written;
	memset(out->get_data<real32>() + samples_written, 0, samples_remaining * sizeof(real32));
	out->set_constant(false);
}

static bool handle_failed_sample(const c_sample *sample, uint32 channel, c_buffer_out out) {
	// If the sample failed, fill with 0
	if (!sample) {
		c_real32_4 *out_ptr = out->get_data<c_real32_4>();
		*out_ptr = c_real32_4(0.0f);
		out->set_constant(true);
	}

	return sample == nullptr;
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

static size_t increment_time(bool loop, real32 length, real32 loop_start_time, real32 loop_end_time,
	s_buffer_operation_sampler *context, const c_real32_4 &advance, size_t &inout_buffer_samples_remaining,
	real32 (&out_time)[k_sse_block_elements]) {
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
		if (loop) {
			if (context->time >= loop_end_time) {
				// Return to the loop start point
				real32 wrap_offset = std::remainder(context->time - loop_start_time, loop_end_time - loop_start_time);
				context->time = loop_start_time + wrap_offset;
			}
		} else {
			if (context->time >= length) {
				context->reached_end = true;
				break;
			}
		}
	}

	// We should only ever increment less than 4 times if we reach the end of the buffer or the end of the sample
	if (increment_count < k_sse_block_elements) {
		wl_assert(context->reached_end || inout_buffer_samples_remaining == 0);
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
	c_int32_4 max_mip_index(static_cast<int32>(mip_count - 1));
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

#if PREDEFINED(GENERATE_SINC_WINDOW)
namespace sinc_window_generator {
	static const real64 k_pi = 3.1415926535897932384626433832795;
	static const real64 k_alpha = 3.0;

	real64 i_0(real64 x) {
		// I_0(x) = sum_{t=0 to inf} ((x/2)^2t / (t!)^2)

		static const int32 k_iterations = 15;
		real64 half_x = x * 0.5;
		real64 sum = 0.0;
		for (int32 t = 0; t < k_iterations; t++) {
			int64 t_factorial_sum = 1;
			for (int32 f = 2; f <= t; f++) {
				t_factorial_sum *= f;
			}

			real64 t_factorial = static_cast<real64>(t_factorial_sum);
			sum += pow(half_x, 2.0 * static_cast<real64>(t)) / (t_factorial * t_factorial);
		}

		return sum;
	}

	real64 kaiser(real64 alpha, uint32 n, uint32 i) {
		wl_assert(i >= 0 && i <= n);

		// w(i) = I_0(pi * alpha * sqrt(1 - ((2i)/n - 1)^2)) / I_0(pi * alpha)
		real64 sqrt_arg = (2.0 * static_cast<real64>(i)) / static_cast<real64>(n) - 1.0;
		sqrt_arg = 1.0 - (sqrt_arg * sqrt_arg);
		return i_0(k_pi * alpha * std::sqrt(sqrt_arg)) / i_0(k_pi * alpha);
	}

	real64 sinc(uint32 n, uint32 i) {
		wl_assert(i >= 0 && i <= n);
		real64 x = (static_cast<real64>(2 * i) / static_cast<real64>(n) - 1.0) *
			static_cast<real64>(k_sinc_window_size / 2);
		return (x == 0.0) ?
			1.0 :
			std::sin(k_pi * x) / (k_pi * x);
	}

	std::string fixup_real32(real32 v) {
		std::string str = std::to_string(v);
		if (str.find_first_of('.') == std::string::npos) {
			str += ".0";
		}
		str.push_back('f');
		return str;
	}

	void generate() {
		std::ofstream out("sinc_window_coefficients.inl");

		// Generate N arrays, one corresponding to each possible resolution offset (range [0,resolution-1])
		// Each array is window_size-1 in length (rounded up to multiples of SSE)

		size_t sse_array_length = align_size(k_sinc_window_size - 1, k_sse_block_elements) / k_sse_block_elements;
		uint32 total_values = static_cast<uint32>((k_sinc_window_size - 1) * k_sinc_window_sample_resolution);

		out << "static const s_sinc_window_coefficients k_sinc_window_coefficients[][" << sse_array_length << "] = {\n";

		for (size_t resolution_index = 0; resolution_index < k_sinc_window_sample_resolution; resolution_index++) {
			size_t sample_index;
			s_sinc_window_coefficients coefficients;
			ZERO_STRUCT(&coefficients);
			size_t sse_index = 0;

			out << "\t{\n";

			for (sample_index = 0; sample_index < k_sinc_window_size - 1; sample_index++) {
				uint32 window_value_index = static_cast<uint32>(
					(k_sinc_window_size - 2 - sample_index) * k_sinc_window_sample_resolution + resolution_index);

				real64 curr_value =
					sinc(total_values, window_value_index) * kaiser(3.0, total_values, window_value_index);
				real64 next_value =
					sinc(total_values, window_value_index + 1) * kaiser(3.0, total_values, window_value_index + 1);
				coefficients.values[sse_index] = static_cast<real32>(curr_value);
				coefficients.slopes[sse_index] = static_cast<real32>(
					(next_value - curr_value) / static_cast<real64>(k_sinc_window_sample_resolution));

				sse_index++;
				// Output if we're a multiple of 4, or if we're the last index
				if (sse_index == k_sse_block_elements || sample_index == k_sinc_window_size - 1) {
					out << "\t\t{ " <<
						fixup_real32(coefficients.values[0]) << ", " <<
						fixup_real32(coefficients.values[1]) << ", " <<
						fixup_real32(coefficients.values[2]) << ", " <<
						fixup_real32(coefficients.values[3]) << ", " <<
						fixup_real32(coefficients.slopes[0]) << ", " <<
						fixup_real32(coefficients.slopes[1]) << ", " <<
						fixup_real32(coefficients.slopes[2]) << ", " <<
						fixup_real32(coefficients.slopes[3]) << " },\n";

					ZERO_STRUCT(&coefficients);
					sse_index = 0;
				}
			}

			out << "\t},\n";
		}

		out << "};\n";
	}
}

int main(int argc, char **argv) {
	sinc_window_generator::generate();
	return 0;
}
#endif // PREDEFINED(GENERATE_SINC_WINDOW)

#if 0
#include <iostream>
void find_fast_log2_coefficients() {
	// Let x be a floating point number
	// x = M * 2^E
	// log2(x) = log2(M * 2^E)
	//         = log2(M) + log2(2^E)
	//         = log2(M) + E
	// For non-denormalized floats, M is in the range [1,2). For x in this range, log2(x) is in the range [0,1). We
	// wish to split up the result of log2 into an integer and fraction:
	// int = E
	// frac = log2(M)
	// Thus, we wish to find a fast approximation for log2(M) for M in the range [1,2).

	// We approximate using a 2nd degree polynomial, f(x) = ax^2 + bx + c. We will fix the endpoints (1,0) and (2,1),
	// add a third point (0,y). This results in three equations:
	//  a +  b + c = 0
	// 4a + 2b + c = 1
	//           c = y
	// Solving for a, b, c, we determine that:
	// a = 0.5 + 0.5y
	// b = -0.5 - 1.5y
	// c = y
	// and so our equation becomes f_y(x) = (0.5 + 0.5y)x^2 + (-0.5y - 1.5y)x + y

	// We now wish to find a y such that the maximum value of |f_y(x) - log2(x)| over the range x in [1,2] is minimized.
	// Let d_y(x) = f_y(x) - log2(x)
	//            = (0.5 + 0.5y)x^2 + (-0.5 - 1.5y)x + y - log2(x)
	// Taking the derivative, we obtain
	// d_y'(x) = (1 + y)x - (0.5 + 1.5y) - 1/(x ln(2))
	// Solving for d_y'(x) = 0 will give us the x-coordinate of either the d_y(x)'s local minimum or maximum. We find
	// this by plugging into the quadratic formula to obtain:
	// r = ((0.5 + 1.5y) +- sqrt((0.5 + 1.5y)^2 + (1 + y)*(4/ln(2)))) / (2 + 2y)

	// Now let err_0(y) = d_y(r_0) and err_1(y) = d_y(r_1). These formulas give the minimum and maximum possible values
	// of d_y(x) for a given y, i.e. they tell us the minimum and maximum error for a given y in one direction (+ or -).
	// To find the maximum absolute error, we wish to minimize the function:
	// err(y) = max(|err_0(y)|, |err_1(x)|)

	// By plotting err(y), we can see that the desired value lies between -1.75 and -1.65, so we will use those as our
	// bounds and iteratively search for the minimum. We do this by finding the point of intersection between |err_0(y)|
	// and |err_1(y)|.

	struct s_helper {
		static real64 err_0(real64 y) {
			real64 a = 1.0 + y;
			real64 b = -0.5 - 1.5*y;
			static const real64 k_c = -1.0 / std::log(2.0);

			real64 r = (-b + std::sqrt(b*b - 4.0*a*k_c)) / (2.0*a);
			return (0.5 + 0.5*y)*r*r + (-0.5 - 1.5*y)*r + y - std::log2(r);
		}

		static real64 err_1(real64 y) {
			real64 a = 1.0 + y;
			real64 b = -0.5 - 1.5*y;
			static const real64 k_c = -1.0 / std::log(2.0);

			real64 r = (-b - std::sqrt(b*b - 4.0*a*k_c)) / (2.0*a);
			return (0.5 + 0.5*y)*r*r + (-0.5 - 1.5*y)*r + y - std::log2(r);
		}
	};

	real64 left_bound = -1.75;
	real64 right_bound = -1.65;
	real64 y = 0.0;
	static const size_t k_iterations = 1000;
	for (size_t i = 0; i < k_iterations; i++) {
		y = (left_bound + right_bound) * 0.5;

		real64 left_err_0 = std::abs(s_helper::err_0(left_bound));
		real64 left_err_1 = std::abs(s_helper::err_1(left_bound));
		real64 y_err_0 = std::abs(s_helper::err_0(y));
		real64 y_err_1 = std::abs(s_helper::err_1(y));

		// The two functions intersect exactly once. Therefore, if the ordering is the same on one bound as it is in the
		// center, the intersection point must be on the other side.
		bool left_order = (left_err_0 > left_err_1);
		bool y_order = (y_err_0 > y_err_1);

		if (left_order == y_order) {
			// Shift to the right
			left_bound = y;
		} else {
			// Shift to the left
			right_bound = y;
		}
	}

	std::cout << y << "\n";

	// Determine a, b, c
	real64 a = 0.5 + 0.5*y;
	real64 b = -0.5 - 1.5*y;
	real64 c = y;

	real32 a_32 = static_cast<real32>(a);
	real32 b_32 = static_cast<real32>(b);
	real32 c_32 = static_cast<real32>(c);

	std::cout << a_32 << "\n";
	std::cout << b_32 << "\n";
	std::cout << c_32 << "\n";
}

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

int main(int argc, char **argv) {
	find_fast_log2_coefficients();
	test_fast_log2();
	return 0;
}
#endif // 0