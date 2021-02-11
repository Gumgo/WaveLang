#include "common/utility/stack_allocator.h"

#include "engine/buffer.h"
#include "engine/resampler/resampler.h"
#include "engine/task_function_registration.h"

#include "instrument/native_modules/resampler/resampler.h"

static constexpr real32 k_quality_low = 0.0f;
static constexpr real32 k_quality_high = 1.0f;

struct s_resampler_library_context {
	// Bool upsampling simply duplicates each bit N times. To do this efficiently, we build lookup tables which map
	// input bytes to sets of output bytes with duplicated bits, e.g. for 2x upsampling, 00110101 -> 0000111100110011
	uint32 upsample2x_bool_mapping[256];
	uint32 upsample3x_bool_mapping[256];
	uint32 upsample4x_bool_mapping[256];

	// Bool downsampling is simply decimation - we only look at every N bits. Like for upsampling, we build lookup
	// tables to map upsampled inputs to decimated outputs, e.g. for 2x downsampling, 0x0x1x1x0x1x0x1x -> 00110101,
	// where 'x' bits don't matter and can be anything (there are duplicated entries for these). These tables consist of
	// 256 entries for each upsampled input byte in a set of N bytes which get ORed together to produce a single
	// downsampled output.
	uint8 downsample2x_bool_mapping[256 * 2];
	uint8 downsample3x_bool_mapping[256 * 3];
	uint8 downsample4x_bool_mapping[256 * 4];
};

class c_resample_real {
public:
	c_resample_real() = default;

	static void calculate_memory(
		bool is_upsampling,
		uint32 resample_factor,
		e_resampler_filter resampler_filter,
		c_stack_allocator::c_memory_calculator &memory_calculator) {
		const s_resampler_parameters &resampler_parameters = get_resampler_parameters(resampler_filter);
		size_t delay = calculate_delay(is_upsampling, resample_factor, resampler_parameters);

		size_t history_extension = c_resampler::get_required_history_samples(resampler_parameters);
		size_t history_length = delay + resampler_parameters.taps_per_phase;
		size_t extended_history_buffer_size = history_extension + history_length;
		memory_calculator.add_array<real32>(extended_history_buffer_size);
	}

	void initialize(
		bool is_upsampling,
		uint32 resample_factor,
		e_resampler_filter resampler_filter,
		c_stack_allocator &allocator) {
		m_is_upsampling = is_upsampling;
		m_resample_factor = resample_factor;
		m_resampler_filter = resampler_filter;

		const s_resampler_parameters &resampler_parameters = get_resampler_parameters(resampler_filter);
		m_delay = calculate_delay(is_upsampling, resample_factor, resampler_parameters);

		m_history_extension = c_resampler::get_required_history_samples(resampler_parameters);
		m_history_length = m_delay + resampler_parameters.taps_per_phase;
		size_t extended_history_buffer_size = m_history_extension + m_history_length;
		allocator.allocate_array(m_history_buffer, extended_history_buffer_size);

		if (is_upsampling) {
			wl_assert(resample_factor <= k_max_upsample_factor);
			for (uint32 i = 0; i < resample_factor; i++) {
				m_upsample_fractions[i] = static_cast<real32>(i) / static_cast<real32>(resample_factor);
			}
		}
	}

	void reset() {
		m_write_index = m_delay;
		m_read_index = 0;
		m_constant_value = 0.0f;
		m_constant_count = m_history_buffer.get_count();

		// $PERF unfortunately I can't think of a way to amortize this cost, which we must pay on every voice activation
		zero_type(m_history_buffer.get_pointer(), m_history_buffer.get_count());
	}

	void process_upsample(const c_real_buffer *input, c_real_buffer *output, size_t input_sample_count) {
		wl_assert(m_is_upsampling);

		if (input->is_constant()) {
			real32 input_value = input->get_constant();
			if (input_value == m_constant_value && m_constant_count >= m_history_length) {
				output->assign_constant(input_value);
				return;
			}

			c_resampler resampler(
				get_resampler_parameters(m_resampler_filter),
				get_resampler_phases(m_resampler_filter));
			real32 *output_data = output->get_data();
			size_t output_sample_index = 0;
			for (size_t sample_index = 0; sample_index < input_sample_count; sample_index++) {
				size_t offset_write_index = m_history_extension + m_write_index;
				m_history_buffer[offset_write_index] = input_value;

				// Duplicate the sample if necessary
				if (m_write_index + m_history_extension >= m_history_length) {
					m_history_buffer[offset_write_index - m_history_length] = input_value;
				}

				for (uint32 i = 0; i < m_resample_factor; i++) {
					output_data[output_sample_index++] = resampler.resample(
						m_history_buffer,
						m_read_index + m_history_extension,
						m_upsample_fractions[i]);
				}

				m_write_index++;
				if (m_write_index == m_history_length) {
					m_write_index = 0;
				}

				m_read_index++;
				if (m_read_index == m_history_length) {
					m_read_index = 0;
				}
			}

			output->set_is_constant(false);

			if (input_value != m_constant_value) {
				m_constant_value = input_value;
				m_constant_count = 0;
			}

			m_constant_count += input_sample_count;
		} else {
			c_resampler resampler(
				get_resampler_parameters(m_resampler_filter),
				get_resampler_phases(m_resampler_filter));
			const real32 *input_data = input->get_data();
			real32 *output_data = output->get_data();
			size_t output_sample_index = 0;
			for (size_t sample_index = 0; sample_index < input_sample_count; sample_index++) {
				real32 input_sample = input_data[sample_index];

				size_t offset_write_index = m_history_extension + m_write_index;
				m_history_buffer[offset_write_index] = input_sample;

				// Duplicate the sample if necessary
				if (m_write_index + m_history_extension >= m_history_length) {
					m_history_buffer[offset_write_index - m_history_length] = input_sample;
				}

				for (uint32 i = 0; i < m_resample_factor; i++) {
					output_data[output_sample_index++] = resampler.resample(
						m_history_buffer,
						m_read_index + m_history_extension,
						m_upsample_fractions[i]);
				}

				m_write_index++;
				if (m_write_index == m_history_length) {
					m_write_index = 0;
				}

				m_read_index++;
				if (m_read_index == m_history_length) {
					m_read_index = 0;
				}
			}

			output->set_is_constant(false);
			m_constant_count = 0;
		}
	}

	void process_downsample(const c_real_buffer *input, c_real_buffer *output, size_t output_sample_count) {
		wl_assert(!m_is_upsampling);

		if (input->is_constant()) {
			real32 input_value = input->get_constant();
			if (input_value == m_constant_value && m_constant_count >= m_history_length) {
				output->assign_constant(input_value);
				return;
			}

			c_resampler resampler(
				get_resampler_parameters(m_resampler_filter),
				get_resampler_phases(m_resampler_filter));
			real32 *output_data = output->get_data();
			for (size_t sample_index = 0; sample_index < output_sample_count; sample_index++) {
				for (size_t i = 0; i < m_resample_factor; i++) {
					size_t offset_write_index = m_history_extension + m_write_index;
					m_history_buffer[offset_write_index] = input_value;

					// Duplicate the sample if necessary
					if (m_write_index + m_history_extension >= m_history_length) {
						m_history_buffer[offset_write_index - m_history_length] = input_value;
					}

					if (i == 0) {
						output_data[sample_index] = resampler.resample(
							m_history_buffer,
							m_read_index + m_history_extension,
							0.0f);
					}

					m_write_index++;
					if (m_write_index == m_history_length) {
						m_write_index = 0;
					}

					m_read_index++;
					if (m_read_index == m_history_length) {
						m_read_index = 0;
					}
				}
			}

			output->set_is_constant(false);

			if (input_value != m_constant_value) {
				m_constant_value = input_value;
				m_constant_count = 0;
			}

			m_constant_count += output_sample_count * m_resample_factor;
		} else {
			c_resampler resampler(
				get_resampler_parameters(m_resampler_filter),
				get_resampler_phases(m_resampler_filter));
			const real32 *input_data = input->get_data();
			real32 *output_data = output->get_data();
			for (size_t sample_index = 0; sample_index < output_sample_count; sample_index++) {
				for (size_t i = 0; i < m_resample_factor; i++) {
					real32 input_sample = input_data[sample_index * m_resample_factor + i];

					size_t offset_write_index = m_history_extension + m_write_index;
					m_history_buffer[offset_write_index] = input_sample;

					// Duplicate the sample if necessary
					if (m_write_index + m_history_extension >= m_history_length) {
						m_history_buffer[offset_write_index - m_history_length] = input_sample;
					}

					if (i == 0) {
						output_data[sample_index] = resampler.resample(
							m_history_buffer,
							m_read_index + m_history_extension,
							0.0f);
					}

					m_write_index++;
					if (m_write_index == m_history_length) {
						m_write_index = 0;
					}

					m_read_index++;
					if (m_read_index == m_history_length) {
						m_read_index = 0;
					}
				}
			}

			output->set_is_constant(false);
			m_constant_count = 0;
		}
	}

private:
	static size_t calculate_delay(
		bool is_upsampling,
		uint32 resample_factor,
		const s_resampler_parameters &resampler_parameters) {
		if (is_upsampling) {
			// The latency is in terms of the downsampled sample rate so we don't need to add any additional delay
			// to keep the downsampled and upsampled signals in sync
			return 0;
		} else {
			// The latency is in terms of the downsampled sample rate. Since we're sampling from the upsampled signal,
			// the reported resampler latency is in terms of the upsampled sample rate. To convert, we divide (rounding
			// up) by the downsample factor. By rounding up, we may add up to downsample_factor - 1 additional samples
			// of delay to the upsampled signal.
			uint32 latency_rounded_up =
				((resampler_parameters.latency + resample_factor - 1) / resample_factor) * resample_factor;
			return latency_rounded_up - resampler_parameters.latency;
		}
	}

	static size_t calculate_extended_history_buffer_size(
		size_t delay,
		const s_resampler_parameters &resampler_parameters) {
		size_t required_history_samples = c_resampler::get_required_history_samples(resampler_parameters);

		// We're using a circular buffer and duplicating samples so that the history buffer can read contiguous blocks.
		// In order for this to work, we need to extend the buffer by required_history_samples - 1.
		return delay + required_history_samples * 2 - 1;
	}

	bool m_is_upsampling = false;				// True if we're upsampling, false if we're downsampling
	uint32 m_resample_factor = 0;				// Factor to resample by
	e_resampler_filter m_resampler_filter;		// The resampler filter being used
	size_t m_delay = 0;							// Additional delay to add so that latency is stays aligned

	static constexpr uint32 k_max_upsample_factor = 4;
	real32 m_upsample_fractions[k_max_upsample_factor];

	c_wrapped_array<real32> m_history_buffer;	// History buffer
	size_t m_history_length = 0;				// "Logical" length of the history buffer (without duplication)
	size_t m_history_extension = 0;				// Number of (duplicated) extended samples at the beginning
	size_t m_write_index = 0;					// Index we're writing to (before offsetting by extension)
	size_t m_read_index = 0;					// Index we're reading to (before offsetting by extension)

	real32 m_constant_value = 0.0f;				// The last constant pushed
	size_t m_constant_count = 0;				// The number of sequential constant values in the buffer
};

static s_task_memory_query_result resample_real_memory_query(
	bool is_upsampling,
	uint32 resample_factor,
	real32 quality) {
	s_task_memory_query_result result;

	c_stack_allocator::c_memory_calculator memory_calculator;
	memory_calculator.add<c_resample_real>();
	c_resample_real::calculate_memory(
		is_upsampling,
		resample_factor,
		get_resampler_filter_from_quality(is_upsampling, resample_factor, quality),
		memory_calculator);

	wl_assert(memory_calculator.get_destructor_count() == 0);
	result.voice_size_alignment = memory_calculator.get_size_alignment();

	return result;
}

static void resample_real_voice_initializer(
	const s_task_function_context &context,
	bool is_upsampling,
	uint32 resample_factor,
	real32 quality) {
	c_stack_allocator allocator(context.voice_memory);

	c_resample_real *resample_real;
	allocator.allocate(resample_real);
	resample_real->initialize(
		is_upsampling,
		resample_factor,
		get_resampler_filter_from_quality(is_upsampling, resample_factor, quality),
		allocator);

	allocator.release_no_destructors();
}

static void compute_upsample_bool_mapping(uint32 upsample_factor, c_wrapped_array<uint32> mapping) {
	wl_assert(upsample_factor >= 2 && upsample_factor <= 4);
	wl_assert(mapping.get_count() == 256);

	uint32 expanded_bits = ~(0xffffffffu << upsample_factor);
	for (uint32 input = 0; input < 256; input++) {
		uint32 output = 0;

		// For each set bit in the input, duplicate it N times in the output
		for (uint32 bit = 0; bit < 8; bit++) {
			if (input & (1 << bit)) {
				output |= (expanded_bits << (bit * upsample_factor));
			}
		}

		mapping[input] = output;
	}
}

static void compute_downsample_bool_mapping(uint32 downsample_factor, c_wrapped_array<uint8> mapping) {
	wl_assert(downsample_factor >= 2 && downsample_factor <= 4);
	wl_assert(mapping.get_count() == 256 * downsample_factor);

	uint32 bit_offset = 0;
	for (uint32 input_byte_index = 0; input_byte_index < downsample_factor; input_byte_index++) {
		// Determine the number of bits in the output byte this input byte covers
		uint32 end_bit_offset = ((input_byte_index + 1) * 8 + (downsample_factor - 1)) / downsample_factor;
		uint32 bit_count = end_bit_offset - bit_offset;

		for (uint32 input = 0; input < 256; input++) {
			uint32 output = 0;

			for (uint32 bit = 0; bit < bit_count; bit++) {
				uint32 input_bit_index = ((bit_offset + bit) * downsample_factor) % 8;
				if (input & (1 << input_bit_index)) {
					uint32 output_bit_index = bit_offset + bit;
					output |= (1 << output_bit_index);
				}
			}

			mapping[input_byte_index * 256 + input] = output;
		}

		bit_offset = end_bit_offset;
	}

	// N input bytes should have covered all bits in the output byte
	wl_assert(bit_offset == 8);
}

static size_t get_bool_buffer_byte_index(size_t byte_index) {
	// Grab the correct byte - bool buffers are native-endian int32 arrays
#if IS_TRUE(ENDIANNESS_LITTLE)
	return byte_index;
#else // IS_TRUE(ENDIANNESS_LITTLE)
	return (byte_index & ~3u) + (3u - (byte_index & 3u));
#endif // IS_TRUE(ENDIANNESS_LITTLE)
}

template<uint32 k_upsample_factor>
void upsample_bool(
	const c_bool_buffer *signal,
	c_bool_buffer *upsampled_signal,
	uint32 buffer_size,
	c_wrapped_array<const uint32> mapping) {
	size_t byte_count = bool_buffer_int32_count(buffer_size) * sizeof(int32);
	const uint8 *input = static_cast<const uint8 *>(signal->get_data_untyped());
	uint8 *output = static_cast<uint8 *>(upsampled_signal->get_data_untyped());

	for (size_t byte_index = 0; byte_index < byte_count; byte_index++) {
		size_t input_byte_index = get_bool_buffer_byte_index(byte_index);
		uint8 input_byte = input[input_byte_index];
		uint32 output_bytes = mapping[input_byte];

		for (size_t i = 0; i < k_upsample_factor; i++) {
			size_t output_byte_index = get_bool_buffer_byte_index(byte_index * k_upsample_factor + i);
			output[output_byte_index] = static_cast<uint8>(output_bytes >> (i * 8));
		}
	}
}

template<uint32 k_downsample_factor>
void downsample_bool(
	const c_bool_buffer *signal,
	c_bool_buffer *downsampled_signal,
	uint32 buffer_size,
	c_wrapped_array<const uint8> mapping) {
	size_t byte_count = bool_buffer_int32_count(buffer_size) * sizeof(int32);
	const uint8 *input = static_cast<const uint8 *>(signal->get_data_untyped());
	uint8 *output = static_cast<uint8 *>(downsampled_signal->get_data_untyped());

	for (size_t byte_index = 0; byte_index < byte_count; byte_index++) {
		uint8 output_byte = 0;
		for (size_t i = 0; i < k_downsample_factor; i++) {
			size_t input_byte_index = get_bool_buffer_byte_index(byte_index * k_downsample_factor + i);
			uint8 input_byte = input[input_byte_index];
			output_byte |= mapping[i * 256 + input_byte];
		}

		// Make sure to grab the correct byte - bool buffers are native-endian int32 arrays
		size_t output_byte_index = get_bool_buffer_byte_index(byte_index);
		output[output_byte_index] = output_byte;
	}
}

namespace resampler_task_functions {

	void *engine_initializer() {
		s_resampler_library_context *context = new s_resampler_library_context();

		// Pre-compute all the bool mapping tables
		compute_upsample_bool_mapping(2, c_wrapped_array<uint32>::construct(context->upsample2x_bool_mapping));
		compute_upsample_bool_mapping(3, c_wrapped_array<uint32>::construct(context->upsample3x_bool_mapping));
		compute_upsample_bool_mapping(4, c_wrapped_array<uint32>::construct(context->upsample4x_bool_mapping));
		compute_downsample_bool_mapping(2, c_wrapped_array<uint8>::construct(context->downsample2x_bool_mapping));
		compute_downsample_bool_mapping(3, c_wrapped_array<uint8>::construct(context->downsample3x_bool_mapping));
		compute_downsample_bool_mapping(4, c_wrapped_array<uint8>::construct(context->downsample4x_bool_mapping));

		return context;
	}

	void engine_deinitializer(void *context) {
		delete static_cast<s_resampler_library_context *>(context);
	}

	void upsample2x_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(real32, quality),
		wl_task_argument_upsampled(c_real_buffer *, 2, upsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_upsample(signal, upsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result upsample2x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(true, 2, *quality);
	}

	void upsample2x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, true, 2, *quality);
	}

	void upsample2x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void upsample3x_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(real32, quality),
		wl_task_argument_upsampled(c_real_buffer *, 3, upsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_upsample(signal, upsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result upsample3x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(true, 3, *quality);
	}

	void upsample3x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, true, 3, *quality);
	}

	void upsample3x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void upsample4x_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(real32, quality),
		wl_task_argument_upsampled(c_real_buffer *, 4, upsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_upsample(signal, upsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result upsample4x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(true, 4, *quality);
	}

	void upsample4x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, true, 4, *quality);
	}

	void upsample4x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void downsample2x_real(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_real_buffer *, 2, signal),
		wl_task_argument(real32, quality),
		wl_task_argument(c_real_buffer *, downsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_downsample(signal, downsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result downsample2x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(false, 2, *quality);
	}

	void downsample2x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, false, 2, *quality);
	}

	void downsample2x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void downsample3x_real(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_real_buffer *, 3, signal),
		wl_task_argument(real32, quality),
		wl_task_argument(c_real_buffer *, downsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_downsample(signal, downsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result downsample3x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(false, 3, *quality);
	}

	void downsample3x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, false, 3, *quality);
	}

	void downsample3x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void downsample4x_real(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_real_buffer *, 4, signal),
		wl_task_argument(real32, quality),
		wl_task_argument(c_real_buffer *, downsampled_signal)) {
		c_resample_real *resample_real = reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer());
		resample_real->process_downsample(signal, downsampled_signal, context.buffer_size);
	}

	s_task_memory_query_result downsample4x_real_memory_query(
		wl_task_argument(real32, quality)) {
		return resample_real_memory_query(false, 4, *quality);
	}

	void downsample4x_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, quality)) {
		resample_real_voice_initializer(context, false, 4, *quality);
	}

	void downsample4x_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<c_resample_real *>(context.voice_memory.get_pointer())->reset();
	}

	void upsample2x_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument_upsampled(c_bool_buffer *, 2, upsampled_signal)) {
		if (signal->is_constant()) {
			upsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			upsample_bool<2>(
				signal,
				upsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint32>::construct(library_context->upsample2x_bool_mapping));
		}
	}

	void upsample3x_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument_upsampled(c_bool_buffer *, 3, upsampled_signal)) {
		if (signal->is_constant()) {
			upsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			upsample_bool<3>(
				signal,
				upsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint32>::construct(library_context->upsample3x_bool_mapping));
		}
	}

	void upsample4x_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument_upsampled(c_bool_buffer *, 4, upsampled_signal)) {
		if (signal->is_constant()) {
			upsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			upsample_bool<4>(
				signal,
				upsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint32>::construct(library_context->upsample4x_bool_mapping));
		}
	}

	void downsample2x_bool(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_bool_buffer *, 2, signal),
		wl_task_argument(c_bool_buffer *, downsampled_signal)) {
		if (signal->is_constant()) {
			downsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			downsample_bool<2>(
				signal,
				downsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint8>::construct(library_context->downsample2x_bool_mapping));
		}
	}

	void downsample3x_bool(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_bool_buffer *, 3, signal),
		wl_task_argument(c_bool_buffer *, downsampled_signal)) {
		if (signal->is_constant()) {
			downsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			downsample_bool<3>(
				signal,
				downsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint8>::construct(library_context->downsample3x_bool_mapping));
		}
	}

	void downsample4x_bool(
		const s_task_function_context &context,
		wl_task_argument_upsampled(const c_bool_buffer *, 4, signal),
		wl_task_argument(c_bool_buffer *, downsampled_signal)) {
		if (signal->is_constant()) {
			downsampled_signal->assign_constant(signal->get_constant());
		} else {
			const s_resampler_library_context *library_context =
				static_cast<const s_resampler_library_context *>(context.library_context);
			downsample_bool<4>(
				signal,
				downsampled_signal,
				context.buffer_size,
				c_wrapped_array<const uint8>::construct(library_context->downsample4x_bool_mapping));
		}
	}

	void scrape_task_functions() {
		static constexpr uint32 k_resampler_library_id = 10;
		wl_task_function_library(k_resampler_library_id, "resampler", 0)
			.set_engine_initializer(engine_initializer)
			.set_engine_deinitializer(engine_deinitializer);

		wl_task_function(0x5c4019d5, "upsample2x$real")
			.set_function<upsample2x_real>()
			.set_memory_query<upsample2x_real_memory_query>()
			.set_voice_initializer<upsample2x_real_voice_initializer>()
			.set_voice_activator<upsample2x_real_voice_activator>();

		wl_task_function(0xec547ec6, "upsample3x$real")
			.set_function<upsample3x_real>()
			.set_memory_query<upsample3x_real_memory_query>()
			.set_voice_initializer<upsample3x_real_voice_initializer>()
			.set_voice_activator<upsample3x_real_voice_activator>();

		wl_task_function(0x9958dab3, "upsample4x$real")
			.set_function<upsample4x_real>()
			.set_memory_query<upsample4x_real_memory_query>()
			.set_voice_initializer<upsample4x_real_voice_initializer>()
			.set_voice_activator<upsample4x_real_voice_activator>();

		wl_task_function(0x1cc763e8, "downsample2x$real")
			.set_function<downsample2x_real>()
			.set_memory_query<downsample2x_real_memory_query>()
			.set_voice_initializer<downsample2x_real_voice_initializer>()
			.set_voice_activator<downsample2x_real_voice_activator>();

		wl_task_function(0x92181696, "downsample3x$real")
			.set_function<downsample3x_real>()
			.set_memory_query<downsample3x_real_memory_query>()
			.set_voice_initializer<downsample3x_real_voice_initializer>()
			.set_voice_activator<downsample3x_real_voice_activator>();

		wl_task_function(0xef2246e5, "downsample4x$real")
			.set_function<downsample4x_real>()
			.set_memory_query<downsample4x_real_memory_query>()
			.set_voice_initializer<downsample4x_real_voice_initializer>()
			.set_voice_activator<downsample4x_real_voice_activator>();

		wl_task_function(0x6751d67f, "upsample2x$bool")
			.set_function<upsample2x_bool>();

		wl_task_function(0xd2f546ee, "upsample3x$bool")
			.set_function<upsample3x_bool>();

		wl_task_function(0x3a944cfc, "upsample4x$bool")
			.set_function<upsample4x_bool>();

		wl_task_function(0x177041b1, "downsample2x$bool")
			.set_function<downsample2x_bool>();

		wl_task_function(0x3dbc7043, "downsample3x$bool")
			.set_function<downsample3x_bool>();

		wl_task_function(0x003c0079, "downsample4x$bool")
			.set_function<downsample4x_bool>();

		wl_end_active_library_task_function_registration();
	}

}
