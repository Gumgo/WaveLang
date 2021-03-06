#include "common/math/math.h"

#include "engine/resampler/resampler.h"
#include "engine/resampler/resampler_filters.inl"

c_resampler::c_resampler(const s_resampler_parameters &parameters, c_wrapped_array<const real32> phases)
	: m_upsample_factor(static_cast<real32>(parameters.upsample_factor))
	, m_taps_per_phase(parameters.taps_per_phase)
	, m_phases(phases) {
	wl_assert(m_taps_per_phase % int32xN::k_element_count == 0);
}

real32 c_resampler::resample(
	c_wrapped_array<const real32> samples,
	size_t sample_index,
	real32 fractional_sample_index) const {
	wl_assert(fractional_sample_index >= 0.0f && fractional_sample_index < 1.0f);
	real32 upsampled_fractional_sample_index = fractional_sample_index * m_upsample_factor;
	uint32 phase_a_index = static_cast<uint32>(upsampled_fractional_sample_index); // Rounds toward zero
	uint32 phase_b_index = phase_a_index + 1;
	real32 phase_fraction = upsampled_fractional_sample_index - static_cast<real32>(phase_a_index);

	// The last tap lands on the sample at sample_index, so we add 1 to the starting index after subtracting tap count
	wl_assert(sample_index < samples.get_count());
	wl_assert(sample_index >= m_taps_per_phase - 1);
	size_t start_sample_index = sample_index - (m_taps_per_phase - 1);
	const real32 *sample_pointer = &samples[start_sample_index];

	if (phase_fraction == 0.0f) {
		const real32 *phase_coefficients_pointer = &m_phases[phase_a_index * m_taps_per_phase];

		real32xN phase_result(0.0f);

		// Apply the phase FIR
		for (uint32 tap_index = 0; tap_index < m_taps_per_phase; tap_index += int32xN::k_element_count) {
			real32xN phase_coefficients(phase_coefficients_pointer);

			real32xN sample_block;
			sample_block.load_unaligned(sample_pointer);

			phase_result += phase_coefficients * sample_block;

			phase_coefficients_pointer += int32xN::k_element_count;
			sample_pointer += int32xN::k_element_count;
		}

		return phase_result.sum_elements().first_element();
	} else {
		const real32 *phase_a_coefficients_pointer = &m_phases[phase_a_index * m_taps_per_phase];
		const real32 *phase_b_coefficients_pointer = &m_phases[phase_b_index * m_taps_per_phase];

		real32xN phase_a_result(0.0f);
		real32xN phase_b_result(0.0f);

		// Apply the two phase FIRs
		for (uint32 tap_index = 0; tap_index < m_taps_per_phase; tap_index += int32xN::k_element_count) {
			real32xN phase_a_coefficients(phase_a_coefficients_pointer);
			real32xN phase_b_coefficients(phase_b_coefficients_pointer);

			real32xN sample_block;
			sample_block.load_unaligned(sample_pointer);

			phase_a_result += phase_a_coefficients * sample_block;
			phase_b_result += phase_b_coefficients * sample_block;

			phase_a_coefficients_pointer += int32xN::k_element_count;
			phase_b_coefficients_pointer += int32xN::k_element_count;
			sample_pointer += int32xN::k_element_count;
		}

		// Because we're only performing linear operations, we can apply interpolation before calling sum_elements() so
		// we only have to perform one hadd
		real32xN interpolated_result = phase_a_result + (phase_b_result - phase_a_result) * real32xN(phase_fraction);
		return interpolated_result.sum_elements().first_element();
	}
}
