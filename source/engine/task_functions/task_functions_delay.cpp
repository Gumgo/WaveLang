#include "common/utility/bit_operations.h"
#include "common/utility/stack_allocator.h"

#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/events/event_interface.h"
#include "engine/task_function_registration.h"

#include <algorithm>
#include <cmath>

template<typename t_value, typename t_buffer_data, typename t_copy_implementation>
class c_delay_buffer {
public:
	c_delay_buffer() = default;

	void initialize(c_wrapped_array<t_buffer_data> buffer, size_t delay_samples) {
		m_buffer = buffer;
		m_delay_samples = delay_samples;
		m_uninitialized_sample_count = delay_samples;
	}

	bool is_valid() const {
		return m_delay_samples > 0;
	}

	void reset(t_value initial_value) {
		m_head_index = 0;
		m_constant_value = initial_value;
		m_constant_sample_count = m_delay_samples;
		m_initial_value = initial_value;
		m_uninitialized_sample_count = m_delay_samples;
	}

	void process(const t_buffer_data *input, t_buffer_data *output, size_t sample_count) {
		wl_assert(m_delay_samples > 0);

		size_t samples_to_process = std::min(m_delay_samples, sample_count);

		// Copy delayed samples to beginning of the output
		pop(output, 0, samples_to_process);

		// Copy input to the history buffer
		push(input, sample_count - samples_to_process, samples_to_process);

		if (m_delay_samples < sample_count) {
			t_copy_implementation::copy_data(output, m_delay_samples, input, 0, sample_count - m_delay_samples);
		}
	}

	// Returns whether the output is constant. If so, only the first element is written to.
	bool process_constant(t_value input, t_buffer_data *output, size_t sample_count) {
		wl_assert(m_delay_samples > 0);

		if (input == m_constant_value && m_constant_sample_count == m_delay_samples) {
			// The entire buffer is constant and the input matches that constant value. We don't need to do anything!
			t_copy_implementation::replicate_constant(output, 0, input, 1);
			return true;
		}

		size_t samples_to_process = std::min(m_delay_samples, sample_count);

		// Copy delayed samples to beginning of the output
		pop(output, 0, samples_to_process);

		// Copy input to the history buffer
		push_constant(input, samples_to_process);

		if (m_delay_samples < sample_count) {
			t_copy_implementation::replicate_constant(output, m_delay_samples, input, sample_count - m_delay_samples);
		}

		return false;
	}

private:
	void push(const t_buffer_data *input, size_t offset, size_t count) {
		wl_assert(count <= m_delay_samples);
		if (m_head_index < count) {
			// Need to do 2 copies
			size_t prev_head_index = (m_head_index + m_delay_samples) - count;
			size_t back_size = m_delay_samples - prev_head_index;
			size_t front_size = count - back_size;

			write_to_buffer(prev_head_index, input, offset, back_size);
			write_to_buffer(0, input, offset + back_size, front_size);
		} else {
			size_t prev_head_index = m_head_index - count;
			write_to_buffer(prev_head_index, input, offset, count);
		}
	}

	void push_constant(t_value value, size_t count) {
		wl_assert(count <= m_delay_samples);
		if (m_head_index < count) {
			// Need to do 2 copies
			size_t prev_head_index = (m_head_index + m_delay_samples) - count;
			size_t back_size = m_delay_samples - prev_head_index;
			size_t front_size = count - back_size;

			write_constant_to_buffer(prev_head_index, back_size, value);
			write_constant_to_buffer(0, front_size, value);
		} else {
			size_t prev_head_index = m_head_index - count;
			write_constant_to_buffer(prev_head_index, count, value);
		}
	}

	void pop(t_buffer_data *output, size_t offset, size_t count) {
		wl_assert(count <= m_delay_samples);
		if (m_head_index + count > m_delay_samples) {
			// Need to do 2 copies
			size_t back_size = m_delay_samples - m_head_index;
			size_t front_size = count - back_size;

			read_from_buffer(output, offset, back_size);
			wl_assert(m_head_index == m_delay_samples);
			m_head_index = 0;
			read_from_buffer(output, offset + back_size, front_size);
		} else {
			read_from_buffer(output, offset, count);
		}
	}

	void write_to_buffer(size_t buffer_index, const t_buffer_data *input, size_t offset, size_t count) {
		wl_assert(buffer_index + count <= m_delay_samples);
		t_copy_implementation::copy_data(m_buffer.get_pointer(), buffer_index, input, offset, count);

		// We're not pushing constant data, so clear the constant state
		m_constant_sample_count = 0;
	}

	void write_constant_to_buffer(size_t buffer_index, size_t count, t_value value) {
		if (value != m_constant_value) {
			m_constant_value = value;
			m_constant_sample_count = 0;
		}

		if (m_constant_sample_count == m_delay_samples) {
			// The buffer is already full with this value, no need to copy over it
			return;
		}

		t_copy_implementation::replicate_constant(m_buffer.get_pointer(), buffer_index, value, count);
		m_constant_sample_count = std::min(m_constant_sample_count + count, m_delay_samples);
	}

	void read_from_buffer(t_buffer_data *output, size_t offset, size_t count) {
		wl_assert(m_head_index + count <= m_delay_samples);
		size_t uninitialized_samples_to_read = std::min(m_uninitialized_sample_count, count);
		t_copy_implementation::replicate_constant(output, offset, m_initial_value, uninitialized_samples_to_read);

		if (m_uninitialized_sample_count < count) {
			t_copy_implementation::copy_data(
				output,
				offset + uninitialized_samples_to_read,
				m_buffer.get_pointer(),
				m_head_index + uninitialized_samples_to_read,
				count - uninitialized_samples_to_read);
		}

		m_uninitialized_sample_count -= uninitialized_samples_to_read;
		m_head_index += count;
	}

	// It is assumed that pop() will always be followed by push() using the same size. Therefore, we only track one head
	// pointer, and assume all pushes will exactly replace popped data.
	c_wrapped_array<t_buffer_data> m_buffer;	// The circular buffer
	size_t m_delay_samples = 0;					// The number of delay samples
	size_t m_head_index = 0;					// Index we write to
	t_value m_constant_value = 0.0f;			// The last constant value written to the buffer
	size_t m_constant_sample_count = 0;			// Number of samples with value m_constant_value in the buffer
	t_value m_initial_value = 0.0f;				// Initial value to provide to the output before delay catches up
	size_t m_uninitialized_sample_count = 0;	// Number of samples in the buffer assumed to contain m_initial_value
};

struct s_real32_copy_implementation {
	static void copy_data(
		real32 *destination,
		size_t destination_offset,
		const real32 *source,
		size_t source_offset,
		size_t count) {
		copy_type(destination + destination_offset, source + source_offset, count);
	}

	static void replicate_constant(
		real32 *destination,
		size_t destination_offset,
		real32 value,
		size_t count) {
		destination += destination_offset;
		for (size_t index = 0; index < count; index++) {
			*destination++ = value;
		}
	}
};

struct s_bool_copy_implementation {
	static void copy_data(
		int32 *destination,
		size_t destination_offset,
		const int32 *source,
		size_t source_offset,
		size_t count) {
		copy_bits(destination, destination_offset, source, source_offset, count);
	}

	static void replicate_constant(
		int32 *destination,
		size_t destination_offset,
		bool value,
		size_t count) {
		replicate_bit(destination, destination_offset, value, count);
	}
};

using c_delay_buffer_real = c_delay_buffer<real32, real32, s_real32_copy_implementation>;
using c_delay_buffer_bool = c_delay_buffer<bool, int32, s_bool_copy_implementation>;

struct s_delay_real_context {
	c_stack_allocator::s_context allocator_context;
	c_delay_buffer_real delay_buffer;
};

struct s_delay_bool_context {
	c_stack_allocator::s_context allocator_context;
	c_delay_buffer_bool delay_buffer;
};

struct s_memory_real_context {
	real32 m_value;
};

struct s_memory_bool_context {
	bool m_value;
};

s_task_memory_query_result delay_real_memory_query(const s_task_function_context &context, real32 duration_samples) {
	uint32 delay_samples;
	if (std::isnan(duration_samples) || std::isinf(duration_samples) || duration_samples < 0.0f) {
		delay_samples = 0;
	} else {
		delay_samples = static_cast<uint32>(duration_samples);
	}

	c_stack_allocator::c_memory_calculator calculator;
	calculator.add<s_delay_real_context>();
	calculator.add_array<real32>(delay_samples);

	s_task_memory_query_result result;
	result.voice_size_alignment = calculator.get_size_alignment();
	return result;
}

void delay_real_voice_initializer(const s_task_function_context &context, real32 duration_samples) {
	uint32 delay_samples;
	if (std::isnan(duration_samples) || std::isinf(duration_samples) || duration_samples < 0.0f) {
		context.event_interface->submit(EVENT_WARNING << "Invalid delay duration, defaulting to 0");
		delay_samples = 0;
	} else {
		delay_samples = static_cast<uint32>(duration_samples);
	}

	s_delay_real_context *delay_context;
	c_wrapped_array<real32> buffer;
	c_stack_allocator allocator(context.voice_memory);
	allocator.allocate(delay_context);
	allocator.allocate_array(buffer, delay_samples);

	delay_context->delay_buffer.initialize(buffer, delay_samples);
	delay_context->allocator_context = allocator.release();
}

void delay_real_voice_deinitializer(const s_task_function_context &context) {
	s_delay_real_context *delay_context = reinterpret_cast<s_delay_real_context *>(context.voice_memory.get_pointer());
	c_stack_allocator::free_context(delay_context->allocator_context);
}

void delay_real_voice_activator(const s_task_function_context &context, real32 initial_value) {
	s_delay_real_context *delay_context = reinterpret_cast<s_delay_real_context *>(context.voice_memory.get_pointer());
	delay_context->delay_buffer.reset(initial_value);
}

void delay_real(
	const s_task_function_context &context,
	real32 duration,
	const c_real_buffer *signal,
	c_real_buffer *result) {
	s_delay_real_context *delay_context = reinterpret_cast<s_delay_real_context *>(context.voice_memory.get_pointer());
	if (signal->is_constant()) {
		bool is_output_constant = delay_context->delay_buffer.process_constant(
			signal->get_constant(),
			result->get_data(),
			context.buffer_size);
		if (is_output_constant) {
			result->extend_constant();
		}
	} else {
		delay_context->delay_buffer.process(signal->get_data(), result->get_data(), context.buffer_size);
		result->set_is_constant(false);
	}
}

s_task_memory_query_result delay_bool_memory_query(const s_task_function_context &context, real32 duration_samples) {
	uint32 delay_samples;
	if (std::isnan(duration_samples) || std::isinf(duration_samples) || duration_samples < 0.0f) {
		delay_samples = 0;
	} else {
		delay_samples = static_cast<uint32>(duration_samples);
	}

	c_stack_allocator::c_memory_calculator calculator;
	calculator.add<s_delay_bool_context>();
	calculator.add_array<int32>(bool_buffer_int32_count(delay_samples));

	s_task_memory_query_result result;
	result.voice_size_alignment = calculator.get_size_alignment();
	return result;
}

void delay_bool_voice_initializer(const s_task_function_context &context, real32 duration_samples) {
	uint32 delay_samples;
	if (std::isnan(duration_samples) || std::isinf(duration_samples) || duration_samples < 0.0f) {
		context.event_interface->submit(EVENT_WARNING << "Invalid delay duration, defaulting to 0");
		delay_samples = 0;
	} else {
		delay_samples = static_cast<uint32>(duration_samples);
	}

	s_delay_bool_context *delay_context;
	c_wrapped_array<int32> buffer;
	c_stack_allocator allocator(context.voice_memory);
	allocator.allocate(delay_context);
	allocator.allocate_array(buffer, bool_buffer_int32_count(delay_samples));

	delay_context->delay_buffer.initialize(buffer, delay_samples);
	delay_context->allocator_context = allocator.release();
}

void delay_bool_voice_deinitializer(const s_task_function_context &context) {
	s_delay_bool_context *delay_context = reinterpret_cast<s_delay_bool_context *>(context.voice_memory.get_pointer());
	c_stack_allocator::free_context(delay_context->allocator_context);
}

void delay_bool_voice_activator(const s_task_function_context &context, bool initial_value) {
	s_delay_bool_context *delay_context = reinterpret_cast<s_delay_bool_context *>(context.voice_memory.get_pointer());
	delay_context->delay_buffer.reset(initial_value);
}

void delay_bool(
	const s_task_function_context &context,
	wl_task_argument(real32, duration),
	wl_task_argument(const c_bool_buffer *, signal),
	wl_task_argument(c_bool_buffer *, result)) {
	s_delay_bool_context *delay_context = reinterpret_cast<s_delay_bool_context *>(context.voice_memory.get_pointer());
	if (signal->is_constant()) {
		bool is_output_constant = delay_context->delay_buffer.process_constant(
			signal->get_constant(),
			result->get_data(),
			context.buffer_size);
		if (is_output_constant) {
			result->extend_constant();
		}
	} else {
		delay_context->delay_buffer.process(signal->get_data(), result->get_data(), context.buffer_size);
		result->set_is_constant(false);
	}
}

namespace delay_task_functions {

	// delay_samples$real

	s_task_memory_query_result delay_samples_real_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_real_memory_query(context, duration);
	}

	void delay_samples_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_real_voice_initializer(context, duration);
	}

	void delay_samples_real_voice_deinitializer(
		const s_task_function_context &context) {
		delay_real_voice_deinitializer(context);
	}

	void delay_samples_real_voice_activator(
		const s_task_function_context &context) {
		delay_real_voice_activator(context, 0.0f);
	}

	void delay_samples_real(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument_unshared(c_real_buffer *, result)) {
		delay_real(context, duration, signal, result);
	}

	// delay_samples$bool

	s_task_memory_query_result delay_samples_bool_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_bool_memory_query(context, duration);
	}

	void delay_samples_bool_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_bool_voice_initializer(context, duration);
	}

	void delay_samples_bool_voice_deinitializer(
		const s_task_function_context &context) {
		delay_bool_voice_deinitializer(context);
	}

	void delay_samples_bool_voice_activator(
		const s_task_function_context &context) {
		delay_bool_voice_activator(context, 0.0f);
	}

	void delay_samples_bool(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument(c_bool_buffer *, result)) {
		delay_bool(context, duration, signal, result);
	}

	// delay_samples$initial_value_real

	s_task_memory_query_result delay_samples_initial_value_real_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_real_memory_query(context, duration);
	}

	void delay_samples_initial_value_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_real_voice_initializer(context, duration);
	}

	void delay_samples_initial_value_real_voice_deinitializer(
		const s_task_function_context &context) {
		delay_real_voice_deinitializer(context);
	}

	void delay_samples_initial_value_real_voice_activator(
		const s_task_function_context &context,
		wl_task_argument(real32, initial_value)) {
		delay_real_voice_activator(context, initial_value);
	}

	void delay_samples_initial_value_real(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(real32, initial_value),
		wl_task_argument_unshared(c_real_buffer *, result)) {
		delay_real(context, duration, signal, result);
	}

	// delay_samples$initial_value_bool

	s_task_memory_query_result delay_samples_initial_value_bool_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_bool_memory_query(context, duration);
	}

	void delay_samples_initial_value_bool_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_bool_voice_initializer(context, duration);
	}

	void delay_samples_initial_value_bool_voice_deinitializer(
		const s_task_function_context &context) {
		delay_bool_voice_deinitializer(context);
	}

	void delay_samples_initial_value_bool_voice_activator(
		const s_task_function_context &context,
		wl_task_argument(bool, initial_value)) {
		delay_bool_voice_activator(context, initial_value);
	}

	void delay_samples_initial_value_bool(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument(bool, initial_value),
		wl_task_argument(c_bool_buffer *, result)) {
		delay_bool(context, duration, signal, result);
	}

	// delay_seconds$real

	s_task_memory_query_result delay_seconds_real_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_real_memory_query(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_real_voice_initializer(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_real_voice_deinitializer(
		const s_task_function_context &context) {
		delay_real_voice_deinitializer(context);
	}

	void delay_seconds_real_voice_activator(
		const s_task_function_context &context) {
		delay_real_voice_activator(context, 0.0f);
	}

	void delay_seconds_real(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument_unshared(c_real_buffer *, result)) {
		delay_real(context, duration, signal, result);
	}

	// delay_seconds$bool

	s_task_memory_query_result delay_seconds_bool_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_bool_memory_query(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_bool_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_bool_voice_initializer(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_bool_voice_deinitializer(
		const s_task_function_context &context) {
		delay_bool_voice_deinitializer(context);
	}

	void delay_seconds_bool_voice_activator(
		const s_task_function_context &context) {
		delay_bool_voice_activator(context, false);
	}

	void delay_seconds_bool(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument(c_bool_buffer *, result)) {
		delay_bool(context, duration, signal, result);
	}

	// delay_seconds$initial_value_real

	s_task_memory_query_result delay_seconds_initial_value_real_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_real_memory_query(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_initial_value_real_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_real_voice_initializer(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_initial_value_real_voice_deinitializer(
		const s_task_function_context &context) {
		delay_real_voice_deinitializer(context);
	}

	void delay_seconds_initial_value_real_voice_activator(
		const s_task_function_context &context,
		wl_task_argument(real32, initial_value)) {
		delay_real_voice_activator(context, initial_value);
	}

	void delay_seconds_initial_value_real(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(real32, initial_value),
		wl_task_argument_unshared(c_real_buffer *, result)) {
		delay_real(context, duration, signal, result);
	}

	// delay_seconds$initial_value_bool

	s_task_memory_query_result delay_seconds_initial_value_bool_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		return delay_bool_memory_query(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_initial_value_bool_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		delay_bool_voice_initializer(context, duration * static_cast<real32>(context.sample_rate));
	}

	void delay_seconds_initial_value_bool_voice_deinitializer(
		const s_task_function_context &context) {
		delay_bool_voice_deinitializer(context);
	}

	void delay_seconds_initial_value_bool_voice_activator(
		const s_task_function_context &context,
		wl_task_argument(bool, initial_value)) {
		delay_bool_voice_activator(context, initial_value);
	}

	void delay_seconds_initial_value_bool(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_bool_buffer *, signal),
		wl_task_argument(bool, initial_value),
		wl_task_argument(c_bool_buffer *, result)) {
		delay_bool(context, duration, signal, result);
	}

	s_task_memory_query_result memory_real_memory_query(const s_task_function_context &context) {
		s_task_memory_query_result result;
		result.voice_size_alignment = sizealignof(s_memory_real_context);
		return result;
	}

	void memory_real_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<s_memory_real_context *>(context.voice_memory.get_pointer())->m_value = 0.0f;
	}

	void memory_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, value),
		wl_task_argument(const c_bool_buffer *, write),
		wl_task_argument(c_real_buffer *, result)) {
		s_memory_real_context *memory_context =
			reinterpret_cast<s_memory_real_context *>(context.voice_memory.get_pointer());

		// Put this value on the stack to avoid memory reads each iteration
		real32 memory_value = memory_context->m_value;

		if (value->is_constant()) {
			real32 value_value = value->get_constant();

			if (write->is_constant()) {
				bool write_value = write->get_constant();
				memory_value = write_value ? value_value : memory_value;
				result->assign_constant(memory_value);
			} else {
				for (size_t index = 0; index < context.buffer_size; index++) {
					bool write_value = test_bit(write->get_data()[index / 32], index % 32);
					memory_value = write_value ? value_value : memory_value;
					result->get_data()[index] = memory_value;
				}

				result->set_is_constant(false);
			}
		} else {
			if (write->is_constant()) {
				bool write_value = write->get_constant();

				if (write_value) {
					copy_type(result->get_data(), value->get_data(), context.buffer_size);
					memory_value = value->get_data()[context.buffer_size - 1];
					result->set_is_constant(false);
				} else {
					result->assign_constant(memory_value);
				}
			} else {
				for (size_t index = 0; index < context.buffer_size; index++) {
					real32 value_value = value->get_data()[index];
					bool write_value = test_bit(write->get_data()[index / 32], index % 32);
					memory_value = write_value ? value_value : memory_value;
					result->get_data()[index] = memory_value;
				}

				result->set_is_constant(false);
			}
		}

		memory_context->m_value = memory_value;
	}

	s_task_memory_query_result memory_bool_memory_query(const s_task_function_context &context) {
		s_task_memory_query_result result;
		result.voice_size_alignment = sizealignof(s_memory_bool_context);
		return result;
	}

	void memory_bool_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<s_memory_bool_context *>(context.voice_memory.get_pointer())->m_value = false;
	}

	void memory_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, value),
		wl_task_argument(const c_bool_buffer *, write),
		wl_task_argument(c_bool_buffer *, result)) {
		s_memory_bool_context *memory_context =
			reinterpret_cast<s_memory_bool_context *>(context.voice_memory.get_pointer());

		// Put this value on the stack to avoid memory reads each iteration
		bool memory_value = memory_context->m_value;

		if (value->is_constant()) {
			bool value_value = value->get_constant();

			if (write->is_constant()) {
				bool write_value = write->get_constant();
				memory_value = write_value ? value_value : memory_value;
				result->assign_constant(memory_value);
			} else {
				// $TODO could optimize this (and similar loops) to use uint32 blocks
				for (size_t index = 0; index < context.buffer_size; index++) {
					bool write_value = test_bit(write->get_data()[index / 32], index % 32);
					memory_value = write_value ? value_value : memory_value;
					result->get_data()[index] = memory_value
						? result->get_data()[index] | (1 << index % 32)
						: result->get_data()[index] & ~(1 << index % 32);
				}

				result->set_is_constant(false);
			}
		} else {
			if (write->is_constant()) {
				bool write_value = write->get_constant();

				if (write_value) {
					copy_type(
						result->get_data(),
						value->get_data(),
						bool_buffer_int32_count(context.buffer_size));
					size_t last_index = context.buffer_size - 1;
					memory_value = test_bit(value->get_data()[last_index / 32], last_index % 32);
					result->set_is_constant(false);
				} else {
					result->assign_constant(memory_value);
				}
			} else {
				for (size_t index = 0; index < context.buffer_size; index++) {
					bool value_value = test_bit(value->get_data()[index / 32], index % 32);
					bool write_value = test_bit(write->get_data()[index / 32], index % 32);
					memory_value = write_value ? value_value : memory_value;
					result->get_data()[index] = memory_value
						? result->get_data()[index] | (1 << index % 32)
						: result->get_data()[index] & ~(1 << index % 32);
				}

				result->set_is_constant(false);
			}
		}

		memory_context->m_value = memory_value;
	}

	void scrape_task_functions() {
		static constexpr uint32 k_delay_library_id = 4;
		wl_task_function_library(k_delay_library_id, "delay", 0);

		wl_task_function(0xa32697d3, "delay_samples$real")
			.set_function<delay_samples_real>()
			.set_memory_query<delay_samples_real_memory_query>()
			.set_voice_initializer<delay_samples_real_voice_initializer>()
			.set_voice_deinitializer<delay_samples_real_voice_deinitializer>()
			.set_voice_activator<delay_samples_real_voice_activator>();

		wl_task_function(0x491ec116, "delay_samples$bool")
			.set_function<delay_samples_bool>()
			.set_memory_query<delay_samples_bool_memory_query>()
			.set_voice_initializer<delay_samples_bool_voice_initializer>()
			.set_voice_deinitializer<delay_samples_bool_voice_deinitializer>()
			.set_voice_activator<delay_samples_bool_voice_activator>();

		wl_task_function(0xb9ffacc6, "delay_samples$initial_value_real")
			.set_function<delay_samples_initial_value_real>()
			.set_memory_query<delay_samples_initial_value_real_memory_query>()
			.set_voice_initializer<delay_samples_initial_value_real_voice_initializer>()
			.set_voice_deinitializer<delay_samples_initial_value_real_voice_deinitializer>()
			.set_voice_activator<delay_samples_initial_value_real_voice_activator>();

		wl_task_function(0x271916ea, "delay_samples$initial_value_bool")
			.set_function<delay_samples_initial_value_bool>()
			.set_memory_query<delay_samples_initial_value_bool_memory_query>()
			.set_voice_initializer<delay_samples_initial_value_bool_voice_initializer>()
			.set_voice_deinitializer<delay_samples_initial_value_bool_voice_deinitializer>()
			.set_voice_activator<delay_samples_initial_value_bool_voice_activator>();

		wl_task_function(0xf37bcf39, "delay_seconds$real")
			.set_function<delay_seconds_real>()
			.set_memory_query<delay_seconds_real_memory_query>()
			.set_voice_initializer<delay_seconds_real_voice_initializer>()
			.set_voice_deinitializer<delay_seconds_real_voice_deinitializer>()
			.set_voice_activator<delay_seconds_real_voice_activator>();

		wl_task_function(0xd078cc64, "delay_seconds$bool")
			.set_function<delay_seconds_bool>()
			.set_memory_query<delay_seconds_bool_memory_query>()
			.set_voice_initializer<delay_seconds_bool_voice_initializer>()
			.set_voice_deinitializer<delay_seconds_bool_voice_deinitializer>()
			.set_voice_activator<delay_seconds_bool_voice_activator>();

		wl_task_function(0xeaf97cff, "delay_seconds$initial_value_real")
			.set_function<delay_seconds_initial_value_real>()
			.set_memory_query<delay_seconds_initial_value_real_memory_query>()
			.set_voice_initializer<delay_seconds_initial_value_real_voice_initializer>()
			.set_voice_deinitializer<delay_seconds_initial_value_real_voice_deinitializer>()
			.set_voice_activator<delay_seconds_initial_value_real_voice_activator>();

		wl_task_function(0x9799e01d, "delay_seconds$initial_value_bool")
			.set_function<delay_seconds_initial_value_bool>()
			.set_memory_query<delay_seconds_initial_value_bool_memory_query>()
			.set_voice_initializer<delay_seconds_initial_value_bool_voice_initializer>()
			.set_voice_deinitializer<delay_seconds_initial_value_bool_voice_deinitializer>()
			.set_voice_activator<delay_seconds_initial_value_bool_voice_activator>();

		wl_task_function(0xbe5eb8bd, "memory$real")
			.set_function<memory_real>()
			.set_memory_query<memory_real_memory_query>()
			.set_voice_activator<memory_real_voice_activator>();

		wl_task_function(0x1fbebc67, "memory$bool")
			.set_function<memory_bool>()
			.set_memory_query<memory_bool_memory_query>()
			.set_voice_activator<memory_bool_voice_activator>();

		wl_end_active_library_task_function_registration();
	}

}
