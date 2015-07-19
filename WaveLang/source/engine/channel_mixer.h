#ifndef WAVELANG_CHANNEL_MIXER_H__
#define WAVELANG_CHANNEL_MIXER_H__

#include "common/common.h"
#include "driver/sample_format.h"
#include "engine/buffer_allocator.h"

void mix_output_buffers_to_channel_buffers(
	uint32 frames,
	c_buffer_allocator &buffer_allocator,
	c_wrapped_array_const<uint32> output_buffers,
	c_wrapped_array_const<uint32> channel_buffers);

void convert_and_interleave_to_output_buffer(
	uint32 frames,
	c_buffer_allocator &buffer_allocator,
	c_wrapped_array_const<uint32> output_buffers,
	e_sample_format output_format,
	void *output_buffer);

#endif // WAVELANG_CHANNEL_MIXER_H__