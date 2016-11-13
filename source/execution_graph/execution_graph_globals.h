#ifndef WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_GLOBALS_H__
#define WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_GLOBALS_H__

#include "common/common.h"

// Also called synth globals
struct s_execution_graph_globals {
	// The maximum number of voice instances that can be playing at once
	uint32 max_voices;

	// The stream sample rate. If 0, this graph is compatible with any stream. Note that while some modules require the
	// sample rate to be pre-specified (such as get_sample_rate()), most are sample rate agnostic.
	uint32 sample_rate;

	// The size of each processing chunk. If 0, this graph is compatible with any processing chunk size. Certain modules
	// may require a minimum chunk size, though most are chunk size agnostic.
	uint32 chunk_size;
};

#endif // WAVELANG_EXECUTION_GRAPH_EXECUTION_GRAPH_GLOBALS_H__
