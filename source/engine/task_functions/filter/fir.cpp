#include "common/math/math.h"

#include "engine/task_functions/filter/fir.h"

// In this FIR implementation, we pad the coefficients with zeros to be a multiple of the SIMD lane count. We store the
// coefficients in reverse and double the length of the history buffer.

void c_fir_coefficients::calculate_memory(
	size_t coefficient_count,
	c_stack_allocator::c_memory_calculator &memory_calculator) {
	wl_assert(coefficient_count > 0);

	size_t padded_coefficient_count = align_size(coefficient_count, k_simd_32_lanes);
	memory_calculator.add_array<real32>(padded_coefficient_count, k_simd_alignment);
}

void c_fir_coefficients::initialize(c_wrapped_array<const real32> coefficients, c_stack_allocator &allocator) {
	wl_assert(coefficients.get_count() > 0);

	size_t padded_coefficient_count = align_size(coefficients.get_count(), k_simd_32_lanes);
	allocator.allocate_array(m_coefficients, padded_coefficient_count, k_simd_alignment);

	// Pad the end of the coefficient array with zeros and store the coefficients in reverse
	zero_type(m_coefficients.get_pointer(), m_coefficients.get_count() - coefficients.get_count());
	for (size_t index = 0; index < coefficients.get_count(); index++) {
		m_coefficients[m_coefficients.get_count() - index - 1] = coefficients[index];
	}
}

void c_fir::calculate_memory(size_t coefficient_count, c_stack_allocator::c_memory_calculator &memory_calculator) {
	wl_assert(coefficient_count > 0);

	// No need for alignment on the history buffer
	size_t padded_coefficient_count = align_size(coefficient_count, k_simd_32_lanes);
	memory_calculator.add_array<real32>(padded_coefficient_count * 2);
}

void c_fir::initialize(size_t coefficient_count, c_stack_allocator &allocator) {
	wl_assert(coefficient_count > 0);

	size_t padded_coefficient_count = align_size(coefficient_count, k_simd_32_lanes);
	allocator.allocate_array(m_history_buffer, padded_coefficient_count * 2);
	zero_type(m_history_buffer.get_pointer(), m_history_buffer.get_count());

	m_history_index = 0;
	m_zero_count = padded_coefficient_count;
}

void c_fir::reset() {
	zero_type(m_history_buffer.get_pointer(), m_history_buffer.get_count());
	m_history_index = 0;
	m_zero_count = m_history_buffer.get_count() / 2;
}

void c_fir::process(
	const c_fir_coefficients &fir_coefficients,
	const real32 *input,
	real32 *output,
	size_t sample_count) {
	size_t coefficient_count = fir_coefficients.m_coefficients.get_count();
	wl_assert(coefficient_count * 2 == m_history_buffer.get_count());

	m_zero_count = 0;

	for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
		// Duplicate the sample in the history buffer
		real32 input_value = input[sample_index];
		m_history_buffer[m_history_index] = input_value;
		m_history_buffer[m_history_index + coefficient_count] = input_value;

		real32xN output_value(0.0f);

		// The history buffer is repeated twice and each repetition is of length coefficient_count. We want our last
		// sampled history value to be the one we just wrote which is at index m_history_index + coefficient_count.
		// Therefore, we start sampling at index (m_history_index + coefficient_count) - (coefficient_count - 1) =
		// m_history_count + 1. Note that the coefficients are stored in reverse so we don't have to reverse the history
		// buffer here.
		size_t history_index = m_history_index + 1;
		for (size_t coefficient_index = 0;
			coefficient_index < coefficient_count;
			coefficient_index += k_simd_32_lanes) {
			real32xN coefficients(&fir_coefficients.m_coefficients[coefficient_index]);
			real32xN history;
			history.load_unaligned(&m_history_buffer[history_index]);
			output_value += coefficients * history;

			history_index += k_simd_32_lanes;
		}

		output[sample_index] = output_value.sum_elements().first_element();

		m_history_index++;
		m_history_index = (m_history_index == coefficient_count) ? 0 : m_history_index;
	}
}

bool c_fir::process_constant(
	const c_fir_coefficients &fir_coefficients,
	real32 input,
	real32 *output,
	size_t sample_count) {
	size_t coefficient_count = fir_coefficients.m_coefficients.get_count();
	wl_assert(coefficient_count * 2 == m_history_buffer.get_count());

	if (input == 0.0f) {
		if (m_zero_count >= coefficient_count) {
			// All coefficients are being multiplied by 0
			*output = 0.0f;
			return true;
		} else {
			m_zero_count += sample_count;
		}
	} else {
		m_zero_count = 0;
	}

	for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
		// Duplicate the sample in the history buffer
		m_history_buffer[m_history_index] = input;
		m_history_buffer[m_history_index + coefficient_count] = input;

		real32xN output_value(0.0f);

		// The history buffer is repeated twice and each repetition is of length coefficient_count. We want our last
		// sampled history value to be the one we just wrote which is at index m_history_index + coefficient_count.
		// Therefore, we start sampling at index (m_history_index + coefficient_count) - (coefficient_count - 1) =
		// m_history_count + 1. Note that the coefficients are stored in reverse so we don't have to reverse the history
		// buffer here.
		size_t history_index = m_history_index + 1;
		for (size_t coefficient_index = 0;
			coefficient_index < coefficient_count;
			coefficient_index += k_simd_32_lanes) {
			real32xN coefficients(&fir_coefficients.m_coefficients[coefficient_index]);
			real32xN history;
			history.load_unaligned(&m_history_buffer[history_index]);
			output_value += coefficients * history;

			history_index += k_simd_32_lanes;
		}

		output[sample_index] = output_value.sum_elements().first_element();

		m_history_index++;
		m_history_index = (m_history_index == coefficient_count) ? 0 : m_history_index;
	}

	return false;
}
