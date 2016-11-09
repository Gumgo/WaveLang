#include "engine/task_functions/sampler/fast_log2.h"
#include "engine/task_functions/sampler/fetch_sample.h"

static const real64 k_mipmap_level_sample_multipliers[] = {
	1.0,
	0.5,
	0.25,
	0.125,
	0.0625,
	0.03125,
	0.015625,
	0.0078125,
	0.00390625,
	0.001953125,
	0.0009765625,
	0.00048828125,
	0.000244140625,
	0.0001220703125,
	0.00006103515625,
	0.000030517578125
};
static_assert(NUMBEROF(k_mipmap_level_sample_multipliers) == k_max_sample_mipmap_levels,
	"Mipmap level sample multipliers mismatch");

// Returns the two mipmap levels to blend between for a given speed-adjusted sample rate, as well as the blend ratio. If
// the two returned mip levels are the same, no blending is required.
static void choose_mipmap_levels(
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, uint32 mip_count,
	c_int32_4 &out_mip_index_a, c_int32_4 &out_mip_index_b, c_real32_4 &out_mip_blend_ratio);

// Treats a and b as a contiguous block of 8 values and shifts left by the given amount; returns only the first 4
static c_real32_4 shift_left_1(const c_real32_4 &a, const c_real32_4 &b);
static c_real32_4 shift_left_2(const c_real32_4 &a, const c_real32_4 &b);
static c_real32_4 shift_left_3(const c_real32_4 &a, const c_real32_4 &b);

real32 fetch_sample(const c_sample *sample, uint32 channel, real64 sample_index) {
	wl_assert(!sample->is_mipmap());

	// Determine which sample we want and split it into fractional and integer parts
	real64 sample_index_rounded_down;
	real32 sample_index_frac_part = static_cast<real32>(std::modf(sample_index, &sample_index_rounded_down));
	uint32 sample_index_int_part = static_cast<uint32>(sample_index_rounded_down);
	sample_index_int_part += sample->get_first_sampling_frame();

	wl_assert(sample_index_int_part >= (k_sinc_window_size / 2 - 1));
	uint32 start_window_sample_index = sample_index_int_part - (k_sinc_window_size / 2 - 1);
	uint32 end_window_sample_index = start_window_sample_index + k_sinc_window_size - 1;

	// We use SSE so we need to read in 16-byte-aligned blocks. Therefore if our start sample is unaligned we must
	// perform some shifting.

	uint32 start_sse_index = align_size_down(start_window_sample_index, k_sse_block_elements);
	uint32 end_sse_index = align_size(end_window_sample_index, static_cast<uint32>(k_sse_block_elements));
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

void fetch_mipmapped_samples(const c_sample *sample, uint32 channel,
	real32 stream_sample_rate, const c_real32_4 &speed_adjusted_sample_rate_0, size_t count,
	const s_static_array<real64, k_sse_block_elements> &samples, real32 *out_ptr) {
	wl_assert(count > 0 && count <= k_sse_block_elements);

	// Compute the mip indices and blend factors for these 4 samples (if we incremented less than 4 times, some
	// are unused)
	c_int32_4 mip_index_a;
	c_int32_4 mip_index_b;
	c_real32_4 mip_blend_ratio;
	choose_mipmap_levels(stream_sample_rate, speed_adjusted_sample_rate_0, sample->get_mipmap_count(),
		mip_index_a, mip_index_b, mip_blend_ratio);

	ALIGNAS_SSE s_static_array<int32, k_sse_block_elements> mip_index_a_array;
	ALIGNAS_SSE s_static_array<int32, k_sse_block_elements> mip_index_b_array;
	mip_index_a.store(mip_index_a_array.get_elements());
	mip_index_b.store(mip_index_b_array.get_elements());

	ALIGNAS_SSE s_static_array<real32, k_sse_block_elements> samples_a_array;
	ALIGNAS_SSE s_static_array<real32, k_sse_block_elements> samples_b_array;

	for (size_t i = 0; i < count; i++) {
		if (mip_index_a_array[i] == mip_index_b_array[i]) {
			real64 sample_index = samples[i] * k_mipmap_level_sample_multipliers[mip_index_a_array[i]];
			samples_a_array[i] = samples_b_array[i] = fetch_sample(
				sample->get_mipmap_sample(mip_index_a_array[i]), channel, sample_index);
		} else {
			real64 sample_a_index = samples[i] * k_mipmap_level_sample_multipliers[mip_index_a_array[i]];
			real64 sample_b_index = samples[i] * k_mipmap_level_sample_multipliers[mip_index_b_array[i]];
			samples_a_array[i] = fetch_sample(sample->get_mipmap_sample(mip_index_a_array[i]), channel, sample_a_index);
			samples_b_array[i] = fetch_sample(sample->get_mipmap_sample(mip_index_b_array[i]), channel, sample_b_index);
		}
	}

	// Interpolate and write to the array. If increment_count < 4, we will be writing some extra (garbage
	// values) to the array, but that's okay - it's either greater than buffer size, or the remainder if the
	// buffer will be filled with 0 because we've reached the end of the loop.
	c_real32_4 samples_a(samples_a_array.get_elements());
	c_real32_4 samples_b(samples_b_array.get_elements());
	c_real32_4 samples_interpolated = samples_a + (samples_b - samples_a) * mip_blend_ratio;
	samples_interpolated.store(out_ptr);
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
	c_int32_4 max_mip_index(cast_integer_verify<int32>(mip_count - 1));
	out_mip_index_a = min(out_mip_index_a, max_mip_index);
	out_mip_index_b = min(out_mip_index_a + c_int32_4(1), max_mip_index);
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