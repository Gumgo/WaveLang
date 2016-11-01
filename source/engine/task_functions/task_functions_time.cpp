#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/task_functions_time.h"

#include "execution_graph/native_modules/native_modules_time.h"

struct s_buffer_operation_time_period {
	static size_t query_memory();
	static void initialize(c_event_interface *event_interface, s_buffer_operation_time_period *context,
		real32 duration);
	static void voice_initialize(s_buffer_operation_time_period *context);

	static void out(
		s_buffer_operation_time_period *context, size_t buffer_size, uint32 sample_rate, real32 duration,
		c_real_buffer_out out);

	real64 current_sample;
};

size_t s_buffer_operation_time_period::query_memory() {
	return sizeof(s_buffer_operation_time_period);
}

void s_buffer_operation_time_period::initialize(c_event_interface *event_interface,
	s_buffer_operation_time_period *context, real32 duration) {
	if (std::isnan(duration) || std::isinf(duration) || duration <= 0.0f) {
		event_interface->submit(EVENT_WARNING << "Invalid time period duration, defaulting to 0");
	}
	context->current_sample = 0.0;
}

void s_buffer_operation_time_period::voice_initialize(s_buffer_operation_time_period *context) {
	context->current_sample = 0.0;
}

void s_buffer_operation_time_period::out(
	s_buffer_operation_time_period *context, size_t buffer_size, uint32 sample_rate, real32 duration,
	c_real_buffer_out out) {
	validate_buffer(out);

	real32 *out_ptr = out->get_data<real32>();

	if (std::isnan(duration) || std::isinf(duration) || duration <= 0.0f) {
		*out_ptr = 0.0f;
		out->set_constant(true);
	} else {
		// $TODO could optimize with SSE
		real64 sample_rate_real = static_cast<real64>(sample_rate);
		real64 period_samples = duration * sample_rate_real;
		real64 inv_sample_rate = 1.0 / sample_rate_real;

		real64 sample = context->current_sample;
		for (size_t index = 0; index < buffer_size; index++) {
			out_ptr[index] = static_cast<real32>(sample * inv_sample_rate);
			sample += 1.0;
			if (sample >= duration) {
				sample = fmod(sample, period_samples);
			}
		}

		out->set_constant(false);
		context->current_sample = sample;
	}
}

namespace time_task_functions {

	size_t period_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_time_period::query_memory();
	}

	void period_initializer(const s_task_function_context &context,
		real32 duration) {
		s_buffer_operation_time_period::initialize(
			context.event_interface,
			static_cast<s_buffer_operation_time_period *>(context.task_memory),
			duration);
	}

	void period_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_time_period::voice_initialize(
			static_cast<s_buffer_operation_time_period *>(context.task_memory));
	}

	void period_out(const s_task_function_context &context,
		real32 duration,
		c_real_buffer *result) {
		s_buffer_operation_time_period::out(
			static_cast<s_buffer_operation_time_period *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			duration,
			result);
	}

}
