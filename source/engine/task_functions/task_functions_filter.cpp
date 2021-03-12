#include "common/utility/stack_allocator.h"

#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_function_registration.h"
#include "engine/task_functions/filter/allpass.h"
#include "engine/task_functions/filter/comb_feedback.h"
#include "engine/task_functions/filter/fir.h"
#include "engine/task_functions/filter/gain.h"
#include "engine/task_functions/filter/iir_sos.h"

struct s_iir_sos_context
{
	// State for each SOS
	c_wrapped_array<s_iir_sos_state> states;

	// Whether each SOS is first order. Note that this could go in the shared memory but it's more convenient this way
	c_wrapped_array<bool> is_first_order;
};

struct s_allpass_context {
	c_wrapped_array<c_allpass> allpass_filters;
};

struct s_comb_feedback_context {
	c_comb_feedback comb_feedback;
	bool fir_enabled;
	c_fir fir;
	s_iir_sos_context iir_sos_context;
	s_allpass_context allpass_context;
};

namespace filter_task_functions {

	s_task_memory_query_result fir_memory_query(
		wl_task_argument(c_real_constant_array, coefficients)) {
		s_task_memory_query_result result;

		c_stack_allocator::c_memory_calculator shared_calculator;
		shared_calculator.add<c_fir_coefficients>();
		c_fir_coefficients::calculate_memory(coefficients->get_count(), shared_calculator);
		wl_assert(shared_calculator.get_destructor_count() == 0);
		result.shared_size_alignment = shared_calculator.get_size_alignment();

		c_stack_allocator::c_memory_calculator voice_calculator;
		voice_calculator.add<c_fir>();
		c_fir::calculate_memory(coefficients->get_count(), voice_calculator);
		wl_assert(voice_calculator.get_destructor_count() == 0);
		result.voice_size_alignment = voice_calculator.get_size_alignment();

		return result;
	}

	void fir_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, coefficients)) {
		c_stack_allocator allocator(context.shared_memory);
		c_fir_coefficients *fir_coefficients;
		allocator.allocate(fir_coefficients);
		fir_coefficients->initialize(*coefficients, allocator);

		allocator.release_no_destructors();
	}

	void fir_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, coefficients)) {
		c_stack_allocator allocator(context.voice_memory);
		c_fir *fir;
		allocator.allocate(fir);
		fir->initialize(coefficients->get_count(), allocator);

		allocator.release_no_destructors();
	}

	void fir_voice_activator(
		const s_task_function_context &context) {
		reinterpret_cast<c_fir *>(context.voice_memory.get_pointer())->reset();
	}

	void fir(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, coefficients),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(c_real_buffer *, result)) {
		const c_fir_coefficients *fir_coefficients =
			reinterpret_cast<const c_fir_coefficients *>(context.shared_memory.get_pointer());
		c_fir *fir = reinterpret_cast<c_fir *>(context.voice_memory.get_pointer());

		if (signal->is_constant()) {
			bool is_output_constant = fir->process_constant(
				*fir_coefficients,
				signal->get_constant(),
				result->get_data(),
				context.buffer_size);
			if (is_output_constant) {
				result->extend_constant();
			} else {
				result->set_is_constant(false);
			}
		} else {
			fir->process(*fir_coefficients, signal->get_data(), result->get_data(), context.buffer_size);
			result->set_is_constant(false);
		}
	}

	s_task_memory_query_result iir_sos_memory_query(
		wl_task_argument(c_real_buffer_array_in, coefficients)) {
		s_task_memory_query_result result;

		c_stack_allocator::c_memory_calculator calculator;
		size_t sos_count = coefficients->get_count() / 5;
		wl_assert(sos_count * 5 == coefficients->get_count());
		calculator.add<s_iir_sos_context>();
		calculator.add_array<s_iir_sos_state>(sos_count);
		calculator.add_array<bool>(sos_count);
		wl_assert(calculator.get_destructor_count() == 0);
		result.voice_size_alignment = calculator.get_size_alignment();

		return result;
	}

	void iir_sos_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer_array_in, coefficients)) {
		c_stack_allocator allocator(context.voice_memory);
		size_t sos_count = coefficients->get_count() / 5;
		wl_assert(sos_count * 5 == coefficients->get_count());
		s_iir_sos_context *iir_sos_context;
		allocator.allocate(iir_sos_context);
		allocator.allocate_array(iir_sos_context->states, sos_count);
		allocator.allocate_array(iir_sos_context->is_first_order, sos_count);

		for (size_t sos_index = 0; sos_index < sos_count; sos_index++) {
			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b2 = (*coefficients)[coefficients_start_index + 2];
			const c_real_buffer *a2 = (*coefficients)[coefficients_start_index + 4];
			iir_sos_context->is_first_order[sos_index] =
				b2->is_compile_time_constant()
				&& b2->get_constant() == 0.0f
				&& a2->is_compile_time_constant()
				&& a2->get_constant() == 0.0f;
		}

		allocator.release_no_destructors();
	}

	void iir_sos_voice_activator(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer_array_in, coefficients)) {
		s_iir_sos_context *iir_sos_context = reinterpret_cast<s_iir_sos_context *>(context.voice_memory.get_pointer());
		for (s_iir_sos_state &state : iir_sos_context->states) {
			state.reset();
		}
	}

	void iir_sos(
		const s_task_function_context &context,
		wl_task_argument(c_real_buffer_array_in, coefficients),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(c_real_buffer *, result)) {
		s_iir_sos_context *iir_sos_context = reinterpret_cast<s_iir_sos_context *>(context.voice_memory.get_pointer());

		for (size_t sos_index = 0; sos_index < iir_sos_context->states.get_count(); sos_index++) {
			// Make a copy of the state for cache locality
			s_iir_sos_state state = iir_sos_context->states[sos_index];

			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b0 = (*coefficients)[coefficients_start_index];
			const c_real_buffer *b1 = (*coefficients)[coefficients_start_index + 1];
			const c_real_buffer *a1 = (*coefficients)[coefficients_start_index + 3];

			if (iir_sos_context->is_first_order[sos_index]) {
				iterate_buffers<1, false>(context.buffer_size, b0, b1, a1, *signal, *result,
					[&state](size_t i, real32 b0, real32 b1, real32 a1, real32 signal, real32 &result) {
						result = state.process_first_order_single_sample(b0, b1, a1, signal);
					});
			} else {
				const c_real_buffer *b2 = (*coefficients)[coefficients_start_index + 2];
				const c_real_buffer *a2 = (*coefficients)[coefficients_start_index + 4];
				iterate_buffers<1, false>(context.buffer_size, b0, b1, b2, a1, a2, *signal, *result,
					[&state](
						size_t i,
						real32 b0,
						real32 b1,
						real32 b2,
						real32 a1,
						real32 a2,
						real32 signal,
						real32 &result) {
						result = state.process_single_sample(b0, b1, b2, a1, a2, signal);
					});
			}

			// Update the state using the local copy
			iir_sos_context->states[sos_index] = state;
		}
	}

	s_task_memory_query_result allpass_memory_query(
		wl_task_argument(c_real_constant_array, delays)) {
		s_task_memory_query_result result;

		c_stack_allocator::c_memory_calculator calculator;
		calculator.add<s_allpass_context>();
		calculator.add_array<c_allpass>(delays->get_count());
		for (size_t allpass_index = 0; allpass_index < delays->get_count(); allpass_index++) {
			c_allpass::calculate_memory(static_cast<uint32>((*delays)[allpass_index]), calculator);
		}

		wl_assert(calculator.get_destructor_count() == 0);
		result.voice_size_alignment = calculator.get_size_alignment();

		return result;
	}

	void allpass_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, delays),
		wl_task_argument(c_real_constant_array, gains)) {
		c_stack_allocator allocator(context.voice_memory);
		s_allpass_context *allpass_context;
		allocator.allocate(allpass_context);
		allocator.allocate_array(allpass_context->allpass_filters, delays->get_count());
		for (size_t allpass_index = 0; allpass_index < delays->get_count(); allpass_index++) {
			allpass_context->allpass_filters[allpass_index].initialize(
				static_cast<uint32>((*delays)[allpass_index]),
				(*gains)[allpass_index],
				allocator);
		}

		allocator.release_no_destructors();
	}

	void allpass_voice_activator(
		const s_task_function_context &context) {
		s_allpass_context *allpass_context = reinterpret_cast<s_allpass_context *>(context.voice_memory.get_pointer());
		for (c_allpass &allpass : allpass_context->allpass_filters) {
			allpass.reset();
		}
	}

	void allpass(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, delays),
		wl_task_argument(c_real_constant_array, gains),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(c_real_buffer *, result)) {
		s_allpass_context *allpass_context = reinterpret_cast<s_allpass_context *>(context.voice_memory.get_pointer());

		// For the first filter, read from the input buffer. For the remaining filters, process the output buffer
		// in-place.
		const c_real_buffer *input = *signal;
		for (c_allpass &allpass : allpass_context->allpass_filters) {
			if (input->is_constant()) {
				bool is_output_constant = allpass.process_constant(
					input->get_constant(),
					result->get_data(),
					context.buffer_size);
				if (is_output_constant) {
					result->extend_constant();
				} else {
					result->set_is_constant(false);
				}
			} else {
				allpass.process(input->get_data(), result->get_data(), context.buffer_size);
				result->set_is_constant(false);
			}

			input = *result;
		}
	}

	s_task_memory_query_result comb_feedback_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, delay),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays)) {
		s_task_memory_query_result result;
		bool fir_enabled = fir_coefficients->get_count() > 0;
		size_t sos_count = iir_sos_coefficients->get_count() / 5;
		size_t allpass_count = allpass_delays->get_count();

		c_stack_allocator::c_memory_calculator shared_calculator;
		if (fir_enabled) {
			c_fir_coefficients::calculate_memory(fir_coefficients->get_count(), shared_calculator);
		}
		wl_assert(shared_calculator.get_destructor_count() == 0);
		result.shared_size_alignment = shared_calculator.get_size_alignment();

		c_stack_allocator::c_memory_calculator voice_calculator;
		c_comb_feedback::calculate_memory(static_cast<uint32>(delay), voice_calculator);
		voice_calculator.add<s_comb_feedback_context>();
		if (fir_enabled) {
			c_fir::calculate_memory(fir_coefficients->get_count(), voice_calculator);
		}
		voice_calculator.add_array<s_iir_sos_state>(sos_count);
		voice_calculator.add_array<bool>(sos_count);
		voice_calculator.add_array<c_allpass>(allpass_count);
		for (size_t allpass_index = 0; allpass_index < allpass_count; allpass_index++) {
			c_allpass::calculate_memory(static_cast<uint32>((*allpass_delays)[allpass_index]), voice_calculator);
		}
		wl_assert(voice_calculator.get_destructor_count() == 0);
		result.voice_size_alignment = voice_calculator.get_size_alignment();

		c_stack_allocator::c_memory_calculator scratch_calculator;
		scratch_calculator.add_array<c_reentrant_iir_sos>(sos_count);
		wl_assert(scratch_calculator.get_destructor_count() == 0);
		result.scratch_size_alignment = scratch_calculator.get_size_alignment();

		return result;
	}

	void comb_feedback_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, fir_coefficients)) {
		if (fir_coefficients->get_count() > 0) {
			c_stack_allocator allocator(context.shared_memory);
			c_fir_coefficients *fir_coefficients_context;
			allocator.allocate(fir_coefficients_context);
			fir_coefficients_context->initialize(*fir_coefficients, allocator);

			allocator.release_no_destructors();
		}
	}

	void comb_feedback_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, delay),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays),
		wl_task_argument(c_real_constant_array, allpass_gains)) {
		c_stack_allocator allocator(context.voice_memory);

		s_comb_feedback_context *comb_feedback_context;
		allocator.allocate(comb_feedback_context);
		comb_feedback_context->fir_enabled = fir_coefficients->get_count() > 0;
		size_t sos_count = iir_sos_coefficients->get_count() / 5;
		size_t allpass_count = allpass_delays->get_count();

		comb_feedback_context->comb_feedback.initialize(static_cast<uint32>(*delay), allocator);

		if (comb_feedback_context->fir_enabled) {
			comb_feedback_context->fir.initialize(fir_coefficients->get_count(), allocator);
		}

		allocator.allocate_array(comb_feedback_context->iir_sos_context.states, sos_count);
		allocator.allocate_array(comb_feedback_context->iir_sos_context.is_first_order, sos_count);
		for (size_t sos_index = 0; sos_index < sos_count; sos_index++) {
			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b2 = (*iir_sos_coefficients)[coefficients_start_index + 2];
			const c_real_buffer *a2 = (*iir_sos_coefficients)[coefficients_start_index + 4];
			comb_feedback_context->iir_sos_context.is_first_order[sos_index] =
				b2->is_compile_time_constant()
				&& b2->get_constant() == 0.0f
				&& a2->is_compile_time_constant()
				&& a2->get_constant() == 0.0f;
		}

		allocator.allocate_array(comb_feedback_context->allpass_context.allpass_filters, allpass_count);
		for (size_t allpass_index = 0; allpass_index < allpass_count; allpass_index++) {
			comb_feedback_context->allpass_context.allpass_filters[allpass_index].initialize(
				static_cast<uint32>((*allpass_delays)[allpass_index]),
				(*allpass_gains)[allpass_index],
				allocator);
		}

		allocator.release_no_destructors();
	}

	void comb_feedback_voice_activator(
		const s_task_function_context &context) {
		s_comb_feedback_context *comb_feedback_context =
			reinterpret_cast<s_comb_feedback_context *>(context.voice_memory.get_pointer());

		comb_feedback_context->comb_feedback.reset();

		if (comb_feedback_context->fir_enabled) {
			comb_feedback_context->fir.reset();
		}

		for (s_iir_sos_state &state : comb_feedback_context->iir_sos_context.states) {
			state.reset();
		}

		for (c_allpass &allpass : comb_feedback_context->allpass_context.allpass_filters) {
			allpass.reset();
		}
	}

	// Note: this implementation excludes the dry input from the output signal. If it is desired it can be manually
	// added to the filter's output.
	void comb_feedback(
		const s_task_function_context &context,
		wl_task_argument(real32, delay),
		wl_task_argument(const c_real_buffer *, gain),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays),
		wl_task_argument(c_real_constant_array, allpass_gains),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument_unshared(c_real_buffer *, result)) { // We use this as a scratch buffer so it can't be shared
		s_comb_feedback_context *comb_feedback_context =
			reinterpret_cast<s_comb_feedback_context *>(context.voice_memory.get_pointer());

		const c_fir_coefficients *fir_coefficients_context = comb_feedback_context->fir_enabled
			? reinterpret_cast<const c_fir_coefficients *>(context.shared_memory.get_pointer())
			: nullptr;

		c_gain gain_context;
		gain_context.initialize(*gain);

		c_stack_allocator scratch_allocator(context.scratch_memory);
		c_wrapped_array<c_reentrant_iir_sos> sos_contexts;
		scratch_allocator.allocate_array(sos_contexts, comb_feedback_context->iir_sos_context.states.get_count());
		for (size_t sos_index = 0; sos_index < sos_contexts.get_count(); sos_index++) {
			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b0 = (*iir_sos_coefficients)[coefficients_start_index];
			const c_real_buffer *b1 = (*iir_sos_coefficients)[coefficients_start_index + 1];
			const c_real_buffer *a1 = (*iir_sos_coefficients)[coefficients_start_index + 3];

			if (comb_feedback_context->iir_sos_context.is_first_order[sos_index]) {
				sos_contexts[sos_index].initialize_first_order(b0, b1, a1);
			} else {
				const c_real_buffer *b2 = (*iir_sos_coefficients)[coefficients_start_index + 2];
				const c_real_buffer *a2 = (*iir_sos_coefficients)[coefficients_start_index + 4];
				sos_contexts[sos_index].initialize(b0, b1, b2, a1, a2);
			}
		}

		uint32 delay_samples = static_cast<uint32>(delay);
		for (size_t sample_index = 0; sample_index < context.buffer_size; sample_index += delay_samples) {
			size_t sample_count = std::min(static_cast<size_t>(delay_samples), context.buffer_size - sample_index);
			real32 *output = result->get_data() + sample_index;

			comb_feedback_context->comb_feedback.read_history(delay_samples, output, sample_count);

			// Apply optional filters in-place
			gain_context.process_in_place(output, sample_count);

			if (comb_feedback_context->fir_enabled) {
				comb_feedback_context->fir.process(*fir_coefficients_context, output, output, sample_count);
			}

			for (size_t sos_index = 0; sos_index < sos_contexts.get_count(); sos_index++) {
				s_iir_sos_state &sos_state = comb_feedback_context->iir_sos_context.states[sos_index];
				if (comb_feedback_context->iir_sos_context.is_first_order[sos_index]) {
					sos_contexts[sos_index].process_first_order(sos_state, output, output, sample_count);
				} else {
					sos_contexts[sos_index].process(sos_state, output, output, sample_count);
				}
			}

			for (c_allpass &allpass : comb_feedback_context->allpass_context.allpass_filters) {
				allpass.process(output, output, sample_count);
			}

			if (signal->is_constant()) {
				comb_feedback_context->comb_feedback.write_history_constant(
					signal->get_constant(),
					output,
					output,
					sample_count);
			} else {
				comb_feedback_context->comb_feedback.write_history(
					signal->get_data() + sample_index,
					output,
					output,
					sample_count);
			}
		}

		result->set_is_constant(false);
	}

	s_task_memory_query_result variable_comb_feedback_memory_query(
		const s_task_function_context &context,
		wl_task_argument(real32, max_delay),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays)) {
		s_task_memory_query_result result;
		bool fir_enabled = fir_coefficients->get_count() > 0;
		size_t sos_count = iir_sos_coefficients->get_count() / 5;
		size_t allpass_count = allpass_delays->get_count();

		c_stack_allocator::c_memory_calculator shared_calculator;
		if (fir_enabled) {
			c_fir_coefficients::calculate_memory(fir_coefficients->get_count(), shared_calculator);
		}
		wl_assert(shared_calculator.get_destructor_count() == 0);
		result.shared_size_alignment = shared_calculator.get_size_alignment();

		c_stack_allocator::c_memory_calculator voice_calculator;
		c_comb_feedback::calculate_memory_variable(max_delay, voice_calculator);
		voice_calculator.add<s_comb_feedback_context>();
		if (fir_enabled) {
			c_fir::calculate_memory(fir_coefficients->get_count(), voice_calculator);
		}
		voice_calculator.add_array<s_iir_sos_state>(sos_count);
		voice_calculator.add_array<bool>(sos_count);
		voice_calculator.add_array<c_allpass>(allpass_count);
		for (size_t allpass_index = 0; allpass_index < allpass_count; allpass_index++) {
			c_allpass::calculate_memory(static_cast<uint32>((*allpass_delays)[allpass_index]), voice_calculator);
		}
		wl_assert(voice_calculator.get_destructor_count() == 0);
		result.voice_size_alignment = voice_calculator.get_size_alignment();

		c_stack_allocator::c_memory_calculator scratch_calculator;
		scratch_calculator.add_array<c_reentrant_iir_sos>(sos_count);
		wl_assert(scratch_calculator.get_destructor_count() == 0);
		result.scratch_size_alignment = scratch_calculator.get_size_alignment();

		return result;
	}

	void variable_comb_feedback_initializer(
		const s_task_function_context &context,
		wl_task_argument(c_real_constant_array, fir_coefficients)) {
		if (fir_coefficients->get_count() > 0) {
			c_stack_allocator allocator(context.shared_memory);
			c_fir_coefficients *fir_coefficients_context;
			allocator.allocate(fir_coefficients_context);
			fir_coefficients_context->initialize(*fir_coefficients, allocator);

			allocator.release_no_destructors();
		}
	}

	void variable_comb_feedback_voice_initializer(
		const s_task_function_context &context,
		wl_task_argument(real32, min_delay),
		wl_task_argument(real32, max_delay),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays),
		wl_task_argument(c_real_constant_array, allpass_gains)) {
		c_stack_allocator allocator(context.voice_memory);

		s_comb_feedback_context *comb_feedback_context;
		allocator.allocate(comb_feedback_context);
		comb_feedback_context->fir_enabled = fir_coefficients->get_count() > 0;
		size_t sos_count = iir_sos_coefficients->get_count() / 5;
		size_t allpass_count = allpass_delays->get_count();

		comb_feedback_context->comb_feedback.initialize_variable(min_delay, max_delay, allocator);

		if (comb_feedback_context->fir_enabled) {
			comb_feedback_context->fir.initialize(fir_coefficients->get_count(), allocator);
		}

		allocator.allocate_array(comb_feedback_context->iir_sos_context.states, sos_count);
		allocator.allocate_array(comb_feedback_context->iir_sos_context.is_first_order, sos_count);
		for (size_t sos_index = 0; sos_index < sos_count; sos_index++) {
			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b2 = (*iir_sos_coefficients)[coefficients_start_index + 2];
			const c_real_buffer *a2 = (*iir_sos_coefficients)[coefficients_start_index + 4];
			comb_feedback_context->iir_sos_context.is_first_order[sos_index] =
				b2->is_compile_time_constant()
				&& b2->get_constant() == 0.0f
				&& a2->is_compile_time_constant()
				&& a2->get_constant() == 0.0f;
		}

		allocator.allocate_array(comb_feedback_context->allpass_context.allpass_filters, allpass_count);
		for (size_t allpass_index = 0; allpass_index < allpass_count; allpass_index++) {
			comb_feedback_context->allpass_context.allpass_filters[allpass_index].initialize(
				static_cast<uint32>((*allpass_delays)[allpass_index]),
				(*allpass_gains)[allpass_index],
				allocator);
		}

		allocator.release_no_destructors();
	}

	void variable_comb_feedback_voice_activator(
		const s_task_function_context &context) {
		s_comb_feedback_context *comb_feedback_context =
			reinterpret_cast<s_comb_feedback_context *>(context.voice_memory.get_pointer());

		comb_feedback_context->comb_feedback.reset();

		if (comb_feedback_context->fir_enabled) {
			comb_feedback_context->fir.reset();
		}

		for (s_iir_sos_state &state : comb_feedback_context->iir_sos_context.states) {
			state.reset();
		}

		for (c_allpass &allpass : comb_feedback_context->allpass_context.allpass_filters) {
			allpass.reset();
		}
	}

	// Note: this implementation excludes the dry input from the output signal. If it is desired it can be manually
	// added to the filter's output.
	void variable_comb_feedback(
		const s_task_function_context &context,
		wl_task_argument(real32, min_delay),
		wl_task_argument(real32, max_delay),
		wl_task_argument(const c_real_buffer *, gain),
		wl_task_argument(c_real_constant_array, fir_coefficients),
		wl_task_argument(c_real_buffer_array_in, iir_sos_coefficients),
		wl_task_argument(c_real_constant_array, allpass_delays),
		wl_task_argument(c_real_constant_array, allpass_gains),
		wl_task_argument(const c_real_buffer *, signal),
		wl_task_argument(const c_real_buffer *, delay),
		wl_task_argument_unshared(c_real_buffer *, result)) { // We use this as a scratch buffer so it can't be shared
		s_comb_feedback_context *comb_feedback_context =
			reinterpret_cast<s_comb_feedback_context *>(context.voice_memory.get_pointer());

		const c_fir_coefficients *fir_coefficients_context = comb_feedback_context->fir_enabled
			? reinterpret_cast<const c_fir_coefficients *>(context.shared_memory.get_pointer())
			: nullptr;

		c_gain gain_context;
		gain_context.initialize(*gain);

		c_stack_allocator scratch_allocator(context.scratch_memory);
		c_wrapped_array<c_reentrant_iir_sos> sos_contexts;
		scratch_allocator.allocate_array(sos_contexts, comb_feedback_context->iir_sos_context.states.get_count());
		for (size_t sos_index = 0; sos_index < sos_contexts.get_count(); sos_index++) {
			size_t coefficients_start_index = sos_index * 5;
			const c_real_buffer *b0 = (*iir_sos_coefficients)[coefficients_start_index];
			const c_real_buffer *b1 = (*iir_sos_coefficients)[coefficients_start_index + 1];
			const c_real_buffer *a1 = (*iir_sos_coefficients)[coefficients_start_index + 3];

			if (comb_feedback_context->iir_sos_context.is_first_order[sos_index]) {
				sos_contexts[sos_index].initialize_first_order(b0, b1, a1);
			} else {
				const c_real_buffer *b2 = (*iir_sos_coefficients)[coefficients_start_index + 2];
				const c_real_buffer *a2 = (*iir_sos_coefficients)[coefficients_start_index + 4];
				sos_contexts[sos_index].initialize(b0, b1, b2, a1, a2);
			}
		}

		uint32 min_delay_samples = static_cast<uint32>(min_delay);
		for (size_t sample_index = 0; sample_index < context.buffer_size; sample_index += min_delay_samples) {
			size_t sample_count = std::min(static_cast<size_t>(min_delay_samples), context.buffer_size - sample_index);
			real32 *output = result->get_data() + sample_index;

			if (delay->is_constant()) {
				comb_feedback_context->comb_feedback.read_history_variable_constant(
					delay->get_constant(),
					output,
					sample_count);
			} else {
				comb_feedback_context->comb_feedback.read_history_variable(
					delay->get_data() + sample_index,
					output,
					sample_count);
			}

			// Apply optional filters in-place
			gain_context.process_in_place(output, sample_count);

			if (comb_feedback_context->fir_enabled) {
				comb_feedback_context->fir.process(*fir_coefficients_context, output, output, sample_count);
			}

			for (size_t sos_index = 0; sos_index < sos_contexts.get_count(); sos_index++) {
				s_iir_sos_state &sos_state = comb_feedback_context->iir_sos_context.states[sos_index];
				if (comb_feedback_context->iir_sos_context.is_first_order[sos_index]) {
					sos_contexts[sos_index].process_first_order(sos_state, output, output, sample_count);
				} else {
					sos_contexts[sos_index].process(sos_state, output, output, sample_count);
				}
			}

			for (c_allpass &allpass : comb_feedback_context->allpass_context.allpass_filters) {
				allpass.process(output, output, sample_count);
			}

			if (signal->is_constant()) {
				comb_feedback_context->comb_feedback.write_history_constant(
					signal->get_constant(),
					output,
					output,
					sample_count);
			} else {
				comb_feedback_context->comb_feedback.write_history(
					signal->get_data() + sample_index,
					output,
					output,
					sample_count);
			}
		}

		result->set_is_constant(false);
	}

	void scrape_task_functions() {
		static constexpr uint32 k_filter_library_id = 5;
		wl_task_function_library(k_filter_library_id, "filter", 0);

		wl_task_function(0xf4544878, "fir")
			.set_function<fir>()
			.set_memory_query<fir_memory_query>()
			.set_initializer<fir_initializer>()
			.set_voice_initializer<fir_voice_initializer>()
			.set_voice_activator<fir_voice_activator>();

		wl_task_function(0xb85daa29, "iir_sos")
			.set_function<iir_sos>()
			.set_memory_query<iir_sos_memory_query>()
			.set_voice_initializer<iir_sos_voice_initializer>()
			.set_voice_activator<iir_sos_voice_activator>();

		wl_task_function(0x24a5a6e8, "allpass")
			.set_function<allpass>()
			.set_memory_query<allpass_memory_query>()
			.set_voice_initializer<allpass_voice_initializer>()
			.set_voice_activator<allpass_voice_activator>();

		wl_task_function(0xc5bfef19, "comb_feedback")
			.set_function<comb_feedback>()
			.set_memory_query<comb_feedback_memory_query>()
			.set_initializer<comb_feedback_initializer>()
			.set_voice_initializer<comb_feedback_voice_initializer>()
			.set_voice_activator<comb_feedback_voice_activator>();

		wl_task_function(0x6e9bb3be, "variable_comb_feedback")
			.set_function<variable_comb_feedback>()
			.set_memory_query<variable_comb_feedback_memory_query>()
			.set_initializer<variable_comb_feedback_initializer>()
			.set_voice_initializer<variable_comb_feedback_voice_initializer>()
			.set_voice_activator<variable_comb_feedback_voice_activator>();

		wl_end_active_library_task_function_registration();
	}

}
