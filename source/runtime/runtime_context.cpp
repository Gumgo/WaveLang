#include "engine/events/event_interface.h"

#include "runtime/runtime_context.h"

#include <iostream>

void s_runtime_context::stream_callback(const s_audio_driver_stream_callback_context &context) {
	// Pipe into the executor
	s_runtime_context *this_ptr = static_cast<s_runtime_context *>(context.user_data);

	s_executor_chunk_context chunk_context;
	chunk_context.sample_rate = static_cast<uint32>(context.driver_settings->sample_rate);
	chunk_context.output_channels = context.driver_settings->output_channels;
	chunk_context.sample_format = context.driver_settings->sample_format;
	chunk_context.frames = context.driver_settings->frames_per_buffer;
	chunk_context.buffer_time_sec = context.buffer_time_sec;
	chunk_context.output_buffer = context.output_buffer;
	// $TODO add more

	this_ptr->executor.execute(chunk_context);
}

size_t s_runtime_context::process_controller_events_callback(
	void *context, c_wrapped_array<s_timestamped_controller_event> controller_events,
	real64 buffer_time_sec, real64 buffer_duration_sec) {
	s_runtime_context *this_ptr = static_cast<s_runtime_context *>(context);
	if (this_ptr->controller_driver_interface.is_stream_running()) {
		return this_ptr->controller_driver_interface.process_controller_events(
			controller_events, buffer_time_sec, buffer_duration_sec);
	} else {
		return 0;
	}
}