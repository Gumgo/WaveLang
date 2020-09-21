#pragma once

#include "common/common.h"

#include <fstream>
#include <vector>

struct s_loaded_sample {
	uint32 sample_rate;				// Sample rate in samples per second
	uint32 frame_count;				// Number of frames (samples per channel)
	uint32 channel_count;			// Number of channels
	uint32 loop_start_sample_index;	// Index of the first sample in the loop region
	uint32 loop_end_sample_index;	// Index of the first sample after the loop region

	// Channels are not interleaved; sample data takes the form [s00 s01 ... s0n; s10 s11 ... s1n; ...] where sxy is
	// the yth sample from channel x.
	std::vector<real32> samples;
};

bool load_sample(const char *filename, s_loaded_sample &out_loaded_sample);
