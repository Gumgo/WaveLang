#pragma once

#include "common/common.h"

// Settings which apply throughout an entire instrument
struct s_instrument_globals {
	// The maximum number of voice instances that can be playing at once
	uint32 max_voices = 0;

	// The stream sample rate. If 0, this graph is compatible with any stream. Note that while some modules require the
	// sample rate to be pre-specified (such as get_sample_rate()), most are sample rate agnostic.
	uint32 sample_rate = 0;

	// The size of each processing chunk. If 0, this graph is compatible with any processing chunk size. Certain modules
	// may require a minimum chunk size, though most are chunk size agnostic.
	uint32 chunk_size = 0;

	// Whether to start FX processing immediately. If false, FX processing starts when any voice is activated.
	bool activate_fx_immediately = false;
};

