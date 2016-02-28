#include "engine/task_functions/task_functions_time.h"
#include "execution_graph/native_modules/native_modules_time.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/events/event_interface.h"

static const uint32 k_task_functions_time_library_id = 5;

static const s_task_function_uid k_task_function_time_period_out_uid = s_task_function_uid::build(k_task_functions_time_library_id, 0);

struct s_buffer_operation_time_period {
	static size_t query_memory();
	static void initialize(c_event_interface *event_interface, s_buffer_operation_time_period *context, real32 period);

	static void out(
		s_buffer_operation_time_period *context, size_t buffer_size, uint32 sample_rate, real32 period,
		c_real_buffer_out out);

	real32 current_sample;
};

size_t s_buffer_operation_time_period::query_memory() {
	return sizeof(s_buffer_operation_time_period);
}

void s_buffer_operation_time_period::initialize(c_event_interface *event_interface,
	s_buffer_operation_time_period *context, real32 period) {
	if (std::isnan(period) || std::isinf(period) || period <= 0.0f) {
		event_interface->submit(EVENT_WARNING << "Invalid time period, defaulting to 0");
	}
	context->current_sample = 0.0f;
}

void s_buffer_operation_time_period::out(
	s_buffer_operation_time_period *context, size_t buffer_size, uint32 sample_rate, real32 period,
	c_real_buffer_out out) {
	validate_buffer(out);

	real32 *out_ptr = out->get_data<real32>();

	if (std::isnan(period) || std::isinf(period) || period <= 0.0f) {
		*out_ptr = 0.0f;
		out->set_constant(true);
	} else {
		// $TODO could optimize with SSE
		real32 sample_rate_real = static_cast<real32>(sample_rate);
		real32 period_samples = period * sample_rate_real;
		real32 inv_sample_rate = 1.0f / sample_rate_real;

		real32 sample = context->current_sample;
		for (size_t index = 0; index < buffer_size; index++) {
			out_ptr[index] = sample * inv_sample_rate;
			sample += 1.0f;
			if (sample >= period) {
				sample = fmod(sample, period_samples);
			}
		}

		context->current_sample = sample;
	}
}

static size_t task_memory_query_time_period(const s_task_function_context &context) {
	return s_buffer_operation_time_period::query_memory();
}

static void task_initializer_time_period(const s_task_function_context &context) {
	s_buffer_operation_time_period::initialize(
		context.event_interface,
		static_cast<s_buffer_operation_time_period *>(context.task_memory),
		context.arguments[0].get_real_constant_in());
}

static void task_function_time_period_out(const s_task_function_context &context) {
	s_buffer_operation_time_period::out(
		static_cast<s_buffer_operation_time_period *>(context.task_memory),
		context.buffer_size,
		context.sample_rate,
		context.arguments[0].get_real_constant_in(),
		context.arguments[1].get_real_buffer_out());
}

void register_task_functions_time() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_time_period_out_uid,
				"time_period_out",
				task_memory_query_time_period, task_initializer_time_period, task_function_time_period_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_time_period_out_uid, "v.",
			s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_time_period_uid, c_task_function_mapping_list::construct(mappings));
	}
}