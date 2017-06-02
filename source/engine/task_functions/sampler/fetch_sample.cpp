#include "engine/task_functions/sampler/fetch_sample.h"

#define INTERPOLATION_MODE_LINEAR 0
#define INTERPOLATION_MODE_WINDOWED_SINC 0
#define BLEND_MODE_MIN 0
#define BLEND_MODE_LINEAR 0

#if IS_TRUE(TARGET_RASPBERRY_PI)
	#undef INTERPOLATION_MODE_LINEAR
	#define INTERPOLATION_MODE_LINEAR 1
	#undef BLEND_MODE_MIN
	#define BLEND_MODE_MIN 1
#else // IS_TRUE(TARGET_RASPBERRY_PI)
	#undef INTERPOLATION_MODE_WINDOWED_SINC
	#define INTERPOLATION_MODE_WINDOWED_SINC 1
	#undef BLEND_MODE_LINEAR
	#define BLEND_MODE_LINEAR 1
#endif // IS_TRUE(TARGET_RASPBERRY_PI)

// Returns the two wavetable levels to blend between for a given speed-adjusted sample rate, as well as the blend ratio.
// If the two returned wavetable levels are the same, no blending is required.
static void choose_wavetable_levels(
	real32 stream_sample_rate, real32 sample_rate_0, const c_real32_4 &speed, uint32 wavetable_count,
	c_int32_4 &out_wavetable_index_a, c_int32_4 &out_wavetable_index_b, c_real32_4 &out_wavetable_blend_ratio);

real32 fetch_sample(const c_sample *sample, uint32 channel, real64 sample_index) {
	wl_assert(!sample->is_wavetable());

	c_wrapped_array_const<real32> sample_data = sample->get_channel_sample_data(channel);

	// Determine which sample we want and split it into fractional and integer parts
	real64 sample_index_rounded_down;
	real64 sample_index_frac_part = static_cast<real32>(std::modf(sample_index, &sample_index_rounded_down));
	uint32 sample_index_int_part = static_cast<uint32>(sample_index_rounded_down);
	sample_index_int_part += sample->get_first_sampling_frame();

#if IS_TRUE(INTERPOLATION_MODE_LINEAR)

	// Lerp between this sample and the next one
	real32 sample_a = sample_data[sample_index_int_part];
	real32 sample_b = sample_data[sample_index_int_part + 1];
	return static_cast<real32>(sample_a + (sample_b - sample_a) * sample_index_frac_part);

#elif IS_TRUE(INTERPOLATION_MODE_WINDOWED_SINC)

	wl_assert(sample_index_int_part >= (k_sinc_window_size / 2 - 1));
	uint32 start_window_sample_index = sample_index_int_part - (k_sinc_window_size / 2 - 1);
	uint32 end_window_sample_index = start_window_sample_index + k_sinc_window_size - 1;

	// We use SSE so we need to read in 16-byte-aligned blocks. Therefore if our start sample is unaligned we must
	// perform some shifting.

	uint32 start_simd_index = align_size_down(start_window_sample_index, k_simd_block_elements);
	uint32 end_simd_index = align_size(end_window_sample_index, static_cast<uint32>(k_simd_block_elements));
	const real32 *sample_ptr = &sample_data[start_simd_index];
	// Don't dereference the array directly because we may want a pointer to the very end, which is not a valid element
	wl_assert(end_simd_index <= sample_data.get_count());
	const real32 *sample_end_ptr = sample_data.get_pointer() + end_simd_index;

	uint32 simd_offset = start_window_sample_index - start_simd_index;
	wl_assert(VALID_INDEX(simd_offset, k_simd_block_elements));

	// Determine which windowed sinc lookup table to use
	real64 sinc_window_table_index = sample_index_frac_part * static_cast<real64>(k_sinc_window_sample_resolution);
	real64 sinc_window_table_index_rounded_down;
	real32 sinc_window_table_index_frac_part = static_cast<real32>(
		std::modf(sinc_window_table_index, &sinc_window_table_index_rounded_down));
	uint32 sinc_window_table_index_int_part = static_cast<uint32>(sinc_window_table_index_rounded_down);
	wl_assert(VALID_INDEX(sinc_window_table_index_int_part, k_sinc_window_sample_resolution));
	const s_sinc_window_coefficients *sinc_window_ptr = k_sinc_window_coefficients[sinc_window_table_index_int_part];

	c_real32_4 slope_multiplier(sinc_window_table_index_frac_part);
	c_real32_4 result_4(0.0f);

	switch (simd_offset) {
	case 0:
	{
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_simd_block_elements, sinc_window_ptr++) {
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
		sample_ptr += k_simd_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_simd_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = extract<1>(prev_block, curr_block);
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
		sample_ptr += k_simd_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_simd_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = extract<2>(prev_block, curr_block);
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
		sample_ptr += k_simd_block_elements;
		for (; sample_ptr < sample_end_ptr; sample_ptr += k_simd_block_elements, sinc_window_ptr++) {
			c_real32_4 curr_block(sample_ptr);
			c_real32_4 samples = extract<3>(prev_block, curr_block);
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
	ALIGNAS_SIMD real32 result[k_simd_block_elements];
	result_4_sum.store(result);
	return result[0];

#else // INTERPOLATION_MODE
#error Unknown interpolation mode
#endif // INTERPOLATION_MODE
}

void fetch_wavetable_samples(
	const c_sample *sample, uint32 channel, real32 stream_sample_rate, real32 sample_rate_0, const c_real32_4 &speed,
	size_t count, const s_static_array<real64, k_simd_block_elements> &samples, real32 *out_ptr) {
	wl_assert(count > 0 && count <= k_simd_block_elements);

	// Compute the wavetabble indices and blend factors for these 4 samples (if we incremented less than 4 times, some
	// are unused)
	c_int32_4 wavetable_index_a;
	c_int32_4 wavetable_index_b;
	c_real32_4 wavetable_blend_ratio;
	choose_wavetable_levels(stream_sample_rate, sample_rate_0, speed, sample->get_wavetable_entry_count(),
		wavetable_index_a, wavetable_index_b, wavetable_blend_ratio);

	ALIGNAS_SIMD s_static_array<int32, k_simd_block_elements> wavetable_index_a_array;
	ALIGNAS_SIMD s_static_array<int32, k_simd_block_elements> wavetable_index_b_array;
	wavetable_index_a.store(wavetable_index_a_array.get_elements());
	wavetable_index_b.store(wavetable_index_b_array.get_elements());

#if IS_TRUE(BLEND_MODE_LINEAR)
	ALIGNAS_SIMD s_static_array<real32, k_simd_block_elements> samples_a_array;
	ALIGNAS_SIMD s_static_array<real32, k_simd_block_elements> samples_b_array;
#endif // IS_TRUE(BLEND_MODE_LINEAR)

	for (size_t i = 0; i < count; i++) {
		const c_sample *sample_a = sample->get_wavetable_entry(wavetable_index_a_array[i]);

#if IS_TRUE(BLEND_MODE_MIN)

		real64 sample_index = samples[i] * sample_a->get_base_sample_rate_ratio();
		out_ptr[i] = fetch_sample(sample_a, channel, sample_index);

#elif IS_TRUE(BLEND_MODE_LINEAR)

		if (wavetable_index_a_array[i] == wavetable_index_b_array[i]) {
			real64 sample_index = samples[i] * sample_a->get_base_sample_rate_ratio();
			samples_a_array[i] = samples_b_array[i] = fetch_sample(sample_a, channel, sample_index);
		} else {
			const c_sample *sample_b = sample->get_wavetable_entry(wavetable_index_b_array[i]);

			real64 sample_a_index = samples[i] * sample_a->get_base_sample_rate_ratio();
			real64 sample_b_index = samples[i] * sample_b->get_base_sample_rate_ratio();
			samples_a_array[i] = fetch_sample(sample_a, channel, sample_a_index);
			samples_b_array[i] = fetch_sample(sample_b, channel, sample_b_index);
		}

#else // BLEND_MODE
#error Unknown blend mode
#endif // BLEND_MODE
	}

#if IS_TRUE(BLEND_MODE_LINEAR)
	// Interpolate and write to the array. If increment_count < 4, we will be writing some extra (garbage
	// values) to the array, but that's okay - it's either greater than buffer size, or the remainder if the
	// buffer will be filled with 0 because we've reached the end of the loop.
	c_real32_4 samples_a(samples_a_array.get_elements());
	c_real32_4 samples_b(samples_b_array.get_elements());
	c_real32_4 samples_interpolated = samples_a + (samples_b - samples_a) * wavetable_blend_ratio;
	samples_interpolated.store(out_ptr);
#endif // IS_TRUE(BLEND_MODE_LINEAR)
}

static void choose_wavetable_levels(
	real32 stream_sample_rate, real32 sample_rate_0, const c_real32_4 &speed, uint32 wavetable_count,
	c_int32_4 &out_wavetable_index_a, c_int32_4 &out_wavetable_index_b, c_real32_4 &out_wavetable_blend_ratio) {
	// We wish to find a wavetable entry index to sample from which avoids exceeding the nyquist frequency. We can't
	// just return a single level though, or pitch-bending would cause "pops" when we switched between levels. Instead,
	// we pick two mip levels, a and b, such that level a is the lowest level which has no frequencies exceeding the
	// stream's nyquist frequency, and b is the level above that, and blend between them.

	// For wavetable selection, if speed is 0 this will cause issues
	c_real32_4 clamped_speed = max(speed, c_real32_4(0.125f));

	// Our wavetable levels are set up such that for a wavetable with a base sampling rate of sample_rate_0, level i
	// contains frequencies up to, and not exceeding, sample_rate_0/2 - i. Therefore, for a stream sampling rate of
	// stream_sample_rate, we want to find level i such that:
	//   speed * (sample_rate_0/2 - i) <= stream_sample_rate/2
	//   sample_rate_0/2 - i <= stream_sample_rate / (2 * speed)
	//   -i <= stream_sample_rate / (2 * speed) - sample_rate_0/2
	//   i >= sample_rate_0/2 - stream_sample_rate / (2 * speed)
	//   i >= (sample_rate_0 - stream_sample_rate/speed) * 0.5
	c_real32_4 wavetable_index = (c_real32_4(sample_rate_0) - c_real32_4(stream_sample_rate) / clamped_speed) * 0.5f;

	// If we blend between floor(wavetable_index) and ceil(wavetable_index), then we will still get aliasing because
	// floor(wavetable_index) has one harmonic above our nyquist limit. Therefore, to be safe, we bias up by one index.
	c_real32_4 wavetable_index_floor = floor(wavetable_index);
	out_wavetable_blend_ratio = wavetable_index - wavetable_index_floor;
	int32 max_wavetable_index = cast_integer_verify<int32>(wavetable_count - 1);

	c_int32_4 index_a = convert_to_int32_4(wavetable_index_floor) + c_int32_4(1);
	out_wavetable_index_a = min(max(index_a, c_int32_4(0)), c_int32_4(max_wavetable_index));

#if IS_TRUE(BLEND_MODE_MIN)
	out_wavetable_index_b = out_wavetable_index_a;
#elif IS_TRUE(BLEND_MODE_LINEAR)
	c_int32_4 index_b = index_a + c_int32_4(1);
	out_wavetable_index_b = min(max(index_b, c_int32_4(0)), c_int32_4(max_wavetable_index));
#else // BLEND_MODE
#error Unknown blend mode
#endif // BLEND_MODE
}
