#pragma once

#include "common/common.h"

#include "engine/buffer.h"
#include "engine/sample_format.h"

// Mixes N inputs to M outputs
void mix_channel_buffers(
	uint32 frames,
	c_wrapped_array<const c_buffer> input_buffers,
	c_wrapped_array<c_buffer> output_buffers);

void convert_and_deinterleave_from_stream_input_buffer(
	uint32 frames,
	e_sample_format input_format,
	c_wrapped_array<const uint8> stream_input_buffer,
	c_wrapped_array<c_buffer> input_buffers);

void convert_and_interleave_to_stream_output_buffer(
	uint32 frames,
	c_wrapped_array<const c_buffer> output_buffers,
	e_sample_format output_format,
	c_wrapped_array<uint8> stream_output_buffer);

void zero_output_buffers(
	uint32 frames,
	uint32 output_buffer_count,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffers);

