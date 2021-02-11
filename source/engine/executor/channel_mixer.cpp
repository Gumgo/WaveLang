#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/executor/channel_mixer.h"

void mix_channel_buffers(
	uint32 frames,
	c_wrapped_array<const c_buffer> input_buffers,
	c_wrapped_array<c_buffer> output_buffers) {
	size_t input_buffer_count = input_buffers.get_count();
	size_t output_buffer_count = output_buffers.get_count();
	wl_assert(input_buffer_count > 0);
	wl_assert(output_buffer_count > 0);

	// Support specific cases:
	if (input_buffer_count == output_buffer_count) {
		for (size_t index = 0; index < output_buffer_count; index++) {
			const c_real_buffer &input_buffer = input_buffers[index].get_as<c_real_buffer>();
			c_real_buffer &output_buffer = output_buffers[index].get_as<c_real_buffer>();
			if (input_buffer.is_constant()) {
				output_buffer.assign_constant(input_buffer.get_constant());
			} else {
				output_buffer.set_is_constant(false);
				copy_type(output_buffer.get_data(), input_buffer.get_data(), frames);
			}
		}
	} else if (input_buffer_count == 1) {
		// Mix mono to N channels
		const c_real_buffer &input_buffer = input_buffers[0].get_as<c_real_buffer>();
		for (size_t index = 0; index < output_buffer_count; index++) {
			c_real_buffer &output_buffer = output_buffers[index].get_as<c_real_buffer>();
			if (input_buffer.is_constant()) {
				output_buffer.assign_constant(input_buffer.get_constant());
			} else {
				output_buffer.set_is_constant(false);
				copy_type(output_buffer.get_data(), input_buffer.get_data(), frames);
			}
		}
	} else if (output_buffer_count == 1) {
		// Mix N channels to mono
		c_real_buffer &output_buffer = output_buffers[0].get_as<c_real_buffer>();
		output_buffer.assign_constant(0.0f);

		// Accumulate and scale by 1/N (we could do 1/sqrt(N) but there's no guarantee the channels add incoherently)
		real32xN scale(1.0f / static_cast<real32>(output_buffer_count));
		for (const c_buffer &input_buffer : input_buffers) {
			const c_real_buffer &real_output_buffer = input_buffer.get_as<c_real_buffer>();
			const c_real_buffer &channel_buffer_const = output_buffer;

			iterate_buffers<k_simd_32_lanes, true>(frames, &real_output_buffer, &channel_buffer_const, &output_buffer,
				[&scale](size_t i, const real32xN &output, const real32xN &channel_in, real32xN &channel_out) {
					channel_out = channel_in + output * scale;
				});
		}
	} else {
		// $TODO unsupported, for now zero the output buffers
		for (c_buffer &buffer : output_buffers) {
			buffer.get_as<c_real_buffer>().assign_constant(0.0f);
		}
	}
}

void convert_and_deinterleave_from_stream_input_buffer(
	uint32 frames,
	e_sample_format input_format,
	c_wrapped_array<const uint8> stream_input_buffer,
	c_wrapped_array<c_buffer> input_buffers) {
	size_t channel_count = input_buffers.get_count();
	switch (input_format) {
	case e_sample_format::k_float32:
	{
		const real32 *typed_input_buffer = reinterpret_cast<const real32 *>(stream_input_buffer.get_pointer());
		for (size_t channel = 0; channel < channel_count; channel++) {
			c_real_buffer &channel_buffer = input_buffers[channel].get_as<c_real_buffer>();
			real32 first_value = frames == 0 ? 0.0f : typed_input_buffer[channel];
			bool constant = true;

			real32 *channel_data = channel_buffer.get_data();
			for (uint32 frame = 0; frame < frames; frame++) {
				real32 value = typed_input_buffer[channel + frame * channel_count];
				constant &= (value == first_value);
				channel_data[frame] = value;
			}

			if (constant) {
				channel_buffer.assign_constant(first_value);
			}
		}
		break;
	}

	default:
		wl_haltf("Unsupported format");
	}
}

void convert_and_interleave_to_stream_output_buffer(
	uint32 frames,
	c_wrapped_array<const c_buffer> output_buffers,
	e_sample_format output_format,
	c_wrapped_array<uint8> stream_output_buffer) {
	size_t channel_count = output_buffers.get_count();
	switch (output_format) {
	case e_sample_format::k_float32:
	{
		real32 *typed_output_buffer = reinterpret_cast<real32 *>(stream_output_buffer.get_pointer());
		for (size_t channel = 0; channel < channel_count; channel++) {
			const c_real_buffer &channel_buffer = output_buffers[channel].get_as<c_real_buffer>();

			if (channel_buffer.is_constant()) {
				real32 value = channel_buffer.get_constant();
				for (uint32 frame = 0; frame < frames; frame++) {
					typed_output_buffer[channel + frame * channel_count] = value;
				}
			} else {
				const real32 *channel_data = channel_buffer.get_data();
				for (uint32 frame = 0; frame < frames; frame++) {
					typed_output_buffer[channel + frame * channel_count] = channel_data[frame];
				}
			}
		}
		break;
	}

	// $TODO other formats should dither

	default:
		wl_haltf("Unsupported format");
	}
}

void zero_output_buffers(
	uint32 frames,
	uint32 output_buffer_count,
	e_sample_format output_format,
	c_wrapped_array<uint8> output_buffers) {
	switch (output_format) {
	case e_sample_format::k_float32:
	{
		wl_assert(output_buffers.get_count() == (frames * output_buffer_count * sizeof(real32)));
		zero_type(output_buffers.get_pointer(), output_buffers.get_count());
		break;
	}

	default:
		wl_haltf("Unsupported format");
	}
}