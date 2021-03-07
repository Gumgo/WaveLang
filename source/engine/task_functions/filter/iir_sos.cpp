#include "engine/task_functions/filter/iir_sos.h"

void c_reentrant_iir_sos::initialize(
	const c_real_buffer *b0,
	const c_real_buffer *b1,
	const c_real_buffer *b2,
	const c_real_buffer *a1,
	const c_real_buffer *a2) {
	// For each buffer, increment is 0 if it is constant (the pointer is incremented by 0) and 1 if it is variable
	m_buffer_state.b0 = b0->get_data();
	m_buffer_state.b1 = b1->get_data();
	m_buffer_state.b2 = b2->get_data();
	m_buffer_state.a1 = a1->get_data();
	m_buffer_state.a2 = a2->get_data();

	m_buffer_state.all_constant = true;
	m_buffer_state.b0_increment = 1 - static_cast<uint8>(b0->is_constant());
	m_buffer_state.all_constant &= b0->is_constant();
	m_buffer_state.b1_increment = 1 - static_cast<uint8>(b1->is_constant());
	m_buffer_state.all_constant &= b1->is_constant();
	m_buffer_state.b2_increment = 1 - static_cast<uint8>(b2->is_constant());
	m_buffer_state.all_constant &= b2->is_constant();
	m_buffer_state.a1_increment = 1 - static_cast<uint8>(a1->is_constant());
	m_buffer_state.all_constant &= a1->is_constant();
	m_buffer_state.a2_increment = 1 - static_cast<uint8>(a2->is_constant());
	m_buffer_state.all_constant &= a2->is_constant();
}

void c_reentrant_iir_sos::initialize_first_order(
	const c_real_buffer *b0,
	const c_real_buffer *b1,
	const c_real_buffer *a1) {
	// For each buffer, increment is 0 if it is constant (the pointer is incremented by 0) and 1 if it is variable
	m_buffer_state.b0 = b0->get_data();
	m_buffer_state.b1 = b1->get_data();
	m_buffer_state.b2 = nullptr;
	m_buffer_state.a1 = a1->get_data();
	m_buffer_state.a2 = nullptr;

	m_buffer_state.all_constant = true;
	m_buffer_state.b0_increment = 1 - static_cast<uint8>(b0->is_constant());
	m_buffer_state.all_constant &= b0->is_constant();
	m_buffer_state.b1_increment = 1 - static_cast<uint8>(b1->is_constant());
	m_buffer_state.all_constant &= b1->is_constant();
	m_buffer_state.b2_increment = 0;
	m_buffer_state.a1_increment = 1 - static_cast<uint8>(a1->is_constant());
	m_buffer_state.all_constant &= a1->is_constant();
	m_buffer_state.a2_increment = 0;
}

void c_reentrant_iir_sos::process(s_iir_sos_state &state, const real32 *input, real32 *output, size_t sample_count) {
	// Make sure this isn't a first-order IIR
	wl_assert(m_buffer_state.b2);
	wl_assert(m_buffer_state.a2);

	// Make copies of state for cache locality
	s_iir_sos_state local_state = state;

	if (m_buffer_state.all_constant) {
		real32 b0 = *m_buffer_state.b0;
		real32 b1 = *m_buffer_state.b1;
		real32 b2 = *m_buffer_state.b2;
		real32 a1 = *m_buffer_state.a1;
		real32 a2 = *m_buffer_state.a2;

		for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
			output[sample_index] = local_state.process_single_sample(b0, b1, b2, a1, a2, input[sample_index]);
		}
	} else {
		s_buffer_state local_buffer_state = m_buffer_state;

		for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
			output[sample_index] = local_state.process_single_sample(
				*local_buffer_state.b0,
				*local_buffer_state.b1,
				*local_buffer_state.b2,
				*local_buffer_state.a1,
				*local_buffer_state.a2,
				input[sample_index]);
			local_buffer_state.b0 += local_buffer_state.b0_increment;
			local_buffer_state.b1 += local_buffer_state.b1_increment;
			local_buffer_state.b2 += local_buffer_state.b2_increment;
			local_buffer_state.a1 += local_buffer_state.a1_increment;
			local_buffer_state.a2 += local_buffer_state.a2_increment;
		}

		m_buffer_state = local_buffer_state;
	}

	// Store off the updated state
	state = local_state;
}

void c_reentrant_iir_sos::process_first_order(
	s_iir_sos_state &state,
	const real32 *input,
	real32 *output,
	size_t sample_count) {
	// Make sure this is a first-order IIR
	wl_assert(!m_buffer_state.b2);
	wl_assert(!m_buffer_state.a2);

	// Make copies of state for cache locality
	s_iir_sos_state local_state = state;

	if (m_buffer_state.all_constant) {
		real32 b0 = *m_buffer_state.b0;
		real32 b1 = *m_buffer_state.b1;
		real32 a1 = *m_buffer_state.a1;

		for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
			output[sample_index] = local_state.process_first_order_single_sample(b0, b1, a1, input[sample_index]);
		}
	} else {
		s_buffer_state local_buffer_state = m_buffer_state;

		for (size_t sample_index = 0; sample_index < sample_count; sample_index++) {
			output[sample_index] = local_state.process_first_order_single_sample(
				*local_buffer_state.b0,
				*local_buffer_state.b1,
				*local_buffer_state.a1,
				input[sample_index]);
			local_buffer_state.b0 += local_buffer_state.b0_increment;
			local_buffer_state.b1 += local_buffer_state.b1_increment;
			local_buffer_state.a1 += local_buffer_state.a1_increment;
		}

		m_buffer_state = local_buffer_state;
	}

	// Store off the updated state
	state = local_state;
}
