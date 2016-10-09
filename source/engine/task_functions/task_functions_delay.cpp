#include "engine/task_functions/task_functions_delay.h"
#include "execution_graph/native_modules/native_modules_delay.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/events/event_interface.h"
#include <algorithm>

static const uint32 k_task_functions_delay_library_id = 3;

static const s_task_function_uid k_task_function_delay_in_out_uid = s_task_function_uid::build(k_task_functions_delay_library_id, 0);

struct s_buffer_operation_delay {
	static size_t query_memory(uint32 sample_rate, real32 delay);
	static void initialize(c_event_interface *event_interface,
		s_buffer_operation_delay *context, uint32 sample_rate, real32 delay);
	static void voice_initialize(s_buffer_operation_delay *context);

	static void in_out(
		s_buffer_operation_delay *context, size_t buffer_size, c_real_buffer_or_constant_in in, c_real_buffer_out out);

	// The inout buffer version would require an extra intermediate buffer and more copies, so it's not really worth
	// implementing.

	uint32 delay_samples;
	size_t delay_buffer_head_index;
	bool is_constant;
};

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

template<typename t_source>
static void circular_buffer_push_constant(void *circular_buffer, size_t circular_buffer_size, size_t head_index,
	t_source source_value, size_t size) {
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
		std::max(0.0f, round(static_cast<real32>(sample_rate) * delay)));

	// Allocate the size of the context plus the delay buffer
	return sizeof(s_buffer_operation_delay) + (delay_samples * sizeof(real32));
}

void s_buffer_operation_delay::initialize(c_event_interface *event_interface,
	s_buffer_operation_delay *context, uint32 sample_rate, real32 delay) {
	if (std::isnan(delay) || std::isinf(delay)) {
		event_interface->submit(EVENT_WARNING << "Invalid delay duration, defaulting to 0");
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
	circular_buffer_pop(delay_buffer_ptr, delay_samples_size, context->delay_buffer_head_index,
		out_ptr, samples_to_process_size);

	// 2) Copy end of input to delay buffer
	if (in.is_constant()) {
		circular_buffer_push_constant(delay_buffer_ptr, delay_samples_size, context->delay_buffer_head_index,
			in.get_constant(), samples_to_process_size);
	} else {
		const real32 *in_ptr = in.get_buffer()->get_data<real32>();
		circular_buffer_push(delay_buffer_ptr, delay_samples_size, context->delay_buffer_head_index,
			in_ptr + (buffer_size - samples_to_process), samples_to_process_size);
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
}

static size_t task_memory_query_delay(const s_task_function_context &context) {
	return s_buffer_operation_delay::query_memory(
		context.sample_rate,
		context.arguments[0].get_real_constant_in());
}

static void task_initializer_delay(const s_task_function_context &context) {
	s_buffer_operation_delay::initialize(
		context.event_interface,
		static_cast<s_buffer_operation_delay *>(context.task_memory),
		context.sample_rate,
		context.arguments[0].get_real_constant_in());
}

static void task_voice_initializer_delay(const s_task_function_context &context) {
	s_buffer_operation_delay::voice_initialize(static_cast<s_buffer_operation_delay *>(context.task_memory));
}

static void task_function_delay_in_out(const s_task_function_context &context) {
	s_buffer_operation_delay::in_out(
		static_cast<s_buffer_operation_delay *>(context.task_memory),
		context.buffer_size,
		context.arguments[1].get_real_buffer_or_constant_in(),
		context.arguments[2].get_real_buffer_out());
}

void register_task_functions_delay() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_delay_in_out_uid,
				"delay_in_out",
				task_memory_query_delay, task_initializer_delay, task_voice_initializer_delay, task_function_delay_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_delay_in_out_uid, "vv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 2))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_delay_uid, c_task_function_mapping_list::construct(mappings));
	}
}