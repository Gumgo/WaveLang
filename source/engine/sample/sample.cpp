#include "common/math/math.h"
#include "common/math/math_constants.h"
#include "common/utility/aligned_allocator.h"
#include "common/utility/file_utility.h"
#include "common/utility/sha1/SHA1.h"

#include "engine/resampler/resampler.h"
#include "engine/sample/sample.h"
#include "engine/sample/sample_loader.h"

#include <iostream>

static constexpr char k_wavetable_cache_folder[] = "cache";
static constexpr char k_wavetable_cache_extension[] = ".wtc";
static constexpr char k_wavetable_cache_identifier[] = { 'w', 't', 'c', 'h' };
static constexpr uint32 k_wavetable_cache_version = 0;

// Factor to upsample samples by generating a cubic fit for interpolation
static constexpr uint32 k_interpolation_upsample_factor = 8;
static constexpr real32 k_inverse_interpolation_upsample_factor =
	1.0f / static_cast<real32>(k_interpolation_upsample_factor);

// Impose a minimum sample count to keep the interpolation error low
static constexpr uint32 k_min_wavetable_sample_count = 16;

// To interpolate samples, we perform high quality upsampling by a factor of N (using the upsampler for loaded waves and
// computed exactly for wavetables) and then fit those points to a cubic polynomial with equality constraints on the
// endpoints. The worst-case error I have seen between the upsampled points and the fit curve using this method is ~1e-4
// and -80dB aliasing magnitude. See sample_interpolation_analysis.py for details.
class c_interpolation_coefficient_solver {
public:
	c_interpolation_coefficient_solver();
	void solve(const real32 *samples, size_t stride, s_sample_interpolation_coefficients &coefficients_out);

private:
	// Include an extra sample for the right endpoint
	static constexpr uint32 k_sample_count = k_interpolation_upsample_factor + 1;
	static constexpr uint32 k_matrix_a_rows = k_sample_count - 2;
	static constexpr uint32 k_matrix_a_columns = 3;
	static constexpr uint32 k_matrix_c_rows = 1;
	static constexpr uint32 k_matrix_c_columns = 3;
	static constexpr uint32 k_matrix_rows = k_matrix_a_columns + k_matrix_c_rows;
	static constexpr uint32 k_matrix_columns = k_matrix_rows;
	static constexpr uint32 k_vector_elements = k_matrix_rows;

	s_static_array<real32, k_matrix_a_rows *k_matrix_a_columns> m_matrix_a;
	s_static_array<real32, k_matrix_rows *k_matrix_columns> m_matrix;
};

template<uint32 k_n>
void solve_system_using_lu_decomposition(
	c_wrapped_array<const real32> matrix,
	c_wrapped_array<const real32> vector,
	c_wrapped_array<real32> solution);

// required_sample_count_out holds the number of samples required to support all frequencies
// actual_sample_count_out may be more than this because we impose a minimum sample count for interpolation quality
static void calculate_wavetable_level_sample_count(
	uint32 level,
	uint32 previous_level_required_sample_count,
	real32 harmonic_weight,
	uint32 &required_sample_count_out,
	uint32 &actual_sample_count_out);

static std::string hash_wavetable_parameters(c_wrapped_array<const real32> harmonic_weights, bool phase_shift_enabled);

static bool read_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	bool phase_shift_enabled,
	c_wrapped_array<s_sample_interpolation_coefficients> out_sample_data);

static bool write_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	bool phase_shift_enabled,
	c_wrapped_array<const s_sample_interpolation_coefficients> sample_data);

bool c_sample::load_file(
	const char *filename,
	e_sample_loop_mode loop_mode,
	bool phase_shift_enabled,
	std::vector<c_sample *> &channel_samples_out) {
	wl_assert(channel_samples_out.empty());
	s_loaded_sample loaded_sample;
	if (!load_sample(filename, loaded_sample)) {
		return false;
	}

	channel_samples_out.reserve(loaded_sample.channel_count);

	// With regular looping, we sample up to the end of the loop, and then we duplicate some samples for padding. We do
	// this because if we're sampling using a window size of N, then the first time we loop, we actually want our window
	// to touch the pre-loop samples. However, once we've looped enough (usually just once if the loop is long enough)
	// we want our window to "wrap" at the edges. In other words, we want both endpoints of our loop to appear to be
	// identical for the maximum window size, so we shift the loop points forward in time.
	// Key: I = intro samples, L = loop samples, T = intro-to-loop-transition samples (beginning of the loop),
	// P = loop padding
	// Orig:  [IIII][LLLLLLLLLLLLLLLL]
	// Final: [IIII][TT][LLLLLLLLLLLL][PP]

	c_interpolation_coefficient_solver coefficient_solver;

	const s_resampler_parameters &resampler_parameters =
		get_resampler_parameters(e_resampler_filter::k_upsample_high_quality);
	c_resampler resampler(resampler_parameters);
	uint32 required_history_samples = c_resampler::get_required_history_samples(resampler_parameters);

	// Resampler details:
	// In order to sample the 0th sample index, we need to advance by our latency. Therefore, we must pad the beginning
	// by required_history_samples - latency:
	// ..01234567 <-- sample indices
	//   ^
	// 543210 <------ resampler coefficients, latency = 3, history length = 5

	// For a non-looping signal, in order to sample at the last index, we need to pad by the latency:
	// 01234567... <-- sample indices
	//        ^
	//      543210 <------ resampler coefficients, latency = 3, history length = 5

	// For a looping signal, we must first push back the loop point by required_history_samples - latency. This is
	// because once we've looped for the first time, we don't want to sample from pre-loop samples anymore:
	//        |        |
	// 0123456789012345  <-- sample indices
	//      >>^
	//      543210 <-------- resampler coefficients, latency = 3, history length = 5

	// Finally, in order to sample from the end of the loop, we must pad with the latency:
	//          |        |
	// 012345678901234567... <-- sample indices
	//                  ^
	//                543210 <-------- resampler coefficients, latency = 3, history length = 5

	uint32 initial_padding = required_history_samples - resampler_parameters.latency;
	uint32 final_padding = resampler_parameters.latency;
	uint32 content_samples = 0;
	if (loop_mode == e_sample_loop_mode::k_none) {
		content_samples = loaded_sample.frame_count;
	} else {
		content_samples = loaded_sample.loop_start_sample_index;
		uint32 loop_samples = loaded_sample.loop_end_sample_index - loaded_sample.loop_start_sample_index;

		if (loop_mode == e_sample_loop_mode::k_bidi_loop && loop_samples > 2) {
			loop_samples += loop_samples - 2;
		}

		if (phase_shift_enabled) {
			loop_samples *= 2;
		}

		content_samples += loop_samples + initial_padding;
	}

	// Pad with 1 extra sample for the final set of interpolation coefficients. This is because each set of
	// interpolation coefficients requires samples [i, i + n] where i is the starting sample index and n is the upsample
	// factor, so the final set needs one additional input sample due to the inclusive nature of the range's end bound.
	std::vector<real32> padded_samples(initial_padding + content_samples + final_padding + 1, 0.0f);
	for (uint32 channel_index = 0; channel_index < loaded_sample.channel_count; channel_index++) {
		c_wrapped_array<const real32> channel_samples(
			&loaded_sample.samples[channel_index * loaded_sample.frame_count],
			loaded_sample.frame_count);

		c_sample *sample = new c_sample();
		sample->m_type = e_type::k_single_sample;
		sample->m_sample_rate = loaded_sample.sample_rate;
		sample->m_looping = loop_mode != e_sample_loop_mode::k_none;
		sample->m_phase_shift_enabled = phase_shift_enabled;

		if (loop_mode == e_sample_loop_mode::k_none) {
			// Simply copy the non-looping content - beginning and end don't need to be re-zeroed
			copy_type(
				&padded_samples[initial_padding],
				channel_samples.get_pointer(),
				loaded_sample.frame_count);

			sample->m_loop_start = 0;
			sample->m_loop_end = content_samples;
		} else {
			// Copy content until the end of the loop
			copy_type(
				&padded_samples[initial_padding],
				channel_samples.get_pointer(),
				loaded_sample.loop_end_sample_index);

			uint32 loop_samples = loaded_sample.loop_end_sample_index - loaded_sample.loop_start_sample_index;

			if (loop_mode == e_sample_loop_mode::k_bidi_loop && loop_samples > 2) {
				// Copy and reverse the loop if bidi looping is enabled
				uint32 bidi_start_sample_index = initial_padding + loaded_sample.loop_end_sample_index;
				for (uint32 bidi_index = 0; bidi_index < loop_samples - 2; bidi_index++) {
					padded_samples[bidi_start_sample_index + bidi_index] =
						padded_samples[bidi_start_sample_index - bidi_index - 1];
				}

				loop_samples += loop_samples - 2;
			}

			// Pad by extending the loop
			uint32 loop_end_sample_index = loaded_sample.loop_start_sample_index + loop_samples;
			uint32 loop_padding = initial_padding + final_padding;
			if (phase_shift_enabled) {
				loop_padding += loop_samples;
			}

			// Extend the loop by 1 extra sample for the final set of resampler coefficients
			for (uint32 index = 0; index < loop_padding + 1; index++) {
				padded_samples[loop_end_sample_index + index] =
					padded_samples[loop_end_sample_index + index - loop_samples];
			}

			sample->m_loop_start = loaded_sample.loop_start_sample_index + initial_padding;
			sample->m_loop_end = sample->m_loop_start + loop_samples;
		}

		sample->m_samples.resize(content_samples);

		// Use the resampler to upsample the signal
		c_wrapped_array<const real32> resampler_input(&padded_samples.front(), padded_samples.size());
		for (uint32 sample_index = 0; sample_index < content_samples; sample_index++) {
			s_static_array<real32, k_interpolation_upsample_factor + 1> upsampled_samples;
			for (uint32 upsample_index = 0; upsample_index <= k_interpolation_upsample_factor; upsample_index++) {
				uint32 upsample_index_fraction = upsample_index & (k_interpolation_upsample_factor - 1);
				uint32 sample_index_offset = upsample_index / k_interpolation_upsample_factor;
				real32 fraction =
					static_cast<real32>(upsample_index_fraction) * k_inverse_interpolation_upsample_factor;
				upsampled_samples[upsample_index] = resampler.resample(
					resampler_input,
					sample_index + sample_index_offset + required_history_samples,
					fraction);
			}

			// Calculate the coefficients
			coefficient_solver.solve(upsampled_samples.get_elements(), 1, sample->m_samples[sample_index]);
		}

		sample->m_entries.push_back(s_sample_data());
		s_sample_data &entry = sample->m_entries.back();
		entry.base_sample_rate_ratio = 1.0f;
		entry.samples = c_wrapped_array<const s_sample_interpolation_coefficients>(
			&sample->m_samples.front(),
			sample->m_samples.size());


		channel_samples_out.push_back(sample);
	}

	return true;
}

c_sample *c_sample::generate_wavetable(c_wrapped_array<const real32> harmonic_weights, bool phase_shift_enabled) {
	// Determine the number of samples needed to support the each level in the wavetable.
	bool any_nonzero_weights = false;
	uint32 total_sample_count = 0;
	uint32 previous_level_required_sample_count = 0;
	uint32 max_level_actual_sample_count = 0;
	uint32 max_level_required_sample_count = 0;
	for (uint32 level = 0; level < harmonic_weights.get_count(); level++) {
		any_nonzero_weights |= (harmonic_weights[level] != 0.0f);
		uint32 required_sample_count;
		uint32 actual_sample_count;
		calculate_wavetable_level_sample_count(
			level,
			previous_level_required_sample_count,
			harmonic_weights[level],
			required_sample_count,
			actual_sample_count);

		total_sample_count += actual_sample_count;
		max_level_required_sample_count = std::max(required_sample_count, max_level_required_sample_count);
		max_level_actual_sample_count = std::max(actual_sample_count, max_level_actual_sample_count);
		previous_level_required_sample_count = required_sample_count;
	}

	if (!any_nonzero_weights) {
		// No weights specified or they were all 0 - that's an error
		return nullptr;
	}

	c_sample *sample = new c_sample();
	sample->m_type = e_type::k_wavetable;
	sample->m_sample_rate = max_level_required_sample_count;
	sample->m_looping = true;
	sample->m_loop_start = 0;
	sample->m_loop_end = max_level_required_sample_count;
	sample->m_phase_shift_enabled = phase_shift_enabled;

	// If phase shift is enabled, we duplicate the entire loop
	uint32 phase_shift_sample_count_multiplier = phase_shift_enabled ? 2 : 1;
	sample->m_samples.resize(total_sample_count * phase_shift_sample_count_multiplier);

	sample->m_entries.resize(harmonic_weights.get_count());

	bool loaded_from_cache = read_wavetable_cache(
		harmonic_weights,
		phase_shift_enabled,
		c_wrapped_array<s_sample_interpolation_coefficients>(&sample->m_samples.front(), sample->m_samples.size()));
	if (!loaded_from_cache) {
		std::cout << "Generating wavetable\n";
	}

	// Allocate space for the upsampled versions of each level - calculate one extra sample for wrapping at the end
	uint32 upsampled_signal_length = max_level_actual_sample_count * k_interpolation_upsample_factor + 1;
	std::vector<real32> upsampled_signal(upsampled_signal_length, 0.0f);

	// Pre-calculate all sine values
	uint32 sine_buffer_length = max_level_actual_sample_count * k_interpolation_upsample_factor;
	c_aligned_allocator<real32, k_simd_alignment> sine_buffer;

	if (!loaded_from_cache) {
		sine_buffer.allocate(sine_buffer_length);
		real32xN sine_index_multiplier(2.0f * k_pi<real32> / static_cast<real32>(sine_buffer_length));
#if IS_TRUE(SIMD_256_ENABLED)
		real32xN sine_index_offsets(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
#elif IS_TRUE(SIMD_128_ENABLED)
		real32xN sine_index_offsets(0.0f, 1.0f, 2.0f, 3.0f);
#else // SIMD
#error Single element SIMD type not supported // $TODO $SIMD real32x1 fallback
#endif // SIMD
		for (uint32 sine_index = 0; sine_index < sine_buffer_length; sine_index += k_simd_32_lanes) {
			real32xN sine_indices = real32xN(static_cast<real32>(sine_index)) + sine_index_offsets;
			real32xN sine_result = sin(sine_indices * sine_index_multiplier);
			sine_result.store(&sine_buffer.get_array()[sine_index]);
		}
	}

	c_interpolation_coefficient_solver coefficient_solver;

	previous_level_required_sample_count = 0;
	uint32 previous_level_actual_sample_count = 0;
	uint32 next_entry_start_sample_index = 0;
	for (uint32 level = 0; level < harmonic_weights.get_count(); level++) {
		uint32 required_sample_count;
		uint32 actual_sample_count;
		calculate_wavetable_level_sample_count(
			level,
			previous_level_required_sample_count,
			harmonic_weights[level],
			required_sample_count,
			actual_sample_count);

		// If sample_count is 0, we just use the previous level's data. Otherwise, we have to calculate it.
		if (!loaded_from_cache && actual_sample_count != 0) {
			// Add this level's harmonic data to the full upsampled signal
			real32 harmonic_weight = harmonic_weights[level];
			if (harmonic_weight != 0.0f) {
				uint32 frequency = level + 1;
				uint32 sine_buffer_index = 0;
				for (uint32 sample_index = 0; sample_index < upsampled_signal_length; sample_index++) {
					upsampled_signal[sample_index] += sine_buffer.get_array()[sine_buffer_index] * harmonic_weight;
					sine_buffer_index += frequency;
					if (sine_buffer_index >= sine_buffer_length) {
						// One subtraction should be enough (rather than mod) because we never exceed more than half a
						// sine cycle per sample due to nyquist limits (less than that actually since we're upsampled)
						sine_buffer_index -= sine_buffer_length;
						wl_assert(sine_buffer_index < sine_buffer_length);
					}
				}
			}

			// Grab the list of coefficients to calculate
			c_wrapped_array<s_sample_interpolation_coefficients> coefficients(
				&sample->m_samples[next_entry_start_sample_index],
				actual_sample_count);

			// Calculate the coefficients - iterate using a stride so we only consider the desired number of samples
			uint32 stride = max_level_actual_sample_count / actual_sample_count;
			for (uint32 sample_index = 0; sample_index < actual_sample_count; sample_index++) {
				coefficient_solver.solve(
					&upsampled_signal[sample_index * stride * k_interpolation_upsample_factor],
					stride,
					coefficients[sample_index]);
			}

			if (phase_shift_enabled) {
				// Copy the loop to support phase shifting
				copy_type(
					&sample->m_samples[next_entry_start_sample_index + actual_sample_count],
					&sample->m_samples[next_entry_start_sample_index],
					actual_sample_count);
			}
		}

		// We're constructing entries in reverse - the first one has the highest detail
		s_sample_data &entry = sample->m_entries[harmonic_weights.get_count() - level - 1];
		if (actual_sample_count != 0) {
			entry.base_sample_rate_ratio =
				static_cast<real32>(actual_sample_count) / static_cast<real32>(max_level_required_sample_count);
			entry.samples = c_wrapped_array<s_sample_interpolation_coefficients>(
				&sample->m_samples[next_entry_start_sample_index],
				actual_sample_count * phase_shift_sample_count_multiplier);
		} else {
			// Copy the previous entry
			entry = sample->m_entries[harmonic_weights.get_count() - level];
		}

		previous_level_required_sample_count = required_sample_count;
		previous_level_actual_sample_count = actual_sample_count;
		next_entry_start_sample_index += actual_sample_count * phase_shift_sample_count_multiplier;
	}

	if (!loaded_from_cache) {
		write_wavetable_cache(
			harmonic_weights,
			phase_shift_enabled,
			c_wrapped_array<s_sample_interpolation_coefficients>(&sample->m_samples.front(), sample->m_samples.size()));
	}

	return sample;
}

bool c_sample::is_wavetable() const {
	return m_type == e_type::k_wavetable;
}

uint32 c_sample::get_sample_rate() const {
	return m_sample_rate;
}

bool c_sample::is_looping() const {
	return m_looping;
}

uint32 c_sample::get_loop_start() const {
	return m_loop_start;
}

uint32 c_sample::get_loop_end() const {
	return m_loop_end;
}

bool c_sample::is_phase_shift_enabled() const {
	return m_phase_shift_enabled;
}

uint32 c_sample::get_entry_count() const {
	return cast_integer_verify<uint32>(m_entries.size());
}

const s_sample_data *c_sample::get_entry(uint32 index) const {
	return &m_entries[index];
}

c_interpolation_coefficient_solver::c_interpolation_coefficient_solver() {
	// We want to approximate n samples using a cubic polynomial of the form p0 + p1*x + p2*x^2 + p3*x^3 for x in
	// [0, 1]. The n samples {(sx_0, sy_0), ..., (sx_{n-1}, sy_{n-1})} are linearly spread across the unit interval:
	// sx_i = i/(n-1). Note that sx_0 = 0 and sx_{n-1} = 1. Additionally, we constrain the polynomial to pass exactly
	// through the left and right endpoints. We apply the left constraint by subtracting the leftmost sample from all
	// samples and removing the constant term p0. We are left with f(x) = p1*x + p2*x^2 + p3*x^3, and the constraint
	// f(1) = sy_{n-1}.

	// We perform this task using constrained least squares: http://www.seas.ucla.edu/~vandenbe/133A/lectures/cls.pdf
	// Solve [ A^T A   C^T ] [ x ] = [ A^T b  ]
	//       [   C      0  ] [ z ]   [   d    ]
	// where f(x) = |Ax - b|^2 is the function we wish to minimize and C^T_i x = d_i are constraints. The matrix A then
	// becomes a matrix of rows (sx_i, sx_i^2, sx_i^3) for i from 1 to n-2:
	// [   sx_1       sx_1^2       sx_1^3   ]
	// [   sx_2       sx_2^2       sx_2^3   ]
	// [               ...                  ]
	// [ sx_{n-2}   sx_{n-2}^2   sx_{n-2}^3 ]
	// We exclude samples 0 and n-1 because sample 0 has been accounted for by removing the constant term p0 and we will
	// add an equality constraint for sample n-1. Our vector b is then [ sy_1 sy_2 ... sy_{n-2} ]. We have only one
	// constraint, sample n-1, so our matrix C is simply [ 1 1 1 ] and d is sy_{n-1}.

	// Construct our matrix once up-front and reuse it each time solve() is called
	// Start by constructing A
	for (uint32 row = 0; row < k_sample_count - 2; row++) {
		real32 sample_x = static_cast<real32>(row + 1) / static_cast<real32>(k_sample_count - 1);
		real32 sample_x2 = sample_x * sample_x;
		m_matrix_a[row * k_matrix_a_columns] = sample_x;
		m_matrix_a[row * k_matrix_a_columns + 1] = sample_x2;
		m_matrix_a[row * k_matrix_a_columns + 2] = sample_x * sample_x2;
	}

	// Place A^T * A in the upper left
	for (uint32 row = 0; row < k_matrix_a_columns; row++) {
		for (uint32 column = 0; column < k_matrix_a_columns; column++) {
			real32 sum = 0.0f;
			for (uint32 i = 0; i < k_matrix_a_rows; i++) {
				sum += m_matrix_a[i * k_matrix_a_columns + row] * m_matrix_a[i * k_matrix_a_columns + column];
			}
			m_matrix[row * k_matrix_columns + column] = sum;
		}
	}

	// Place C^T in the upper right and C in the lower left
	for (uint32 c_column = 0; c_column < k_matrix_c_columns; c_column++) {
		m_matrix[k_matrix_columns * c_column + (k_matrix_columns - 1)] = 1.0f;
		m_matrix[k_matrix_columns * (k_matrix_rows - 1) + c_column] = 1.0f;
	}
}

void c_interpolation_coefficient_solver::solve(
	const real32 *samples,
	size_t stride,
	s_sample_interpolation_coefficients &coefficients_out) {
	// Remove constant coefficient
	real32 sample_0 = *samples;
	s_static_array<real32, k_sample_count - 1> offset_samples;
	for (uint32 sample_index = 0; sample_index < k_sample_count - 1; sample_index++) {
		offset_samples[sample_index] = samples[(sample_index + 1) * stride] - sample_0;
	}

	// Construct the vector [ A^T b   1 ]
	s_static_array<real32, k_vector_elements> vector;
	for (uint32 vector_row = 0; vector_row < k_matrix_a_columns; vector_row++) {
		real32 sum = 0.0f;
		for (uint32 i = 0; i < k_matrix_a_rows; i++) {
			sum += m_matrix_a[i * k_matrix_a_columns + vector_row] * offset_samples[i];
		}
		vector[vector_row] = sum;
	}

	vector[k_matrix_a_columns] = offset_samples[k_sample_count - 2];

	// Solve for the coefficients
	s_static_array<real32, k_vector_elements> solution;
	solve_system_using_lu_decomposition<k_matrix_rows>(
		c_wrapped_array<const real32>(m_matrix.get_elements(), m_matrix.get_count()),
		c_wrapped_array<const real32>(vector.get_elements(), vector.get_count()),
		c_wrapped_array<real32>(solution.get_elements(), solution.get_count()));

	coefficients_out.coefficients[0] = sample_0;
	copy_type(&coefficients_out.coefficients[1], solution.get_elements(), 3);
}

template<uint32 k_n>
void solve_system_using_lu_decomposition(c_wrapped_array<const real32> matrix, c_wrapped_array<const real32> vector, c_wrapped_array<real32> solution) {
	// https://en.wikipedia.org/wiki/LU_decomposition
	wl_assert(matrix.get_count() == k_n * k_n);
	wl_assert(vector.get_count() == k_n);
	wl_assert(solution.get_count() == k_n);

	s_static_array<real32, k_n * k_n> lu;
	zero_type(&lu);
	for (uint32 i = 0; i < k_n; i++) {
		for (uint32 j = i; j < k_n; j++) {
			real32 sum = 0.0f;
			for (uint32 k = 0; k < i; k++) {
				sum += lu[i * k_n + k] * lu[k * k_n + j];
			}

			lu[i * k_n + j] = matrix[i * k_n + j] - sum;
		}

		for (uint32 j = i + 1; j < k_n; j++) {
			real32 sum = 0.0f;
			for (uint32 k = 0; k < i; k++) {
				sum += lu[j * k_n + k] * lu[k * k_n + i];
			}

			wl_assert(lu[i * k_n + i] != 0.0f); // Make sure the matrix is invertible
			lu[j * k_n + i] = (matrix[j * k_n + i] - sum) / lu[i * k_n + i];
		}
	}

	// lu = L + U - I
	// Find solution of Ly = b
	s_static_array<real32, k_n> y;
	zero_type(&y);
	for (uint32 i = 0; i < k_n; i++) {
		real32 sum = 0.0f;
		for (uint32 k = 0; k < i; k++) {
			sum += lu[i * k_n + k] * y[k];
		}

		y[i] = vector[i] - sum;
	}

	// Find solution of Ux = y
	zero_type(solution.get_pointer(), k_n);
	for (uint32 j = 0; j < k_n; j++) {
		uint32 i = k_n - j - 1;
		real32 sum = 0.0f;
		for (uint32 k = i + 1; k < k_n; k++) {
			sum += lu[i * k_n + k] * solution[k];
		}

		wl_assert(lu[i * k_n + i] != 0.0f); // Make sure the matrix is invertible
		solution[i] = (y[i] - sum) / lu[i * k_n + i];
	}
}

static void calculate_wavetable_level_sample_count(
	uint32 level,
	uint32 previous_level_required_sample_count,
	real32 harmonic_weight,
	uint32 &required_sample_count_out,
	uint32 &actual_sample_count_out) {
	uint32 sample_count;
	if (harmonic_weight == 0.0) {
		if (previous_level_required_sample_count == 0) {
			// This is the first level. There's no harmonic data here, but we need samples.
			required_sample_count_out = 1;
			actual_sample_count_out = k_min_wavetable_sample_count;
		} else {
			// No harmonic data was added, so we can reuse the previous level's sample data
			required_sample_count_out = previous_level_required_sample_count;
			actual_sample_count_out = 0;
		}
	} else {
		// Since this level includes all previous levels, we need at least as many samples as the previous level
		sample_count = std::max(previous_level_required_sample_count, 1u);

		// A single sine cycle needs more than 2 samples - just 2 isn't enough because our sines have 0 phase
		uint32 frequency = level + 1;
		while (sample_count < frequency * 2) {
			sample_count *= 2;
		}

		required_sample_count_out = sample_count;
		actual_sample_count_out = std::max(sample_count, k_min_wavetable_sample_count);
	}
}

static std::string hash_wavetable_parameters(c_wrapped_array<const real32> harmonic_weights, bool phase_shift_enabled) {
	CSHA1 hash;
	uint32 harmonic_weights_count = cast_integer_verify<uint32>(harmonic_weights.get_count());
	hash.Update(
		reinterpret_cast<const uint8 *>(&harmonic_weights_count),
		sizeof(harmonic_weights_count));
	hash.Update(
		reinterpret_cast<const uint8 *>(harmonic_weights.get_pointer()),
		cast_integer_verify<uint32>(sizeof(harmonic_weights[0]) * harmonic_weights.get_count()));
	hash.Update(
		reinterpret_cast<const uint8 *>(&phase_shift_enabled),
		sizeof(phase_shift_enabled));
	hash.Final();

	std::string hash_string;
	hash.ReportHashStl(hash_string, CSHA1::REPORT_HEX_SHORT);
	return hash_string;
}

static bool read_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	bool phase_shift_enabled,
	c_wrapped_array<s_sample_interpolation_coefficients> out_sample_data) {
	std::string hash_string = hash_wavetable_parameters(harmonic_weights, phase_shift_enabled);
	std::string wavetable_cache_filename = k_wavetable_cache_folder;
	wavetable_cache_filename += '/';
	wavetable_cache_filename += hash_string;
	wavetable_cache_filename += k_wavetable_cache_extension;

	bool result = true;

	std::ifstream file(wavetable_cache_filename, std::ios::binary);
	if (!file.is_open()) {
		result = false;
	}

	if (result) {
		uint32 file_identifier;
		file.read(reinterpret_cast<char *>(&file_identifier), sizeof(file_identifier));
		if (file.fail() || memcmp(&file_identifier, k_wavetable_cache_identifier, sizeof(file_identifier)) != 0) {
			result = false;
		}
	}

	if (result) {
		uint32 file_version;
		file.read(reinterpret_cast<char *>(&file_version), sizeof(file_version));
		if (file.fail() || file_version != k_wavetable_cache_version) {
			result = false;
		}
	}

	uint32 file_harmonic_weights_count;
	uint32 file_phase_shift_enabled;

	if (result) {
		file.read(reinterpret_cast<char *>(&file_harmonic_weights_count), sizeof(file_harmonic_weights_count));
		if (file.fail() || file_harmonic_weights_count != harmonic_weights.get_count()) {
			result = false;
		}
	}

	if (result) {
		if (file_harmonic_weights_count > 0) {
			std::vector<real32> file_harmonic_weights(file_harmonic_weights_count);
			size_t harmonic_weights_size = sizeof(file_harmonic_weights[0]) * file_harmonic_weights.size();
			file.read(reinterpret_cast<char *>(&file_harmonic_weights.front()), harmonic_weights_size);
			if (file.fail()
				|| memcmp(&file_harmonic_weights.front(), harmonic_weights.get_pointer(), harmonic_weights_size) != 0) {
				result = false;
			}
		}
	}

	if (result) {
		file.read(reinterpret_cast<char *>(&file_phase_shift_enabled), sizeof(file_phase_shift_enabled));
		if (file.fail() || file_phase_shift_enabled != static_cast<uint32>(phase_shift_enabled)) {
			result = false;
		}
	}

	if (result) {
		// Read sample data if everything has matched so far
		file.read(
			reinterpret_cast<char *>(out_sample_data.get_pointer()),
			sizeof(out_sample_data[0]) * out_sample_data.get_count());
		if (file.fail()) {
			result = false;
		}
	}

	if (result) {
		std::cout << "Loaded wavetable '" << wavetable_cache_filename << "'\n";
	} else {
		std::cout << "Failed to load wavetable '" << wavetable_cache_filename << "'\n";
	}
	return result;
}

static bool write_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	bool phase_shift_enabled,
	c_wrapped_array<const s_sample_interpolation_coefficients> sample_data) {
	std::string hash_string = hash_wavetable_parameters(harmonic_weights, phase_shift_enabled);
	std::string wavetable_cache_filename = k_wavetable_cache_folder;
	wavetable_cache_filename += '/';
	wavetable_cache_filename += hash_string;
	wavetable_cache_filename += k_wavetable_cache_extension;

	create_directory(k_wavetable_cache_folder);

	std::ofstream file(wavetable_cache_filename, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	file.write(reinterpret_cast<const char *>(k_wavetable_cache_identifier), sizeof(k_wavetable_cache_identifier));
	file.write(reinterpret_cast<const char *>(&k_wavetable_cache_version), sizeof(k_wavetable_cache_version));

	uint32 file_harmonic_weights_count = cast_integer_verify<uint32>(harmonic_weights.get_count());
	uint32 file_phase_shift_enabled = phase_shift_enabled ? 1 : 0;

	file.write(reinterpret_cast<const char *>(&file_harmonic_weights_count), sizeof(file_harmonic_weights_count));

	size_t harmonic_weights_size = sizeof(harmonic_weights[0]) * harmonic_weights.get_count();
	file.write(reinterpret_cast<const char *>(harmonic_weights.get_pointer()), harmonic_weights_size);
	file.write(reinterpret_cast<const char *>(&file_phase_shift_enabled), sizeof(file_phase_shift_enabled));

	// Write sample data
	file.write(
		reinterpret_cast<const char *>(sample_data.get_pointer()),
		sizeof(sample_data[0]) * sample_data.get_count());

	if (file.fail()) {
		std::cout << "Failed to save wavetable '" << wavetable_cache_filename << "'\n";
	} else {
		std::cout << "Saved wavetable '" << wavetable_cache_filename << "'\n";
	}

	return !file.fail();
}
