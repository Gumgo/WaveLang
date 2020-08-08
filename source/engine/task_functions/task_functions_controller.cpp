#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/controller_interface/controller_interface.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/task_functions_controller.h"
#include "engine/voice_interface/voice_interface.h"

#include <algorithm>
#include <cmath>

struct s_buffer_operation_controller_get_note_id {
	static void out(int32 note_id, c_real_buffer_out out);
};

struct s_buffer_operation_controller_get_note_velocity {
	static void out(real32 note_velocity, c_real_buffer_out out);
};

struct s_buffer_operation_controller_get_note_press_duration {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_controller_get_note_press_duration *context);
	static void out(
		s_buffer_operation_controller_get_note_press_duration *context,
		size_t buffer_size,
		uint32 sample_rate,
		c_real_buffer_out out);

	int64 current_sample;
};

struct s_buffer_operation_controller_get_note_release_duration {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_controller_get_note_release_duration *context);
	static void out(
		s_buffer_operation_controller_get_note_release_duration *context,
		size_t buffer_size,
		uint32 sample_rate,
		int32 note_release_sample,
		c_real_buffer_out out);

	bool released;
	int64 current_sample;
};

void s_buffer_operation_controller_get_note_id::out(int32 note_id, c_real_buffer_out out) {
	*out->get_data<real32>() = static_cast<real32>(note_id);
	out->set_constant(true);
}

void s_buffer_operation_controller_get_note_velocity::out(real32 note_velocity, c_real_buffer_out out) {
	*out->get_data<real32>() = note_velocity;
	out->set_constant(true);
}

size_t s_buffer_operation_controller_get_note_press_duration::query_memory() {
	return sizeof(s_buffer_operation_controller_get_note_press_duration);
}

void s_buffer_operation_controller_get_note_press_duration::voice_initialize(
	s_buffer_operation_controller_get_note_press_duration *context) {
	context->current_sample = 0;
}

void s_buffer_operation_controller_get_note_press_duration::out(
	s_buffer_operation_controller_get_note_press_duration *context,
	size_t buffer_size,
	uint32 sample_rate,
	c_real_buffer_out out) {
	real32 *out_ptr = out->get_data<real32>();

	real64 inv_sample_rate = 1.0 / static_cast<real64>(sample_rate);
	real64 current_sample = static_cast<real64>(context->current_sample);
	for (size_t index = 0; index < buffer_size; index++) {
		out_ptr[index] = static_cast<real32>(current_sample * inv_sample_rate);
		current_sample += 1.0;
	}

	out->set_constant(false);
	context->current_sample += buffer_size;
}

size_t s_buffer_operation_controller_get_note_release_duration::query_memory() {
	return sizeof(s_buffer_operation_controller_get_note_release_duration);
}

void s_buffer_operation_controller_get_note_release_duration::voice_initialize(
	s_buffer_operation_controller_get_note_release_duration *context) {
	context->released = false;
	context->current_sample = 0;
}

void s_buffer_operation_controller_get_note_release_duration::out(
	s_buffer_operation_controller_get_note_release_duration *context,
	size_t buffer_size,
	uint32 sample_rate,
	int32 note_release_sample,
	c_real_buffer_out out) {
	real32 *out_ptr = out->get_data<real32>();

	if (!context->released && note_release_sample < 0) {
		*out_ptr = 0.0f;
		out->set_constant(true);
	} else {
		size_t start_index = (note_release_sample < 0) ? 0 : cast_integer_verify<size_t>(note_release_sample);

		for (size_t index = 0; index < start_index; index++) {
			out_ptr[index] = 0.0f;
		}

		real64 inv_sample_rate = 1.0 / static_cast<real64>(sample_rate);
		real64 current_sample = static_cast<real64>(context->current_sample);
		for (size_t index = start_index; index < buffer_size; index++) {
			out_ptr[index] = static_cast<real32>(current_sample * inv_sample_rate);
			current_sample += 1.0;
		}

		out->set_constant(false);
		context->released = true;
		context->current_sample += buffer_size;
	}

}

namespace controller_task_functions {

	void get_note_id_out(
		const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_id::out(
			context.voice_interface->get_note_id(), result);
	}

	void get_note_velocity_out(
		const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_velocity::out(
			context.voice_interface->get_note_velocity(), result);
	}

	size_t get_note_press_duration_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_controller_get_note_press_duration::query_memory();
	}

	void get_note_press_duration_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_controller_get_note_press_duration::voice_initialize(
			static_cast<s_buffer_operation_controller_get_note_press_duration *>(context.task_memory));
	}

	void get_note_press_duration_out(
		const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_press_duration::out(
			static_cast<s_buffer_operation_controller_get_note_press_duration *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			result);
	}

	size_t get_note_release_duration_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_controller_get_note_release_duration::query_memory();
	}

	void get_note_release_duration_voice_initializer(const s_task_function_context &context) {
		s_buffer_operation_controller_get_note_release_duration::voice_initialize(
			static_cast<s_buffer_operation_controller_get_note_release_duration *>(context.task_memory));
	}

	void get_note_release_duration_out(
		const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_release_duration::out(
			static_cast<s_buffer_operation_controller_get_note_release_duration *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			context.voice_interface->get_note_release_sample(),
			result);
	}

	void get_parameter_value_initializer(
		const s_task_function_context &context,
		real32 parameter_id) {
		// Perform error check only once
		if (parameter_id < 0.0f || parameter_id != std::floor(parameter_id)) {
			context.event_interface->submit(EVENT_ERROR << "Invalid controller parameter ID '" << parameter_id << "'");
		}
	}

	void get_parameter_value_out(
		const s_task_function_context &context,
		real32 parameter_id,
		c_real_buffer *result) {
		if (parameter_id < 0.0f || parameter_id != std::floor(parameter_id)) {
			*result->get_data<real32>() = 0.0f;
			result->set_constant(true);
			return;
		}

		uint32 integer_parameter_id = static_cast<uint32>(parameter_id);
		real32 previous_value = 0.0f;
		c_wrapped_array<const s_timestamped_controller_event> parameter_change_events =
			context.controller_interface->get_parameter_change_events(integer_parameter_id, previous_value);

		if (parameter_change_events.get_count() == 0) {
			*result->get_data<real32>() = previous_value;
			result->set_constant(true);
		} else {
			// $UPSAMPLE will need to adjust sample indices here
			real64 sample_rate = static_cast<real64>(context.sample_rate);

			real32 *out = result->get_data<real32>();
			real32 current_value = previous_value;
			size_t start_frame = 0;
			for (size_t event_index = 0; event_index < parameter_change_events.get_count(); event_index++) {
				const s_timestamped_controller_event &controller_event = parameter_change_events[event_index];
				wl_assert(controller_event.timestamp_sec >= 0.0);
				uint32 end_frame = static_cast<uint32>(controller_event.timestamp_sec * sample_rate);
				end_frame = std::min(end_frame, context.buffer_size);

				for (size_t frame = start_frame; frame < end_frame; frame++) {
					out[frame] = current_value;
				}

				const s_controller_event_data_parameter_change *parameter_change_event_data =
					controller_event.controller_event.get_data<s_controller_event_data_parameter_change>();
				current_value = parameter_change_event_data->value;
				start_frame = end_frame;
			}

			for (size_t frame = start_frame; frame < context.buffer_size; frame++) {
				out[frame] = current_value;
			}

			result->set_constant(false);
		}
	}

}
