#pragma once

#include "common/common.h"
#include "common/utility/stack_allocator.h"

class c_allpass {
public:
	c_allpass() = default;

	static void calculate_memory(uint32 delay, c_stack_allocator::c_memory_calculator &memory_calculator);
	void initialize(uint32 delay, real32 gain, c_stack_allocator &allocator);

	void reset();

	void process(const real32 *input, real32 *output, size_t sample_count);

	// Returns whether the output is constant. If so, only the first element is written to.
	bool process_constant(real32 input, real32 *output, size_t sample_count);

private:
	real32 m_gain = 0.0f;
	uint32 m_delay = 0;

	// Padded history buffer with the beginning duplicated to the end
	c_wrapped_array<real32> m_history_buffer;

	// Current read/write index in the history buffer
	size_t m_history_index = 0;

	// Number of zeros in the history buffer
	size_t m_zero_count = 0;

};
