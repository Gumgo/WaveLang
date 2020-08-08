#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/task_functions_delay.h"

#include <algorithm>
#include <cmath>

struct s_buffer_operation_delay {
	static size_t query_memory(uint32 sample_rate, real32 delay);
	static void initialize(
		c_event_interface *event_interface,
		s_buffer_operation_delay *context,
		uint32 sample_rate,
		real32 delay);
	static void voice_initialize(s_buffer_operation_delay *context);

	static void in_out(
		s_buffer_operation_delay *context, size_t buffer_size, c_real_buffer_or_constant_in in, c_real_buffer_out out);

	// The inout buffer version would require an extra intermediate buffer and more copies, so it's not really worth
	// implementing.

	uint32 delay_samples;
	size_t delay_buffer_head_index;
	bool is_constant;
};

struct s_buffer_operation_memory_real {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_memory_real *context);

	static void in_in_out(
		s_buffer_operation_memory_real *context,
		size_t buffer_size,
		c_real_buffer_or_constant_in value,
		c_bool_buffer_or_constant_in write,
		c_real_buffer_out result);
	static void inout_in(
		s_buffer_operation_memory_real *context,
		size_t buffer_size,
		c_real_buffer_inout value_result,
		c_bool_buffer_or_constant_in write);

	real32 m_value;
};

struct s_buffer_operation_memory_bool {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_memory_bool *context);

	static void in_in_out(
		s_buffer_operation_memory_bool *context,
		size_t buffer_size,
		c_bool_buffer_or_constant_in value,
		c_bool_buffer_or_constant_in write,
		c_bool_buffer_out result);
	static void inout_in(
		s_buffer_operation_memory_bool *context,
		size_t buffer_size,
		c_bool_buffer_inout value_result,
		c_bool_buffer_or_constant_in write);
	static void in_inout(
		s_buffer_operation_memory_bool *context,
		size_t buffer_size,
		c_bool_buffer_or_constant_in value,
		c_bool_buffer_inout write_result);

	bool m_value;
};

static real32 *get_delay_buffer(s_buffer_operation_delay *context) {
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

size_t s_buffer_operation_delay::query_memory(uint32 sample_rate, real32 delay) {
	if (std::isnan(delay) || std::isinf(delay)) {
		delay = 0.0f;
	}

	uint32 delay_samples = static_cast<uint32>(
		std::max(0.0f, roundf(static_cast<real32>(sample_rate) * delay)));

	// Allocate the size of the context plus the delay buffer
	return sizeof(s_buffer_operation_delay) + (delay_samples * sizeof(real32));
}

void s_buffer_operation_delay::initialize(
	c_event_interface *event_interface,
	s_buffer_operation_delay *context,
	uint32 sample_rate,
	real32 delay) {
	if (std::isnan(delay) || std::isinf(delay)) {
		event_interface->submit(EVENT_WARNING << "Invalid delay duration, defaulting to 0");
		delay = 0.0f;
	}

	context->delay_samples = static_cast<uint32>(
		std::max(0.0f, roundf(static_cast<real32>(sample_rate) * delay)));
	context->delay_buffer_head_index = 0;
	context->is_constant = true;

	// Zero out the delay buffer
	real32 *delay_buffer = get_delay_buffer(context);
	memset(delay_buffer, 0, sizeof(real32) * context->delay_samples);
}

void s_buffer_operation_delay::voice_initialize(s_buffer_operation_delay *context) {
	context->delay_buffer_head_index = 0;
	context->is_constant = true;

	// Zero out the delay buffer
	real32 *delay_buffer = get_delay_buffer(context);
	memset(delay_buffer, 0, sizeof(real32) * context->delay_samples);
}

void s_buffer_operation_delay::in_out(
	s_buffer_operation_delay *context, size_t buffer_size, c_real_buffer_or_constant_in in, c_real_buffer_out out) {
	real32 *delay_buffer_ptr = get_delay_buffer(context);
	real32 *out_ptr = out->get_data<real32>();

	if (context->delay_samples == 0) {
		if (in.is_constant()) {
			*out_ptr = in.get_constant();
			out->set_constant(true);
		} else {
			const real32 *in_ptr = in.get_buffer()->get_data<real32>();
			memcpy(out_ptr, in_ptr, buffer_size * sizeof(real32));
			out->set_constant(false);
		}
		return;
	}

	if (in.is_constant() && context->is_constant) {
		real32 in_val = in.get_constant();
		if (in_val == *delay_buffer_ptr) {
			// The input and the delay buffer are constant and identical, so the output is constant and the delay buffer
			// doesn't change
			*out_ptr = in_val;
			out->set_constant(true);
			return;
		}
	}

	// $TODO Keep track of whether we fill the delay buffer with identical constant pushes, so we can set is_constant.
	// Right now, is_constant starts as true, but will always immediately become false forever.
	context->is_constant = false;

	size_t delay_samples_size = context->delay_samples * sizeof(real32);
	size_t samples_to_process = std::min(static_cast<size_t>(context->delay_samples), buffer_size);
	size_t samples_to_process_size = samples_to_process * sizeof(real32);

	// 1) Copy delay buffer to beginning of output
	circular_buffer_pop(
		delay_buffer_ptr,
		delay_samples_size,
		context->delay_buffer_head_index,
		out_ptr,
		samples_to_process_size);

	// 2) Copy end of input to delay buffer
	if (in.is_constant()) {
		circular_buffer_push_constant(
			delay_buffer_ptr,
			delay_samples_size,
			context->delay_buffer_head_index,
			in.get_constant(),
			samples_to_process_size);
	} else {
		const real32 *in_ptr = in.get_buffer()->get_data<real32>();
		circular_buffer_push(
			delay_buffer_ptr,
			delay_samples_size,
			context->delay_buffer_head_index,
			in_ptr + (buffer_size - samples_to_process),
			samples_to_process_size);
	}

	if (context->delay_samples < buffer_size) {
		// 3) If our delay is smaller than our buffer, it means that some input samples will be immediately used in the
		// output buffer, so copy beginning of input to end of output
		if (in.is_constant()) {
			real32 in_val = in.get_constant();
			real32 *out_ptr_start = out_ptr + context->delay_samples;
			real32 *out_ptr_end = out_ptr_start + (buffer_size - context->delay_samples);
			for (real32 *ptr = out_ptr_start; ptr < out_ptr_end; ptr++) {
				*ptr = in_val;
			}
		} else {
			const real32 *in_ptr = in.get_buffer()->get_data<real32>();
			memcpy(out_ptr + context->delay_samples, in_ptr, (buffer_size - context->delay_samples) * sizeof(real32));
		}
	}

	out->set_constant(false);
}

size_t s_buffer_operation_memory_real::query_memory() {
	return sizeof(s_buffer_operation_memory_real);
}

void s_buffer_operation_memory_real::voice_initialize(s_buffer_operation_memory_real *context) {
	context->m_value = 0.0f;
}

void s_buffer_operation_memory_real::in_in_out(
	s_buffer_operation_memory_real *context,
	size_t buffer_size,
	c_real_buffer_or_constant_in value,
	c_bool_buffer_or_constant_in write,
	c_real_buffer_out result) {
	real32 *result_ptr = result->get_data<real32>();

	// Put this value on the stack to avoid memory reads each iteration
	real32 memory_value = context->m_value;

	if (value.is_constant()) {
		real32 value_val = value.get_constant();

		if (write.is_constant()) {
			bool write_val = write.get_constant();
			memory_value = write_val ? value_val : memory_value;
			*result_ptr = memory_value;
			result->set_constant(true);
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				result_ptr[index] = memory_value;
			}

			result->set_constant(false);
		}
	} else {
		const real32 *value_ptr = value.get_buffer()->get_data<real32>();

		if (write.is_constant()) {
			bool write_val = write.get_constant();

			if (write_val) {
				memcpy(result_ptr, value_ptr, buffer_size * sizeof(real32));
				memory_value = value_ptr[buffer_size - 1];
				result->set_constant(false);
			} else {
				*result_ptr = memory_value;
				result->set_constant(true);
			}
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				real32 value_val = value_ptr[index];
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				result_ptr[index] = memory_value;
			}

			result->set_constant(false);
		}
	}

	context->m_value = memory_value;
}

void s_buffer_operation_memory_real::inout_in(
	s_buffer_operation_memory_real *context,
	size_t buffer_size,
	c_real_buffer_inout value_result,
	c_bool_buffer_or_constant_in write) {
	real32 *value_result_ptr = value_result->get_data<real32>();

	// Put this value on the stack to avoid memory reads each iteration
	real32 memory_value = context->m_value;

	if (value_result->is_constant()) {
		real32 value_val = *value_result->get_data<real32>();

		if (write.is_constant()) {
			bool write_val = write.get_constant();
			memory_value = write_val ? value_val : memory_value;
			*value_result_ptr = memory_value;
			value_result->set_constant(true);
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				value_result_ptr[index] = memory_value;
			}

			value_result->set_constant(false);
		}
	} else {
		if (write.is_constant()) {
			bool write_val = write.get_constant();

			if (write_val) {
				// Nothing to do! The input already equals the output
				memory_value = value_result_ptr[buffer_size - 1];
			} else {
				*value_result_ptr = memory_value;
				value_result->set_constant(true);
			}
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				real32 value_val = value_result_ptr[index];
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				value_result_ptr[index] = memory_value;
			}

			value_result->set_constant(false);
		}
	}

	context->m_value = memory_value;
}

size_t s_buffer_operation_memory_bool::query_memory() {
	return sizeof(s_buffer_operation_memory_bool);
}

void s_buffer_operation_memory_bool::voice_initialize(s_buffer_operation_memory_bool *context) {
	context->m_value = false;
}

void s_buffer_operation_memory_bool::in_in_out(
	s_buffer_operation_memory_bool *context,
	size_t buffer_size,
	c_bool_buffer_or_constant_in value,
	c_bool_buffer_or_constant_in write,
	c_bool_buffer_out result) {
	uint32 *result_ptr = result->get_data<uint32>();

	// Put this value on the stack to avoid memory reads each iteration
	bool memory_value = context->m_value;

	if (value.is_constant()) {
		bool value_val = value.get_constant();

		if (write.is_constant()) {
			bool write_val = write.get_constant();
			memory_value = write_val ? value_val : memory_value;
			*result_ptr = memory_value ? 1 : 0;
			result->set_constant(true);
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			// $TODO could optimize this (and similar loops) to use uint32 blocks
			for (size_t index = 0; index < buffer_size; index++) {
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				result_ptr[index] = memory_value ?
					result_ptr[index] | (1 << index % 32) :
					result_ptr[index] & ~(1 << index % 32);
			}

			result->set_constant(false);
		}
	} else {
		const uint32 *value_ptr = value.get_buffer()->get_data<uint32>();

		if (write.is_constant()) {
			bool write_val = write.get_constant();

			if (write_val) {
				memcpy(result_ptr, value_ptr, BOOL_BUFFER_INT32_COUNT(buffer_size) * sizeof(uint32));
				size_t last_index = buffer_size - 1;
				memory_value = test_bit(value_ptr[last_index / 32], last_index % 32);
				result->set_constant(false);
			} else {
				*result_ptr = memory_value;
				result->set_constant(true);
			}
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				bool value_val = test_bit(value_ptr[index / 32], index % 32);
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				result_ptr[index] = memory_value ?
					result_ptr[index] | (1 << index % 32) :
					result_ptr[index] & ~(1 << index % 32);
			}

			result->set_constant(false);
		}
	}

	context->m_value = memory_value;
}

void s_buffer_operation_memory_bool::inout_in(
	s_buffer_operation_memory_bool *context,
	size_t buffer_size,
	c_bool_buffer_inout value_result,
	c_bool_buffer_or_constant_in write) {
	uint32 *value_result_ptr = value_result->get_data<uint32>();

	// Put this value on the stack to avoid memory reads each iteration
	bool memory_value = context->m_value;

	if (value_result->is_constant()) {
		bool value_val = (*value_result->get_data<uint32>() & 1) != 0;

		if (write.is_constant()) {
			bool write_val = write.get_constant();
			memory_value = write_val ? value_val : memory_value;
			*value_result_ptr = memory_value ? 1 : 0;
			value_result->set_constant(true);
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			// $TODO could optimize this (and similar loops) to use uint32 blocks
			for (size_t index = 0; index < buffer_size; index++) {
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				value_result_ptr[index] = memory_value ?
					value_result_ptr[index] | (1 << index % 32) :
					value_result_ptr[index] & ~(1 << index % 32);
			}

			value_result->set_constant(false);
		}
	} else {
		if (write.is_constant()) {
			bool write_val = write.get_constant();

			if (write_val) {
				// Nothing to do! The input already equals the output
				size_t last_index = buffer_size - 1;
				memory_value = test_bit(value_result_ptr[last_index / 32], last_index % 32);
			} else {
				*value_result_ptr = memory_value ? 1 : 0;
				value_result->set_constant(true);
			}
		} else {
			const uint32 *write_ptr = write.get_buffer()->get_data<uint32>();

			for (size_t index = 0; index < buffer_size; index++) {
				bool value_val = test_bit(value_result_ptr[index / 32], index % 32);
				bool write_val = test_bit(write_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				value_result_ptr[index] = memory_value ?
					value_result_ptr[index] | (1 << index % 32) :
					value_result_ptr[index] & ~(1 << index % 32);
			}

			value_result->set_constant(false);
		}
	}

	context->m_value = memory_value;
}

void s_buffer_operation_memory_bool::in_inout(
	s_buffer_operation_memory_bool *context,
	size_t buffer_size,
	c_bool_buffer_or_constant_in value,
	c_bool_buffer_inout write_result) {
	uint32 *write_result_ptr = write_result->get_data<uint32>();

	// Put this value on the stack to avoid memory reads each iteration
	bool memory_value = context->m_value;

	if (value.is_constant()) {
		bool value_val = value.get_constant();

		if (write_result->is_constant()) {
			bool write_val = (*write_result->get_data<uint32>() & 1) != 0;
			memory_value = write_val ? value_val : memory_value;
			*write_result_ptr = memory_value ? 1 : 0;
			write_result->set_constant(true);
		} else {
			// $TODO could optimize this (and similar loops) to use uint32 blocks
			for (size_t index = 0; index < buffer_size; index++) {
				bool write_val = test_bit(write_result_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				write_result_ptr[index] = memory_value ?
					write_result_ptr[index] | (1 << index % 32) :
					write_result_ptr[index] & ~(1 << index % 32);
			}

			write_result->set_constant(false);
		}
	} else {
		const uint32 *value_ptr = value.get_buffer()->get_data<uint32>();

		if (write_result->is_constant()) {
			bool write_val = (*write_result->get_data<uint32>() & 1) != 0;

			if (write_val) {
				memcpy(write_result_ptr, value_ptr, BOOL_BUFFER_INT32_COUNT(buffer_size) * sizeof(uint32));
				size_t last_index = buffer_size - 1;
				memory_value = test_bit(value_ptr[last_index / 32], last_index % 32);
				write_result->set_constant(false);
			} else {
				*write_result_ptr = memory_value;
				write_result->set_constant(true);
			}
		} else {
			for (size_t index = 0; index < buffer_size; index++) {
				bool value_val = test_bit(value_ptr[index / 32], index % 32);
				bool write_val = test_bit(write_result_ptr[index / 32], index % 32);
				memory_value = write_val ? value_val : memory_value;
				write_result_ptr[index] = memory_value ?
					write_result_ptr[index] | (1 << index % 32) :
					write_result_ptr[index] & ~(1 << index % 32);
			}

			write_result->set_constant(false);
		}
	}

	context->m_value = memory_value;
}

namespace delay_task_functions {

	size_t delay_memory_query(
		const s_task_function_context &context,
		real32 duration) {
		return s_buffer_operation_delay::query_memory(
			context.sample_rate,
			duration);
	}

	void delay_initializer(
		const s_task_function_context &context,
		real32 duration) {
		s_buffer_operation_delay::initialize(
			context.event_interface,
			static_cast<s_buffer_operation_delay *>(context.task_memory),
			context.sample_rate,
			duration);
	}

	void delay_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_delay::voice_initialize(static_cast<s_buffer_operation_delay *>(context.task_memory));
	}

	void delay_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant signal,
		c_real_buffer *result) {
		s_buffer_operation_delay::in_out(
			static_cast<s_buffer_operation_delay *>(context.task_memory),
			context.buffer_size,
			signal,
			result);
	}

	size_t memory_real_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_memory_real::query_memory();
	}

	void memory_real_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_memory_real::voice_initialize(
			static_cast<s_buffer_operation_memory_real *>(context.task_memory));
	}

	void memory_real_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant value,
		c_bool_const_buffer_or_constant write,
		c_real_buffer *result) {
		s_buffer_operation_memory_real::in_in_out(
			static_cast<s_buffer_operation_memory_real *>(context.task_memory),
			context.buffer_size,
			value,
			write,
			result);
	}

	void memory_real_inout_in(
		const s_task_function_context &context,
		c_real_buffer *value_result,
		c_bool_const_buffer_or_constant write) {
		s_buffer_operation_memory_real::inout_in(
			static_cast<s_buffer_operation_memory_real *>(context.task_memory),
			context.buffer_size,
			value_result,
			write);
	}

	size_t memory_bool_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_memory_bool::query_memory();
	}

	void memory_bool_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_memory_bool::voice_initialize(
			static_cast<s_buffer_operation_memory_bool *>(context.task_memory));
	}

	void memory_bool_in_in_out(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant value,
		c_bool_const_buffer_or_constant write,
		c_bool_buffer *result) {
		s_buffer_operation_memory_bool::in_in_out(
			static_cast<s_buffer_operation_memory_bool *>(context.task_memory),
			context.buffer_size,
			value,
			write,
			result);
	}

	void memory_bool_inout_in(
		const s_task_function_context &context,
		c_bool_buffer *value_result,
		c_bool_const_buffer_or_constant write) {
		s_buffer_operation_memory_bool::inout_in(
			static_cast<s_buffer_operation_memory_bool *>(context.task_memory),
			context.buffer_size,
			value_result,
			write);
	}

	void memory_bool_in_inout(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant value,
		c_bool_buffer *write_result) {
		s_buffer_operation_memory_bool::in_inout(
			static_cast<s_buffer_operation_memory_bool *>(context.task_memory),
			context.buffer_size,
			value,
			write_result);
	}

}
