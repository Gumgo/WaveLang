#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/controller_interface/controller_interface.h"
#include "engine/events/event_interface.h"
#include "engine/task_function_registration.h"
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

	void get_note_id(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer *, result)) {
		result->assign_constant(static_cast<real32>(context.voice_interface->get_note_id()));
	}

	void get_note_velocity(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer *, result)) {
		result->assign_constant(context.voice_interface->get_note_velocity());
	}

	s_task_memory_query_result get_note_press_duration_memory_query(const s_task_function_context &context) {
		s_task_memory_query_result result;
		result.voice_size_alignment = sizealignof(s_get_note_press_duration_context);
		return result;
	}

	void get_note_press_duration_voice_activator(const s_task_function_context &context) {
		reinterpret_cast<s_get_note_press_duration_context *>(context.voice_memory.get_pointer())->current_sample = 0;
	}

	void get_note_press_duration(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer *, result)) {
		s_get_note_press_duration_context *get_note_press_duration_context =
			reinterpret_cast<s_get_note_press_duration_context *>(context.voice_memory.get_pointer());
		real64 inv_sample_rate = 1.0 / static_cast<real64>(context.sample_rate);
		real64 current_sample = static_cast<real64>(get_note_press_duration_context->current_sample);
		for (size_t index = 0; index < context.buffer_size; index++) {
			result->get_data()[index] = static_cast<real32>(current_sample * inv_sample_rate);
			current_sample += 1.0;
		}

		result->set_is_constant(false);
		get_note_press_duration_context->current_sample += context.buffer_size;
	}

	s_task_memory_query_result get_note_release_duration_memory_query(const s_task_function_context &context) {
		s_task_memory_query_result result;
		result.voice_size_alignment = sizealignof(s_get_note_release_duration_context);
		return result;
	}

	void get_note_release_duration_voice_activator(const s_task_function_context &context) {
		s_get_note_release_duration_context *get_note_release_duration_context =
			reinterpret_cast<s_get_note_release_duration_context *>(context.voice_memory.get_pointer());
		get_note_release_duration_context->released = false;
		get_note_release_duration_context->current_sample = 0;
	}

	void get_note_release_duration(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer *, result)) {
		s_get_note_release_duration_context *get_note_release_duration_context =
			reinterpret_cast<s_get_note_release_duration_context *>(context.voice_memory.get_pointer());
		int32 note_release_sample = context.voice_interface->get_note_release_sample();

		if (!get_note_release_duration_context->released && note_release_sample < 0) {
			result->assign_constant(0.0f);
		} else {
			size_t start_index = (note_release_sample < 0) ? 0 : cast_integer_verify<size_t>(note_release_sample);

			zero_type(result->get_data(), start_index);

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

	void get_parameter_value_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, parameter_id)) {
		// Perform error check only once
		if (*parameter_id < 0.0f || *parameter_id != std::floor(*parameter_id)) {
			context.event_interface->submit(EVENT_ERROR << "Invalid controller parameter ID '" << *parameter_id << "'");
		}
	}

	void get_parameter_value(
		const s_task_function_context &context,
		wl_task_argument(real32, parameter_id),
		wl_task_argument(c_real_buffer *, result)) {
		if (*parameter_id < 0.0f || *parameter_id != std::floor(*parameter_id)) {
			result->assign_constant(0.0f);
			return;
		}

		uint32 integer_parameter_id = static_cast<uint32>(*parameter_id);
		real32 previous_value = 0.0f;
		c_wrapped_array<const s_timestamped_controller_event> parameter_change_events =
			context.controller_interface->get_parameter_change_events(integer_parameter_id, previous_value);

		if (parameter_change_events.get_count() == 0) {
			result->assign_constant(previous_value);
		} else {
			// Since sample_rate and buffer_size are already adjusted for upsampling, we don't need to do any additional
			// work here
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

	void scrape_task_functions() {
		static constexpr uint32 k_controller_library_id = 7;
		wl_task_function_library(k_controller_library_id, "controller", 0);

		wl_task_function(0xbeaf383d, "get_note_id")
			.set_function<get_note_id>();

		wl_task_function(0x8b9d039b, "get_note_velocity")
			.set_function<get_note_velocity>();

		wl_task_function(0x05b9e818, "get_note_press_duration")
			.set_function<get_note_press_duration>()
			.set_memory_query<get_note_press_duration_memory_query>()
			.set_voice_activator<get_note_press_duration_voice_activator>();

		wl_task_function(0xa370e402, "get_note_release_duration")
			.set_function<get_note_release_duration>()
			.set_memory_query<get_note_release_duration_memory_query>()
			.set_voice_activator<get_note_release_duration_voice_activator>();

		wl_task_function(0x6badd8e8, "get_parameter_value")
			.set_function<get_parameter_value>()
			.set_initializer<get_parameter_value_initializer>();

		wl_end_active_library_task_function_registration();
	}

}
