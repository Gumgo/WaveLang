#include "engine/task_functions/filter/gain.h"

void c_gain::initialize(const c_real_buffer *gain) {
	m_gain = gain->get_data();
	m_is_constant = gain->is_constant();
}

void c_gain::process_in_place(real32 *input_output, size_t sample_count) {
	if (m_is_constant) {
		real32 gain = *m_gain;
		if (gain == 1.0f) {
			// Nothing to do
			return;
		}

		size_t sample_index = 0;
		size_t simd_sample_count = align_size_down(sample_count, k_simd_32_lanes);

		real32xN gain_vector(gain);
		while (sample_index < simd_sample_count) {
			real32xN input;
			input.load_unaligned(input_output);
			real32xN result = input * gain_vector;
			result.store_unaligned(input_output);

			sample_index += k_simd_32_lanes;
			input_output += k_simd_32_lanes;
		}

		while (sample_index < sample_count) {
			*input_output *= gain;
			sample_index++;
			input_output++;
		}
	} else {
		size_t sample_index = 0;
		size_t simd_sample_count = align_size_down(sample_count, k_simd_32_lanes);

		while (sample_index < simd_sample_count) {
			real32xN input;
			input.load_unaligned(input_output);
			real32xN gain_vector;
			gain_vector.load_unaligned(m_gain);
			real32xN result = input * gain_vector;
			result.store_unaligned(input_output);

			sample_index += k_simd_32_lanes;
			input_output += k_simd_32_lanes;
			m_gain += k_simd_32_lanes;
		}

		while (sample_index < sample_count) {
			*input_output *= *m_gain;
			sample_index++;
			input_output++;
			m_gain++;
		}
	}
}
