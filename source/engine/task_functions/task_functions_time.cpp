#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/task_functions_time.h"

#include <cmath>

struct s_period_context {
	real64 current_sample;
};

namespace time_task_functions {

	size_t period_memory_query(const s_task_function_context &context) {
		return sizeof(s_period_context);
	}

	void period_initializer(const s_task_function_context &context, real32 duration) {
		if (std::isnan(duration) || std::isinf(duration) || duration <= 0.0f) {
			context.event_interface->submit(EVENT_WARNING << "Invalid time period duration, defaulting to 0");
		}

		static_cast<s_period_context *>(context.task_memory)->current_sample = 0.0;
	}

	void period_voice_initializer(const s_task_function_context &context) {
		static_cast<s_period_context *>(context.task_memory)->current_sample = 0.0;
	}

	void period(const s_task_function_context &context, real32 duration, c_real_buffer *result) {
		if (std::isnan(duration) || std::isinf(duration) || duration <= 0.0f) {
			result->assign_constant(0.0f);
		} else {
			s_period_context *period_context = static_cast<s_period_context *>(context.task_memory);

			// $TODO could optimize with SSE
			real64 sample_rate_real = static_cast<real64>(context.sample_rate);
			real64 period_samples = duration * sample_rate_real;
			real64 inv_sample_rate = 1.0 / sample_rate_real;

			real64 sample = period_context->current_sample;
			for (size_t index = 0; index < context.buffer_size; index++) {
				result->get_data()[index] = static_cast<real32>(sample * inv_sample_rate);
				sample += 1.0;
				if (sample >= duration) {
					sample = std::fmod(sample, period_samples);
				}
			}

			result->set_is_constant(false);
			period_context->current_sample = sample;
		}
	}

}
