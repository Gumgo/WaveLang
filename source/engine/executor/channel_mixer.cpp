#include "engine/buffer_operations/buffer_operations_internal.h"
#include "engine/executor/channel_mixer.h"

void mix_output_buffers_to_channel_buffers(
	uint32 frames,
	c_buffer_allocator &buffer_allocator,
	c_wrapped_array_const<uint32> output_buffers,
	c_wrapped_array_const<uint32> channel_buffers) {
	size_t output_buffer_count = output_buffers.get_count();
	size_t channel_buffer_count = channel_buffers.get_count();
	wl_assert(output_buffer_count > 0);
	wl_assert(channel_buffer_count > 0);
	wl_vassert(output_buffer_count != channel_buffer_count, "Mixing is unnecessary");

	// Support specific cases:
	if (output_buffer_count == 1) {
		// Mix mono to N channels
		c_buffer *output_buffer = buffer_allocator.get_buffer(output_buffers[0]);
		for (size_t index = 0; index < channel_buffer_count; index++) {
			s_buffer_operation_assignment::in_out(
				frames,
				output_buffer,
				buffer_allocator.get_buffer(channel_buffers[index]));
		}
	} else if (channel_buffer_count == 1) {
		// Mix N channels to mono
		// Add the first 2 directly
		s_buffer_operation_addition::in_in_out(
			frames,
			buffer_allocator.get_buffer(output_buffers[0]),
			buffer_allocator.get_buffer(output_buffers[1]),
			buffer_allocator.get_buffer(channel_buffers[0]));

		// Accumulate the remaining buffers
		for (size_t index = 2; index < channel_buffer_count; index++) {
			s_buffer_operation_addition::inout_in(
				frames,
				buffer_allocator.get_buffer(channel_buffers[0]),
				buffer_allocator.get_buffer(output_buffers[index]));
		}

		// Scale the result
		s_buffer_operation_multiplication::inout_in(
			frames,
			buffer_allocator.get_buffer(channel_buffers[0]),
			1.0f / static_cast<real32>(output_buffers.get_count()));
	} else {
		// $TODO unsupported, for now zero the channel buffers
		for (size_t index = 0; index < channel_buffer_count; index++) {
			s_buffer_operation_assignment::in_out(
				frames,
				c_real_buffer_or_constant_in(0.0f),
				buffer_allocator.get_buffer(channel_buffers[index]));
		}
	}
}

void convert_and_interleave_to_output_buffer(
	uint32 frames,
	c_buffer_allocator &buffer_allocator,
	c_wrapped_array_const<uint32> output_buffers,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffer) {
	size_t channel_count = output_buffers.get_count();
	switch (output_format) {
	case k_sample_format_float32:
	{
		real32 *typed_output_buffer = reinterpret_cast<real32 *>(output_buffer.get_pointer());
		for (size_t channel = 0; channel < channel_count; channel++) {
			c_buffer *buffer = buffer_allocator.get_buffer(output_buffers[channel]);
			const real32 *channel_buffer = buffer->get_data<real32>();

			if (buffer->is_constant()) {
				real32 value = *channel_buffer;
				for (uint32 frame = 0; frame < frames; frame++) {
					typed_output_buffer[channel + frame * channel_count] = value;
				}
			} else {
				for (uint32 frame = 0; frame < frames; frame++) {
					typed_output_buffer[channel + frame * channel_count] = channel_buffer[frame];
				}
			}
		}
		break;
	}

	default:
		wl_vhalt("Unsupported format");
	}
}

void zero_output_buffers(
	uint32 frames,
	uint32 output_buffer_count,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffers) {
	switch (output_format) {
	case k_sample_format_float32:
	{
		wl_assert(output_buffers.get_count() == (frames * output_buffer_count * sizeof(real32)));
		memset(output_buffers.get_pointer(), 0, output_buffers.get_count());
		break;
	}

	default:
		wl_vhalt("Unsupported format");
	}
}