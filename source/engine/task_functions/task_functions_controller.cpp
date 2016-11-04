#include "engine/buffer.h"
#include "engine/task_function_registry.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/task_functions/task_functions_controller.h"
#include "engine/voice_interface/voice_interface.h"

struct s_buffer_operation_controller_get_note_id {
	static void out(int32 note_id, c_real_buffer_out out);
};

struct s_buffer_operation_controller_get_note_velocity {
	static void out(real32 note_velocity, c_real_buffer_out out);
};

struct s_buffer_operation_controller_get_note_press_duration {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_controller_get_note_press_duration *context);
	static void out(s_buffer_operation_controller_get_note_press_duration *context,
		size_t buffer_size, uint32 sample_rate, c_real_buffer_out out);

	int64 current_sample;
};

struct s_buffer_operation_controller_get_note_release_duration {
	static size_t query_memory();
	static void voice_initialize(s_buffer_operation_controller_get_note_release_duration *context);
	static void out(s_buffer_operation_controller_get_note_release_duration *context,
		size_t buffer_size, uint32 sample_rate, int32 note_release_sample, c_real_buffer_out out);

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
	s_buffer_operation_controller_get_note_press_duration *context, size_t buffer_size, uint32 sample_rate,
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
	s_buffer_operation_controller_get_note_release_duration *context, size_t buffer_size, uint32 sample_rate,
	int32 note_release_sample, c_real_buffer_out out) {
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

	void get_note_id_out(const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_id::out(
			context.voice_interface->get_note_id(), result);
	}

	void get_note_velocity_out(const s_task_function_context &context,
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

	void get_note_press_duration_out(const s_task_function_context &context,
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

	void get_note_release_duration_out(const s_task_function_context &context,
		c_real_buffer *result) {
		s_buffer_operation_controller_get_note_release_duration::out(
			static_cast<s_buffer_operation_controller_get_note_release_duration *>(context.task_memory),
			context.buffer_size,
			context.sample_rate,
			context.voice_interface->get_note_release_sample(),
			result);
	}

}
