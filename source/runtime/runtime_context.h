#pragma once

#include "common/common.h"

#include "engine/executor/executor.h"
#include "engine/runtime_instrument.h"

#include "runtime/driver/audio_driver_interface.h"
#include "runtime/driver/controller_driver_interface.h"

struct s_runtime_context {
	static void stream_callback(const s_audio_driver_stream_callback_context &context);
	static size_t process_controller_events_callback(
		void *context,
		c_wrapped_array<s_timestamped_controller_event> controller_events,
		real64 buffer_time_sec,
		real64 buffer_duration_sec);

	// Interface for the audio driver
	c_audio_driver_interface audio_driver_interface;

	// Interface for controller driver
	c_controller_driver_interface controller_driver_interface;

	// Executes the synth logic
	c_executor executor;

	// We store two runtime instruments, one for the active audio stream, and one for the loading synth. We load into
	// the non-active one and then swap at a deterministic time.
	s_static_array<c_runtime_instrument, 2> runtime_instruments;
	int32 active_instrument;
};
