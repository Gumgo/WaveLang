#include "common/math/math.h"

#include "engine/task_functions/sampler/fetch_sample.h"

#include <cmath>

static void split_fractional_sample_index(real64 sample_index, uint32 &int_out, real32 &fraction_out);
static real32 interpolate_samples(const s_sample_interpolation_coefficients &coefficients, real32 fraction);

// Returns the two wavetable levels to blend between for a given speed-adjusted sample rate, as well as the blend ratio.
// If the two returned wavetable levels are the same, no blending is required.
static void choose_wavetable_level(
	real32 stream_sample_rate,
	real32 baes_sample_rate,
	real32 speed,
	uint32 wavetable_count,
	uint32 &wavetable_index_a_out,
	uint32 &wavetable_index_b_out,
	real32 &wavetable_blend_ratio_out);

real32 fetch_sample(const c_sample *sample, real64 sample_index) {
	wl_assert(!sample->is_wavetable());

	const s_sample_data *sample_data = sample->get_entry(0);
	uint32 sample_index_int;
	real32 sample_index_fraction;
	split_fractional_sample_index(sample_index, sample_index_int, sample_index_fraction);
	return interpolate_samples(sample_data->samples[sample_index_int], sample_index_fraction);
}

real32 fetch_wavetable_sample(
	const c_sample *sample,
	real32 stream_sample_rate,
	real32 base_sample_rate,
	real32 speed,
	real64 sample_index) {
	// Compute the wavetable entry index and blend factor for the sample
	uint32 entry_index_a;
	uint32 entry_index_b;
	real32 blend_ratio;
	choose_wavetable_level(
		stream_sample_rate,
		base_sample_rate,
		speed,
		sample->get_entry_count(),
		entry_index_a,
		entry_index_b,
		blend_ratio);

	const s_sample_data *sample_data_a = sample->get_entry(entry_index_a);
	uint32 sample_a_index_int;
	real32 sample_a_index_fraction;
	split_fractional_sample_index(
		sample_index * sample_data_a->base_sample_rate_ratio,
		sample_a_index_int,
		sample_a_index_fraction);
	real32 sample_a = interpolate_samples(sample_data_a->samples[sample_a_index_int], sample_a_index_fraction);

	if (blend_ratio == 0.0f) {
		return sample_a;
	} else {
		const s_sample_data *sample_data_b = sample->get_entry(entry_index_b);
		uint32 sample_b_index_int;
		real32 sample_b_index_fraction;
		split_fractional_sample_index(
			sample_index * sample_data_b->base_sample_rate_ratio,
			sample_b_index_int,
			sample_b_index_fraction);
		real32 sample_b = interpolate_samples(sample_data_b->samples[sample_b_index_int], sample_b_index_fraction);

		return sample_a + (sample_b - sample_a) * blend_ratio;
	}
}

static void split_fractional_sample_index(real64 sample_index, uint32 &int_out, real32 &fraction_out) {
	// Not sure how fast this is. Maybe we could do better by utilizing SSE?
	int_out = static_cast<uint32>(sample_index);
	fraction_out = static_cast<real32>(sample_index - static_cast<real64>(int_out));
}

static real32 interpolate_samples(const s_sample_interpolation_coefficients &coefficients, real32 fraction) {
#if SIMD_IMPLEMENTATION_SSE3_ENABLED
	__m128 ones = _mm_set1_ps(1.0f);
	__m128 x = _mm_set1_ps(fraction);
	__m128 p = _mm_load_ps(coefficients.coefficients.get_elements());

	// Shuffle and multiply to get a vector containing (1, x, x^2, x^3)
	__m128 one_x_one_x = _mm_unpacklo_ps(ones, x);
	__m128 one_one_x_x = _mm_shuffle_ps(one_x_one_x, one_x_one_x, 0x50);
	__m128 one_x_x_x2 = _mm_mul_ps(one_x_one_x, one_one_x_x);
	__m128 one_x_x2_x3 = _mm_mul_ps(one_one_x_x, one_x_x_x2);
	__m128 product = _mm_mul_ps(one_x_x2_x3, p);

	__m128 sum = _mm_hadd_ps(product, product);
	sum = _mm_hadd_ps(sum, sum);
	return _mm_cvtss_f32(sum);
#elif SIMD_IMPLEMENTATION_NEON_ENABLED
#error Not yet implemented // $TODO
#else // SIMD_IMPLEMENTATION
	return coefficients.coefficients[0]
		+ (fraction * coefficients.coefficients[1]
			+ (fraction * coefficients.coefficients[2]
				+ fraction * coefficients.coefficients[3]));
#endif // SIMD_IMPLEMENTATION
}

static void choose_wavetable_level(
	real32 stream_sample_rate,
	real32 base_sample_rate,
	real32 speed,
	uint32 wavetable_count,
	uint32 &wavetable_index_a_out,
	uint32 &wavetable_index_b_out,
	real32 &wavetable_blend_ratio_out) {
	// We wish to find a wavetable entry index to sample from which avoids exceeding the nyquist frequency. We can't
	// just return a single level though, or pitch-bending would cause "pops" when we switched between levels. Instead,
	// we pick two mip levels, a and b, such that level a is the lowest level which has no frequencies exceeding the
	// stream's nyquist frequency, and b is the level above that, and blend between them.

	// For wavetable selection, if speed is 0 this will cause issues
	real32 clamped_speed = std::max(speed, 0.125f);

	// Our wavetable levels are set up such that for a wavetable with a base sampling rate of base_sample_rate, level i
	// contains frequencies up to, and not exceeding, base_sample_rate/2 - i. Therefore, for a stream sampling rate of
	// stream_sample_rate, we want to find level i such that:
	//   speed * (base_sample_rate/2 - i) <= stream_sample_rate/2
	//   base_sample_rate/2 - i <= stream_sample_rate / (2 * speed)
	//   -i <= stream_sample_rate / (2 * speed) - base_sample_rate/2
	//   i >= base_sample_rate/2 - stream_sample_rate / (2 * speed)
	//   i >= (base_sample_rate - stream_sample_rate/speed) * 0.5
	real32 wavetable_index = (base_sample_rate - stream_sample_rate / clamped_speed) * 0.5f;

	// If we blend between floor(wavetable_index) and ceil(wavetable_index), then we will still get aliasing because
	// floor(wavetable_index) has one harmonic above our nyquist limit. Therefore, to be safe, we bias up by one index.
	real32 wavetable_index_floor = std::floor(wavetable_index);
	wavetable_blend_ratio_out = wavetable_index - wavetable_index_floor;
	int32 max_wavetable_index = cast_integer_verify<int32>(wavetable_count - 1);

	int32 index_a = static_cast<int32>(wavetable_index_floor) + 1;
	wavetable_index_a_out = static_cast<uint32>(clamp(index_a, 0, max_wavetable_index));
	int32 index_b = index_a + 1;
	wavetable_index_b_out = static_cast<uint32>(clamp(index_b, 0, max_wavetable_index));
}
