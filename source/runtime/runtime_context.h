#ifndef WAVELANG_RUNTIME_CONTEXT_H__
#define WAVELANG_RUNTIME_CONTEXT_H__

#include "common/common.h"
#include "driver/audio_driver_interface.h"
#include "driver/controller_driver_interface.h"
#include "engine/task_graph.h"
#include "engine/executor.h"

struct s_runtime_context {
	static void stream_callback(const s_audio_driver_stream_callback_context &context);

	// Interface for the audio driver
	c_audio_driver_interface audio_driver_interface;

	// Interface for controller driver
	c_controller_driver_interface controller_driver_interface;

	// Executes the synth logic
	c_executor executor;

	// We store two task graphs, one for the active audio stream, and one for the loading synth. We load into the
	// non-active one and then swap at a deterministic time.
	c_task_graph task_graphs[2];
	int32 active_task_graph;
};

#endif // WAVELANG_RUNTIME_CONTEXT_H__