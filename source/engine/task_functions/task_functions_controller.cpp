#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/controller_interface/controller_interface.h"
#include "engine/events/event_interface.h"
#include "engine/task_functions/task_functions_controller.h"
#include "engine/voice_interface/voice_interface.h"

#include <algorithm>
#include <cmath>

struct s_get_note_press_duration_context {
	int64 current_sample;
};

struct s_get_note_release_duration_context {
	bool released;
	int64 current_sample;
};

namespace controller_task_functions {

	void get_note_id(const s_task_function_context &context, c_real_buffer *result) {
		result->assign_constant(static_cast<real32>(context.voice_interface->get_note_id()));
	}

	void get_note_velocity(const s_task_function_context &context, c_real_buffer *result) {
		result->assign_constant(context.voice_interface->get_note_velocity());
	}

	size_t get_note_press_duration_memory_query(const s_task_function_context &context) {
		return sizeof(s_get_note_press_duration_context);
	}

	void get_note_press_duration_voice_initializer(const s_task_function_context &context) {
		static_cast<s_get_note_press_duration_context *>(context.task_memory)->current_sample = 0;
	}

	void get_note_press_duration(const s_task_function_context &context, c_real_buffer *result) {
		s_get_note_press_duration_context *get_note_press_duration_context =
			static_cast<s_get_note_press_duration_context *>(context.task_memory);
		real64 inv_sample_rate = 1.0 / static_cast<real64>(context.sample_rate);
		real64 current_sample = static_cast<real64>(get_note_press_duration_context->current_sample);
		for (size_t index = 0; index < context.buffer_size; index++) {
			result->get_data()[index] = static_cast<real32>(current_sample * inv_sample_rate);
			current_sample += 1.0;
		}

		result->set_is_constant(false);
		get_note_press_duration_context->current_sample += context.buffer_size;
	}

	size_t get_note_release_duration_memory_query(const s_task_function_context &context) {
		return sizeof(s_get_note_release_duration_context);
	}

	void get_note_release_duration_voice_initializer(const s_task_function_context &context) {
		s_get_note_release_duration_context *get_note_release_duration_context =
			static_cast<s_get_note_release_duration_context *>(context.task_memory);
		get_note_release_duration_context->released = false;
		get_note_release_duration_context->current_sample = 0;
	}

	void get_note_release_duration(const s_task_function_context &context, c_real_buffer *result) {
		s_get_note_release_duration_context *get_note_release_duration_context =
			static_cast<s_get_note_release_duration_context *>(context.task_memory);
		int32 note_release_sample = context.voice_interface->get_note_release_sample();

		if (!get_note_release_duration_context->released && note_release_sample < 0) {
			result->assign_constant(0.0f);
		} else {
			size_t start_index = (note_release_sample < 0) ? 0 : cast_integer_verify<size_t>(note_release_sample);

			memset(result->get_data(), 0, start_index * sizeof(real32));

			real64 inv_sample_rate = 1.0 / static_cast<real64>(context.sample_rate);
			real64 current_sample = static_cast<real64>(get_note_release_duration_context->current_sample);
			for (size_t index = start_index; index < context.buffer_size; index++) {
				result->get_data()[index] = static_cast<real32>(current_sample * inv_sample_rate);
				current_sample += 1.0;
			}

			result->set_is_constant(false);
			get_note_release_duration_context->released = true;
			get_note_release_duration_context->current_sample += context.buffer_size;
		}
	}

	void get_parameter_value_initializer(const s_task_function_context &context, real32 parameter_id) {
		// Perform error check only once
		if (parameter_id < 0.0f || parameter_id != std::floor(parameter_id)) {
			context.event_interface->submit(EVENT_ERROR << "Invalid controller parameter ID '" << parameter_id << "'");
		}
	}

	void get_parameter_value(const s_task_function_context &context, real32 parameter_id, c_real_buffer *result) {
		if (parameter_id < 0.0f || parameter_id != std::floor(parameter_id)) {
			result->assign_constant(0.0f);
			return;
		}

		uint32 integer_parameter_id = static_cast<uint32>(parameter_id);
		real32 previous_value = 0.0f;
		c_wrapped_array<const s_timestamped_controller_event> parameter_change_events =
			context.controller_interface->get_parameter_change_events(integer_parameter_id, previous_value);

		if (parameter_change_events.get_count() == 0) {
			result->assign_constant(previous_value);
		} else {
			// $UPSAMPLE will need to adjust sample indices here
			real64 sample_rate = static_cast<real64>(context.sample_rate);

			real32 current_value = previous_value;
			size_t start_frame = 0;
			for (size_t event_index = 0; event_index < parameter_change_events.get_count(); event_index++) {
				const s_timestamped_controller_event &controller_event = parameter_change_events[event_index];
				wl_assert(controller_event.timestamp_sec >= 0.0);
				uint32 end_frame = static_cast<uint32>(controller_event.timestamp_sec * sample_rate);
				end_frame = std::min(end_frame, context.buffer_size);

				for (size_t frame = start_frame; frame < end_frame; frame++) {
					result->get_data()[frame] = current_value;
				}

				const s_controller_event_data_parameter_change *parameter_change_event_data =
					controller_event.controller_event.get_data<s_controller_event_data_parameter_change>();
				current_value = parameter_change_event_data->value;
				start_frame = end_frame;
			}

			for (size_t frame = start_frame; frame < context.buffer_size; frame++) {
				result->get_data()[frame] = current_value;
			}

			result->set_is_constant(false);
		}
	}
}
