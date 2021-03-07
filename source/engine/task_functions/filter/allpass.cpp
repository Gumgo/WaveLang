#include "common/math/math.h"

#include "engine/task_functions/filter/allpass.h"

void c_allpass::calculate_memory(uint32 delay, c_stack_allocator::c_memory_calculator &memory_calculator) {
	wl_assert(delay > 0);

	// Pad the end of the history buffer with values duplicated from the beginning. SIMD alignment isn't needed because
	// the delay value might not be a multiple of the SIMD vector size so we need to do unaligned loads.
	memory_calculator.add_array<real32>(delay + k_simd_32_lanes - 1);
}

void c_allpass::initialize(uint32 delay, real32 gain, c_stack_allocator &allocator) {
	wl_assert(delay > 0);
	m_delay = delay;
	m_gain = gain;

	allocator.allocate_array(m_history_buffer, delay + k_simd_32_lanes - 1);

	m_history_index = 0;
	m_zero_count = delay;
}

void c_allpass::reset() {
	zero_type(m_history_buffer.get_pointer(), m_history_buffer.get_count());
	m_history_index = 0;
	m_zero_count = m_delay;
}

void c_allpass::process(const real32 *input, real32 *output, size_t sample_count) {
	m_zero_count = 0;

	// If our delay is smaller than the SIMD vector size, fall back to processing 1 sample at a time
	size_t simd_sample_count = (m_delay < k_simd_32_lanes) ? 0 : (sample_count & (k_simd_32_lanes - 1));

	uint32 delay = m_delay;
	size_t history_index = m_history_index;
	size_t sample_index = 0;

	{
		real32xN gain(m_gain);
		for (; sample_index < simd_sample_count; sample_index++) {
			real32xN history_value;
			history_value.load_unaligned(&m_history_buffer[history_index]);
			real32xN input_value;
			input_value.load_unaligned(&input[sample_index]); // Unaligned because this can be embedded in comb filters
			real32xN v = input_value - gain * history_value;
			real32xN result = gain * v + history_value;
			result.store(&output[sample_index]);

			// Update the history buffer
			v.store_unaligned(&m_history_buffer[history_index]);

			// Duplicate elements back to the beginning
			if (history_index + k_simd_32_lanes > delay) {
				size_t copy_count = history_index + k_simd_32_lanes - delay;
				copy_type(&m_history_buffer[0], &m_history_buffer[delay], copy_count);
			}

			// Duplicate elements into the extension zone
			if (history_index < k_simd_32_lanes) {
				size_t copy_count = k_simd_32_lanes - history_index;
				copy_type(
					&m_history_buffer[m_history_buffer.get_count() - copy_count],
					&m_history_buffer[history_index],
					copy_count);
			}

			history_index += k_simd_32_lanes;
			history_index = (history_index >= delay) ? history_index - delay : history_index;
		}
	}

	{
		real32 gain = m_gain;
		for (; sample_index < sample_count; sample_index++) {
			real32 history_value = m_history_buffer[history_index];
			real32 v = input[sample_index] - gain * history_value;
			output[sample_index] = gain * v + history_value;

			// Update the history buffer
			m_history_buffer[history_index] = v;

			history_index++;
			history_index = (history_index == delay) ? 0 : history_index;
		}

		if constexpr (k_simd_32_lanes > 1) {
			// The above loop doesn't use history duplication so just duplicate once at the end
			copy_type(&m_history_buffer[delay], &m_history_buffer[0], k_simd_32_lanes - 1);
		}
	}

	m_history_index = history_index;
}

bool c_allpass::process_constant(real32 input, real32 *output, size_t sample_count) {
	if (input == 0.0f) {
		if (m_zero_count >= m_delay) {
			// All coefficients are being multiplied by 0
			*output = 0.0f;
			return true;
		} else {
			m_zero_count += sample_count;
		}
	} else {
		m_zero_count = 0;
	}

	// If our delay is smaller than the SIMD vector size, fall back to processing 1 sample at a time
	size_t simd_sample_count = (m_delay < k_simd_32_lanes) ? 0 : (sample_count & (k_simd_32_lanes - 1));

	uint32 delay = m_delay;
	size_t history_index = m_history_index;
	size_t sample_index = 0;

	{
		real32xN gain(m_gain);
		real32xN input_value(input);
		for (; sample_index < simd_sample_count; sample_index++) {
			real32xN history_value;
			history_value.load_unaligned(&m_history_buffer[history_index]);
			real32xN v = input_value - gain * history_value;
			real32xN result = gain * v + history_value;
			result.store(&output[sample_index]);

			// Update the history buffer
			v.store_unaligned(&m_history_buffer[history_index]);

			// Duplicate elements back to the beginning
			if (history_index + k_simd_32_lanes > delay) {
				size_t copy_count = history_index + k_simd_32_lanes - delay;
				copy_type(&m_history_buffer[0], &m_history_buffer[delay], copy_count);
			}

			// Duplicate elements into the extension zone
			if (history_index < k_simd_32_lanes) {
				size_t copy_count = k_simd_32_lanes - history_index;
				copy_type(
					&m_history_buffer[m_history_buffer.get_count() - copy_count],
					&m_history_buffer[history_index],
					copy_count);
			}

			history_index += k_simd_32_lanes;
			history_index = (history_index >= delay) ? history_index - delay : history_index;
		}
	}

	{
		real32 gain = m_gain;
		for (; sample_index < sample_count; sample_index++) {
			real32 history_value = m_history_buffer[history_index];
			real32 v = input - gain * history_value;
			output[sample_index] = gain * v + history_value;

			// Update the history buffer
			m_history_buffer[history_index] = v;

			history_index++;
			history_index = (history_index == delay) ? 0 : history_index;
		}

		if constexpr (k_simd_32_lanes > 1) {
			// The above loop doesn't use history duplication so just duplicate once at the end
			copy_type(&m_history_buffer[delay], &m_history_buffer[0], k_simd_32_lanes - 1);
		}
	}

	m_history_index = history_index;

	return false;
}
