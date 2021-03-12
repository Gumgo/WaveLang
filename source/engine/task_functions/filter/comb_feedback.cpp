#include "common/math/math.h"

#include "engine/resampler/resampler.h"
#include "engine/task_functions/filter/comb_feedback.h"

#include "instrument/resampler/resampler.h"

#include <cmath>

static constexpr e_resampler_filter k_resampler_filter = e_resampler_filter::k_upsample_low_quality;

void c_comb_feedback::calculate_memory(uint32 delay, c_stack_allocator::c_memory_calculator &memory_calculator) {
	wl_assert(delay > 0);

	uint32 history_length = delay;
	memory_calculator.add_array<real32>(history_length);
}

void c_comb_feedback::calculate_memory_variable(
	real32 max_delay,
	c_stack_allocator::c_memory_calculator &memory_calculator) {
	wl_assert(max_delay > 0.0f);
	uint32 max_delay_integer = static_cast<uint32>(std::ceil(max_delay));

	const s_resampler_parameters &resampler_parameters = get_resampler_parameters(k_resampler_filter);

	// To sample at index i, the resampler needs (taps_per_phase - latency) preceding samples
	uint32 history_length = max_delay_integer + (resampler_parameters.taps_per_phase - resampler_parameters.latency);

	// Duplicate samples at the end of the buffer for the resampler
	memory_calculator.add_array<real32>(history_length + resampler_parameters.taps_per_phase - 1);
}

void c_comb_feedback::initialize(uint32 delay, c_stack_allocator &allocator) {
	wl_assert(delay > 0);

	m_history_length = delay;
	m_min_delay = 0.0f;
	m_max_delay = 0.0f;
	allocator.allocate_array(m_history_buffer, m_history_length);

	m_history_index = 0;
}

void c_comb_feedback::initialize_variable(real32 min_delay, real32 max_delay, c_stack_allocator &allocator) {
	wl_assert(max_delay > 0.0f);
	uint32 max_delay_integer = static_cast<uint32>(std::ceil(max_delay));

	const s_resampler_parameters &resampler_parameters = get_resampler_parameters(k_resampler_filter);

#if IS_TRUE(ASSERTS_ENABLED)
	uint32 min_allowed_delay = resampler_parameters.taps_per_phase - resampler_parameters.latency;
	uint32 min_delay_integer = static_cast<uint32>(min_delay);
	wl_assert(min_delay_integer >= min_allowed_delay);
#endif // IS_TRUE(ASSERTS_ENABLED)

	// To sample at index i, the resampler needs (taps_per_phase - latency) preceding samples
	m_history_length = max_delay_integer + (resampler_parameters.taps_per_phase - resampler_parameters.latency);
	m_min_delay = min_delay;
	m_max_delay = max_delay;

	// Duplicate samples at the end of the buffer for the resampler
	allocator.allocate_array(m_history_buffer, m_history_length + resampler_parameters.taps_per_phase - 1);

	m_history_index = 0;
}

void c_comb_feedback::reset() {
	zero_type(m_history_buffer.get_pointer(), m_history_buffer.get_count());
	m_history_index = 0;
}

void c_comb_feedback::read_history(uint32 delay, real32 *output, size_t sample_count) {
	wl_assert(delay > 0);
	wl_assert(delay == m_history_length);
	wl_assert(sample_count <= delay);

	size_t start_history_index = (delay <= m_history_index)
		? m_history_index - delay
		: m_history_index + m_history_length - delay;
	if (start_history_index + sample_count > m_history_length) {
		// We need to divide our read up into two ranges
		size_t pre_wrap_sample_count = m_history_length - start_history_index;
		read_contiguous_history(start_history_index, output, pre_wrap_sample_count);
		read_contiguous_history(0, output + pre_wrap_sample_count, sample_count - pre_wrap_sample_count);
	} else {
		read_contiguous_history(start_history_index, output, sample_count);
	}
}

void c_comb_feedback::read_history_variable(const real32 *delay, real32 *output, size_t sample_count) {
	wl_assert(sample_count <= static_cast<uint32>(m_min_delay));

	const s_resampler_parameters &resampler_parameters = get_resampler_parameters(k_resampler_filter);
	c_resampler resampler(resampler_parameters, get_resampler_phases(k_resampler_filter));

	// Cache some values on the stack
	real32 min_delay = m_min_delay;
	real32 max_delay = m_max_delay;
	uint32 latency = resampler_parameters.latency;
	uint32 history_length = m_history_length;
	size_t history_index = m_history_index;

	for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
		real32 sanitized_delay = std::clamp(sanitize_inf_nan(delay[sample_index]), min_delay, max_delay);
		uint32 delay_integer = static_cast<uint32>(sanitized_delay);
		real32 delay_fraction = sanitized_delay - static_cast<real32>(delay_integer);

		// Account for resampling latency
		wl_assert(delay_integer >= latency);
		delay_integer -= latency;

		// If the resampler would require sampling before index 0, sample from the duplicated end of the history buffer
		size_t history_sample_index = (history_index < delay_integer + (resampler_parameters.taps_per_phase - 1))
			? history_index + history_length - delay_integer
			: history_index - delay_integer;

		output[sample_index] = resampler.resample(m_history_buffer, history_sample_index, delay_fraction);

		history_index++;
		history_index = (history_index == history_length) ? 0 : history_index;
	}
}

void c_comb_feedback::read_history_variable_constant(real32 delay, real32 *output, size_t sample_count) {
	wl_assert(sample_count <= static_cast<uint32>(m_min_delay));

	static constexpr e_resampler_filter k_resampler_filter = e_resampler_filter::k_upsample_low_quality;
	const s_resampler_parameters &resampler_parameters = get_resampler_parameters(k_resampler_filter);
	c_resampler resampler(resampler_parameters, get_resampler_phases(k_resampler_filter));

	// Cache some values on the stack
	real32 sanitized_delay = std::clamp(sanitize_inf_nan(delay), m_min_delay, m_max_delay);
	uint32 delay_integer = static_cast<uint32>(sanitized_delay);
	real32 delay_fraction = sanitized_delay - static_cast<real32>(delay_integer);

	// Account for resampling latency
	wl_assert(delay_integer >= resampler_parameters.latency);
	delay_integer -= resampler_parameters.latency;

	uint32 history_length = m_history_length;
	size_t history_index = m_history_index;

	for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
		// If the resampler would require sampling before index 0, sample from the duplicated end of the history buffer
		size_t history_sample_index = (history_index < delay_integer + (resampler_parameters.taps_per_phase - 1))
			? history_index + history_length - delay_integer
			: history_index - delay_integer;

		output[sample_index] = resampler.resample(m_history_buffer, history_sample_index, delay_fraction);

		history_index++;
		history_index = (history_index == history_length) ? 0 : history_index;
	}
}

void c_comb_feedback::write_history(
	const real32 *input,
	const real32 *filtered_history,
	real32 *output,
	size_t sample_count) {
	if (m_min_delay == 0.0f) {
		wl_assert(sample_count <= m_history_length);
	} else {
		wl_assert(sample_count <= static_cast<uint32>(m_min_delay));
	}

	if (m_history_index + sample_count > m_history_length) {
		// We need to divide our write up into two ranges
		size_t pre_wrap_sample_count = m_history_length - m_history_index;
		write_contiguous_history(input, filtered_history, pre_wrap_sample_count);
		write_contiguous_history(
			input + pre_wrap_sample_count,
			filtered_history + pre_wrap_sample_count,
			sample_count - pre_wrap_sample_count);
	} else {
		write_contiguous_history(input, filtered_history, sample_count);
	}
}

void c_comb_feedback::write_history_constant(
	real32 input,
	const real32 *filtered_history,
	real32 *output,
	size_t sample_count) {
	if (m_min_delay == 0.0f) {
		wl_assert(sample_count <= m_history_length);
	} else {
		wl_assert(sample_count <= static_cast<uint32>(m_min_delay));
	}

	if (m_history_index + sample_count > m_history_length) {
		// We need to divide our write up into two ranges
		size_t pre_wrap_sample_count = m_history_length - m_history_index;
		write_contiguous_history(input, filtered_history, pre_wrap_sample_count);
		write_contiguous_history(input, filtered_history + pre_wrap_sample_count, sample_count - pre_wrap_sample_count);
	} else {
		write_contiguous_history(input, filtered_history, sample_count);
	}
}


void c_comb_feedback::read_contiguous_history(size_t history_index, real32 *output, size_t sample_count) const {
	wl_assert(history_index + sample_count <= m_history_length);
	copy_type(output, &m_history_buffer[history_index], sample_count);
}

void c_comb_feedback::write_contiguous_history(
	const real32 *input,
	const real32 *filtered_history,
	size_t sample_count) {
	wl_assert(m_history_index + sample_count <= m_history_length);

	size_t initial_history_index = m_history_index;
	size_t sample_index = 0;
	size_t simd_sample_count = align_size_down(sample_count, k_simd_32_lanes);

	while (sample_index < simd_sample_count) {
		real32xN input_value;
		input_value.load_unaligned(&input[sample_index]);
		real32xN filtered_history_value;
		filtered_history_value.load_unaligned(&filtered_history[sample_index]);
		real32xN result = input_value + filtered_history_value;
		result.store_unaligned(&m_history_buffer[m_history_index]);

		sample_index += k_simd_32_lanes;
		m_history_index += k_simd_32_lanes;
	}

	while (sample_index < sample_count) {
		m_history_buffer[m_history_index] = input[sample_index] + filtered_history[sample_index];
		sample_index++;
		m_history_index++;
	}

	m_history_index = (m_history_index == m_history_length) ? 0 : m_history_index;

	// Copy to the end of the buffer if necessary
	size_t padding_sample_count = m_history_buffer.get_count() - m_history_length;
	if (initial_history_index < padding_sample_count) {
		size_t copy_sample_count = padding_sample_count - initial_history_index;
		copy_type(
			&m_history_buffer[m_history_length + initial_history_index],
			&m_history_buffer[initial_history_index],
			copy_sample_count);
	}
}

void c_comb_feedback::write_contiguous_history(real32 input, const real32 *filtered_history, size_t sample_count) {
	wl_assert(m_history_index + sample_count <= m_history_length);

	size_t initial_history_index = m_history_index;
	size_t sample_index = 0;
	size_t simd_sample_count = align_size_down(sample_count, k_simd_32_lanes);

	real32xN input_value(input);
	while (sample_index < simd_sample_count) {
		real32xN filtered_history_value;
		filtered_history_value.load_unaligned(&filtered_history[sample_index]);
		real32xN result = input_value + filtered_history_value;
		result.store_unaligned(&m_history_buffer[m_history_index]);

		sample_index += k_simd_32_lanes;
		m_history_index += k_simd_32_lanes;
	}

	while (sample_index < sample_count) {
		m_history_buffer[m_history_index] = input + filtered_history[sample_index];
		sample_index++;
		m_history_index++;
	}

	m_history_index = (m_history_index == m_history_length) ? 0 : m_history_index;

	// Copy to the end of the buffer if necessary
	size_t padding_sample_count = m_history_buffer.get_count() - m_history_length;
	if (initial_history_index < padding_sample_count) {
		size_t copy_sample_count = padding_sample_count - initial_history_index;
		copy_type(
			&m_history_buffer[m_history_length + initial_history_index],
			&m_history_buffer[initial_history_index],
			copy_sample_count);
	}
}
