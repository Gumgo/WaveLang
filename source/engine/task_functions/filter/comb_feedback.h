#pragma once

#include "common/common.h"
#include "common/utility/stack_allocator.h"

class c_comb_feedback {
public:
	c_comb_feedback() = default;

	static void calculate_memory(uint32 delay, c_stack_allocator::c_memory_calculator &memory_calculator);
	static void calculate_memory_variable(real32 max_delay, c_stack_allocator::c_memory_calculator &memory_calculator);

	void initialize(uint32 delay, c_stack_allocator &allocator);
	void initialize_variable(real32 min_delay, real32 max_delay, c_stack_allocator &allocator);

	void reset();

	// Call this first to sample from the history buffer, then apply any additional filtering manually
	void read_history(uint32 delay, real32 *output, size_t sample_count);
	void read_history_variable(const real32 *delay, real32 *output, size_t sample_count);
	void read_history_variable_constant(real32 delay, real32 *output, size_t sample_count);

	// Call this second to write the summed input and history back to both the history buffer and the output buffer
	void write_history(const real32 *input, const real32 *filtered_history, real32 *output, size_t sample_count);
	void write_history_constant(real32 input, const real32 *filtered_history, real32 *output, size_t sample_count);

private:
	void read_contiguous_history(size_t history_index, real32 *output, size_t sample_count) const;
	void write_contiguous_history(const real32 *input, const real32 *filtered_history, size_t sample_count);
	void write_contiguous_history(real32 input, const real32 *filtered_history, size_t sample_count);

	uint32 m_history_length = 0;
	real32 m_min_delay = 0.0f;
	real32 m_max_delay = 0.0f;

	c_wrapped_array<real32> m_history_buffer;

	// Current write index in the history buffer
	size_t m_history_index = 0;
};
