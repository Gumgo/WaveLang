#include "engine/buffer_operations/buffer_operations_delay.h"
#include <algorithm>

static real32 *get_delay_buffer(s_buffer_operation_delay *context) {
	// Delay buffer is allocated directly after the context
	return reinterpret_cast<real32 *>(context + 1);
}

// Circular buffer functions: it is assumed that pop() will always be followed by push() using the same size. Therefore,
// we only track one head pointer, and assume all pushes will exactly replace popped data.

static void circular_buffer_pop(const void *circular_buffer, size_t circular_buffer_size, size_t &head_index,
	void *destination, size_t size) {
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

static void circular_buffer_push(void *circular_buffer, size_t circular_buffer_size, size_t head_index,
	const void *source, size_t size) {
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

size_t s_buffer_operation_delay::query_memory(uint32 sample_rate, real32 delay) {
	if (std::isnan(delay) || std::isinf(delay)) {
		delay = 0.0f;
	}

	uint32 delay_samples = static_cast<uint32>(
		std::max(0.0f, round(static_cast<real32>(sample_rate) * delay)));

	// Allocate the size of the context plus the delay buffer
	return sizeof(s_buffer_operation_delay) + (delay_samples * sizeof(real32));
}

void s_buffer_operation_delay::initialize(s_buffer_operation_delay *context, uint32 sample_rate, real32 delay) {
	if (std::isnan(delay) || std::isinf(delay)) {
		delay = 0.0f;
	}

	context->delay_samples = static_cast<uint32>(
		std::max(0.0f, round(static_cast<real32>(sample_rate) * delay)));
	context->delay_buffer_head_index = 0;
	context->is_constant = true;

	// Zero out the delay buffer
	real32 *delay_buffer = get_delay_buffer(context);
	memset(delay_buffer, 0, sizeof(real32) * context->delay_samples);
}

void s_buffer_operation_delay::buffer(
	s_buffer_operation_delay *context, size_t buffer_size, c_buffer_out out, c_buffer_in in) {
	validate_buffer(out);
	validate_buffer(in);

	real32 *delay_buffer_ptr = get_delay_buffer(context);
	const real32 *in_ptr = in->get_data<real32>();
	real32 *out_ptr = out->get_data<real32>();

	if (context->delay_samples == 0) {
		if (in->is_constant()) {
			c_real32_4 in_val(in_ptr);
			in_val.store(out_ptr);
			out->set_constant(true);
		} else {
			memcpy(out_ptr, in_ptr, buffer_size * sizeof(real32));
			out->set_constant(false);
		}
		return;
	}

	if (in->is_constant() && context->is_constant &&
		*in_ptr == *delay_buffer_ptr) {
		// The input and the delay buffer are constant and identical, so the output is constant and the delay buffer
		// doesn't change
		c_real32_4 in_val(in_ptr);
		in_val.store(out_ptr);
		out->set_constant(true);
		return;
	}

	// $TODO Keep track of whether we fill the delay buffer with identical constant pushes, so we can set is_constant.
	// Right now, is_constant starts as true, but will always immediately become false forever.
	context->is_constant = false;

	size_t delay_samples_size = context->delay_samples * sizeof(real32);
	size_t samples_to_process = std::min(context->delay_samples, buffer_size);
	size_t samples_to_process_size = samples_to_process * sizeof(real32);

	// 1) Copy delay buffer to beginning of output
	circular_buffer_pop(delay_buffer_ptr, delay_samples_size, context->delay_buffer_head_index,
		out_ptr, samples_to_process_size);

	// 2) Copy end of input to delay buffer
	circular_buffer_push(delay_buffer_ptr, delay_samples_size, context->delay_buffer_head_index,
		in_ptr + (buffer_size - samples_to_process), samples_to_process_size);

	if (context->delay_samples < buffer_size) {
		// 3) If our delay is smaller than our buffer, it means that some input samples will be immediately used in the
		// output buffer, so copy beginning of input to end of output
		memcpy(out_ptr + context->delay_samples, in_ptr, (buffer_size - context->delay_samples) * sizeof(real32));
	}
}