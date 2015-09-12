#ifndef WAVELANG_BUFFER_OPERATIONS_DELAY_H__
#define WAVELANG_BUFFER_OPERATIONS_DELAY_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

struct s_buffer_operation_delay {
	static size_t query_memory(uint32 sample_rate, real32 delay);
	static void initialize(s_buffer_operation_delay *context, uint32 sample_rate, real32 delay);

	static void buffer(
		s_buffer_operation_delay *context, size_t buffer_size, c_buffer_out out, c_buffer_in in);

	// The inout buffer version would require an extra intermediate buffer and more copies, so it's not really worth
	// implementing.

	uint32 delay_samples;
	size_t delay_buffer_head_index;
	bool is_constant;
};

#endif // WAVELANG_BUFFER_OPERATIONS_DELAY_H__