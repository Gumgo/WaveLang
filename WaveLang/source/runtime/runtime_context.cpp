#include "runtime/runtime_context.h"
#include "engine/events/event_interface.h"

void s_runtime_context::stream_callback(const s_driver_stream_callback_context &context) {
	// Pipe into the executor
	s_runtime_context *this_ptr = static_cast<s_runtime_context *>(context.user_data);

	s_executor_chunk_context chunk_context;
	chunk_context.sample_rate = static_cast<uint32>(context.driver_settings->sample_rate);
	chunk_context.output_channels = context.driver_settings->output_channels;
	chunk_context.sample_format = context.driver_settings->sample_format;
	chunk_context.frames = context.driver_settings->frames_per_buffer;
	chunk_context.output_buffers = c_wrapped_array<uint8>(
		static_cast<uint8 *>(context.output_buffers),
		chunk_context.output_channels * chunk_context.frames * get_sample_format_size(chunk_context.sample_format));
	// $TODO add more

	this_ptr->executor.execute(chunk_context);
}