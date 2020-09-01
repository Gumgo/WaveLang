#pragma once

#include "common/common.h"

#include "driver/sample_format.h"

#include "engine/buffer.h"

void mix_output_buffers_to_channel_buffers(
	uint32 frames,
	c_wrapped_array<c_buffer> output_buffers,
	c_wrapped_array<c_buffer> channel_buffers);

void convert_and_interleave_to_output_buffer(
	uint32 frames,
	c_wrapped_array<c_buffer> output_buffers,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffer);

void zero_output_buffers(
	uint32 frames,
	uint32 output_buffer_count,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffers);

