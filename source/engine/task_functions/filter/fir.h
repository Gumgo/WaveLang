#pragma once

#include "common/common.h"
#include "common/utility/stack_allocator.h"

class c_fir_coefficients {
public:
	c_fir_coefficients() = default;

	static void calculate_memory(size_t coefficient_count, c_stack_allocator::c_memory_calculator &memory_calculator);
	void initialize(c_wrapped_array<const real32> coefficients, c_stack_allocator &allocator);

private:
	friend class c_fir;

	// Aligned and padded coefficients
	c_wrapped_array<real32> m_coefficients;
};

class c_fir {
public:
	c_fir() = default;

	static void calculate_memory(size_t coefficient_count, c_stack_allocator::c_memory_calculator &memory_calculator);
	void initialize(size_t coefficient_count, c_stack_allocator &allocator);

	void reset();

	void process(const c_fir_coefficients &fir_coefficients, const real32 *input, real32 *output, size_t sample_count);

	// Returns whether the output is constant. If so, only the first element is written to.
	bool process_constant(
		const c_fir_coefficients &fir_coefficients,
		real32 input,
		real32 *output,
		size_t sample_count);

private:
	// Padded history buffer
	c_wrapped_array<real32> m_history_buffer;

	// Current history index
	size_t m_history_index = 0;

	// Number of zeros in the history buffer
	size_t m_zero_count = 0;
};
