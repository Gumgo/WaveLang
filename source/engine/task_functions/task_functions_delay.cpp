#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/events/event_interface.h"
#include "engine/task_function_registration.h"

#include <algorithm>
#include <cmath>

struct s_delay_context {
	uint32 delay_samples;
	size_t delay_buffer_head_index;
	bool is_constant;
};

struct s_memory_real_context {
	real32 m_value;
};

struct s_memory_bool_context {
	bool m_value;
};

static real32 *get_delay_buffer(s_delay_context *context) {
	// Delay buffer is allocated directly after the context
	return reinterpret_cast<real32 *>(context + 1);
}

// Circular buffer functions: it is assumed that pop() will always be followed by push() using the same size. Therefore,
// we only track one head pointer, and assume all pushes will exactly replace popped data.

static void circular_buffer_pop(
	const void *circular_buffer,
	size_t circular_buffer_size,
	size_t &head_index,
	void *destination,
	size_t size) {
	wl_assert(size <= circular_buffer_size);
	if (head_index + size > circular_buffer_size) {
		// Need to do 2 copies
		size_t back_size = circular_buffer_size - head_index;
		size_t front_size = size - back_size;
		memcpy(destination, static_cast<const uint8 *>(circular_buffer) + head_index, back_size);
		memcpy(static_cast<uint8 *>(destination) + back_size, circular_buffer, front_size);
		head_index = (head_index + size) - circular_buffer_size;
	} else {
		memcpy(destination, static_cast<const uint8 *>(circular_buffer) + head_index, size);
		head_index += size;
	}
}

static void circular_buffer_push(
	void *circular_buffer,
	size_t circular_buffer_size,
	size_t head_index,
	const void *source,
	size_t size) {
	wl_assert(size <= circular_buffer_size);
	if (head_index < size) {
		// Need to do 2 copies
		size_t prev_head_index = (head_index + circular_buffer_size) - size;
		size_t back_size = circular_buffer_size - prev_head_index;
		size_t front_size = size - back_size;
		memcpy(static_cast<uint8 *>(circular_buffer) + prev_head_index, source, back_size);
		memcpy(circular_buffer, static_cast<const uint8 *>(source) + back_size, front_size);
	} else {
		size_t prev_head_index = head_index - size;
		memcpy(static_cast<uint8 *>(circular_buffer) + prev_head_index, source, size);
	}
}

template<typename t_source>
static void circular_buffer_push_constant(
	void *circular_buffer,
	size_t circular_buffer_size,
	size_t head_index,
	t_source source_value,
	size_t size) {
	wl_assert(size <= circular_buffer_size);
	if (head_index < size) {
		// Need to do 2 copies
		size_t prev_head_index = (head_index + circular_buffer_size) - size;
		size_t back_size = circular_buffer_size - prev_head_index;
		size_t front_size = size - back_size;

		wl_assert(prev_head_index % sizeof(source_value) == 0);
		wl_assert(back_size % sizeof(source_value) == 0);
		wl_assert(front_size % sizeof(source_value) == 0);

		t_source *back_start_ptr = static_cast<t_source *>(circular_buffer) + (prev_head_index / sizeof(source_value));
		t_source *back_end_ptr = back_start_ptr + (back_size / sizeof(source_value));
		for (t_source *ptr = back_start_ptr; ptr < back_end_ptr; ptr++) {
			*ptr = source_value;
		}

		t_source *front_start_ptr = static_cast<t_source *>(circular_buffer);
		t_source *front_end_ptr = front_start_ptr + (front_size / sizeof(source_value));
		for (t_source *ptr = front_start_ptr; ptr < front_end_ptr; ptr++) {
			*ptr = source_value;
		}
	} else {
		size_t prev_head_index = head_index - size;

		wl_assert(prev_head_index % sizeof(source_value) == 0);
		wl_assert(size % sizeof(source_value) == 0);

		t_source *start_ptr = static_cast<t_source *>(circular_buffer) + (prev_head_index / sizeof(source_value));
		t_source *end_ptr = start_ptr + (size / sizeof(source_value));
		for (t_source *ptr = start_ptr; ptr < end_ptr; ptr++) {
			*ptr = source_value;
		}
	}
}

namespace delay_task_functions {

	size_t delay_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		uint32 delay_samples;
		if (std::isnan(duration) || std::isinf(duration)) {
			delay_samples = 0;
		} else {
			delay_samples = static_cast<uint32>(
				std::max(0.0f, std::round(static_cast<real32>(context.sample_rate) * duration)));
		}


		// Allocate the size of the context plus the delay buffer
		return sizeof(s_delay_context) + (delay_samples * sizeof(real32));
	}

	void delay_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, duration)) {
		s_delay_context *delay_context = static_cast<s_delay_context *>(context.task_memory);

		if (std::isnan(duration) || std::isinf(duration)) {
			context.event_interface->submit(EVENT_WARNING << "Invalid delay duration, defaulting to 0");
			delay_context->delay_samples = 0;
		} else {
			delay_context->delay_samples = static_cast<uint32>(
				std::max(0.0f, std::round(static_cast<real32>(context.sample_rate) * duration)));
		}

		delay_context->delay_buffer_head_index = 0;
		delay_context->is_constant = true;

		// Zero out the delay buffer
		real32 *delay_buffer = get_delay_buffer(delay_context);
		zero_type(delay_buffer, delay_context->delay_samples);
	}

	void delay_voice_initializer(const s_task_function_context &context) {
		s_delay_context *delay_context = static_cast<s_delay_context *>(context.task_memory);

		delay_context->delay_buffer_head_index = 0;
		delay_context->is_constant = true;

		// Zero out the delay buffer
		real32 *delay_buffer = get_delay_buffer(delay_context);
		zero_type(delay_buffer, delay_context->delay_samples);
	}

	void delay(
		const s_task_function_context &context,
		wl_task_argument(real32, duration),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(c_real_buffer *, result)) {
		s_delay_context *delay_context = static_cast<s_delay_context *>(context.task_memory);

		real32 *delay_buffer = get_delay_buffer(delay_context);

		if (delay_context->delay_samples == 0) {
			if (signal->is_constant()) {
				result->assign_constant(signal->get_constant());
			} else {
				copy_type(result->get_data(), signal->get_data(), context.buffer_size);
				result->set_is_constant(false);
			}
			return;
		}

		if (signal->is_constant() && delay_context->is_constant) {
			real32 signal_value = signal->get_constant();
			if (signal_value == *delay_buffer) {
				// The input and the delay buffer are constant and identical, so the output is constant and the delay
				// buffer doesn't change
				result->assign_constant(signal_value);
				return;
			}
		}

		// $TODO Keep track of whether we fill the delay buffer with identical constant pushes, so we can set
		// is_constant. Right now, is_constant starts as true, but will always immediately become false forever.
		delay_context->is_constant = false;

		size_t delay_samples_size = delay_context->delay_samples * sizeof(real32);
		size_t samples_to_process = std::min(delay_context->delay_samples, context.buffer_size);
		size_t samples_to_process_size = samples_to_process * sizeof(real32);

		// 1) Copy delay buffer to beginning of output
		circular_buffer_pop(
			delay_buffer,
			delay_samples_size,
			delay_context->delay_buffer_head_index,
			result->get_data(),
			samples_to_process_size);

		// 2) Copy end of input to delay buffer
		if (signal->is_constant()) {
			circular_buffer_push_constant(
				delay_buffer,
				delay_samples_size,
				delay_context->delay_buffer_head_index,
				signal->get_constant(),
				samples_to_process_size);
		} else {
			circular_buffer_push(
				delay_buffer,
				delay_samples_size,
				delay_context->delay_buffer_head_index,
				signal->get_data() + (context.buffer_size - samples_to_process),
				samples_to_process_size);
		}

		if (delay_context->delay_samples < context.buffer_size) {
			// 3) If our delay is smaller than our buffer, it means that some input samples will be immediately used in
			// the output buffer, so copy beginning of input to end of output
			if (signal->is_constant()) {
				real32 signal_value = signal->get_constant();
				real32 *out_start = result->get_data() + delay_context->delay_samples;
				real32 *out_end = out_start + (context.buffer_size - delay_context->delay_samples);
				for (real32 *out_pointer = out_start; out_pointer < out_end; out_pointer++) {
					*out_pointer = signal_value;
				}
			} else {
				copy_type(
					result->get_data() + delay_context->delay_samples,
					signal->get_data(),
					(context.buffer_size - delay_context->delay_samples));
			}
		}

		result->set_is_constant(false);
	}

	size_t memory_real_memory_query(const s_task_function_context &context) {
		return sizeof(s_memory_real_context);
	}

	void memory_real_voice_initializer(const s_task_function_context &context) {
		static_cast<s_memory_real_context *>(context.task_memory)->m_value = 0.0f;
	}

	void memory_real(
		const s_task_function_context &context,
		wl_task_argument(const c_real_buffer *, value),
		wl_task_argument(const c_bool_buffer *, write),
		wl_task_argument(c_real_buffer *, result)) {
		s_memory_real_context *memory_context = static_cast<s_memory_real_context *>(context.task_memory);

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

	size_t memory_bool_memory_query(const s_task_function_context &context) {
		return sizeof(s_memory_bool_context);
	}

	void memory_bool_voice_initializer(const s_task_function_context &context) {
		static_cast<s_memory_bool_context *>(context.task_memory)->m_value = false;
	}

	void memory_bool(
		const s_task_function_context &context,
		wl_task_argument(const c_bool_buffer *, value),
		wl_task_argument(const c_bool_buffer *, write),
		wl_task_argument(c_bool_buffer *, result)) {
		s_memory_bool_context *memory_context = static_cast<s_memory_bool_context *>(context.task_memory);

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

		wl_task_function(0xbd8b8e85, "delay")
			.set_function<delay>()
			.set_memory_query<delay_memory_query>()
			.set_initializer<delay_initializer>()
			.set_voice_initializer<delay_voice_initializer>();

		wl_task_function(0xbe5eb8bd, "memory$real")
			.set_function<memory_real>()
			.set_memory_query<memory_real_memory_query>()
			.set_voice_initializer<memory_real_voice_initializer>();

		wl_task_function(0x1fbebc67, "memory$bool")
			.set_function<memory_bool>()
			.set_memory_query<memory_bool_memory_query>()
			.set_voice_initializer<memory_bool_voice_initializer>();

		wl_end_active_library_task_function_registration();
	}

}
