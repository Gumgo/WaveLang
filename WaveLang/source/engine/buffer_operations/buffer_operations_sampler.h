#ifndef WAVELANG_BUFFER_OPERATIONS_SAMPLER_H__
#define WAVELANG_BUFFER_OPERATIONS_SAMPLER_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/sample/sample.h"

class c_sample_library_accessor;
class c_sample_library_requester;

struct s_buffer_operation_sampler {
	static size_t query_memory();
	static void initialize(c_sample_library_requester *sample_requester,
		s_buffer_operation_sampler *context, const char *sample, e_sample_loop_mode loop_mode, real32 channel);

	static void buffer(
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_buffer_out out, c_buffer_in speed);
	static void bufferio(
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_buffer_inout speed_out);
	static void constant(
		c_sample_library_accessor *sample_accessor, s_buffer_operation_sampler *context,
		size_t buffer_size, uint32 sample_rate, c_buffer_out out, real32 speed);

	uint32 sample_handle;
	uint32 channel;
	real32 time;
	bool reached_end;
};

#endif // WAVELANG_BUFFER_OPERATIONS_SAMPLER_H__