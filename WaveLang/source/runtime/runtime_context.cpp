#include "runtime_context.h"

void s_runtime_context::stream_callback(const s_driver_stream_callback_context &context) {
	// Pipe into the executor
	s_runtime_context *this_ptr = static_cast<s_runtime_context *>(context.user_data);

	s_executor_chunk_context chunk_context;
	chunk_context.sample_rate = context.driver_settings->sample_rate;
	chunk_context.output_channels = context.driver_settings->output_channels;
	chunk_context.sample_format = context.driver_settings->sample_format;
	chunk_context.frames = context.driver_settings->frames_per_buffer;
	chunk_context.output_buffers = context.output_buffers;
	// $TODO add more

	this_ptr->executor.execute(chunk_context);
}