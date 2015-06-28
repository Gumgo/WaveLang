#ifndef WAVELANG_RUNTIME_CONTEXT_H__
#define WAVELANG_RUNTIME_CONTEXT_H__

#include "common/common.h"
#include "driver/driver_interface.h"
#include "engine/task_graph.h"

struct s_runtime_context {
	// Interface for the audio driver
	c_driver_interface driver_interface;

	// We store two task graphs, one for the active audio stream, and one for the loading synth. We load into the
	// non-active one and then swap at a deterministic time.
	c_task_graph task_graphs[2];
	int32 active_task_graph;
};

#endif // WAVELANG_RUNTIME_CONTEXT_H__