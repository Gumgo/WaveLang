#pragma once

#include "common/common.h"

#include "engine/buffer.h"

// This implementation uses transposed direct-form 2:
// y = b0*x + s1
// s1 = b1*x - a1*y + s2
// s2 = b2*x - a2*y
// Note: using direct-form 1 may have subtle differences if animated coefficients are used because the history state
// pipes the animated coefficients through in a slightly different order, but it should not make any significant
// difference.
struct s_iir_sos_state {
	real64 s1;
	real64 s2;

	void reset() {
		s1 = 0.0;
		s2 = 0.0;
	}

	real32 process_single_sample(real32 b0, real32 b1, real32 b2, real32 a1, real32 a2, real32 x) {
		real64 x_real64 = static_cast<real64>(x);
		real64 y = b0 * x_real64 + s1;
		s1 = b1 * x_real64 - a1 * y + s2;
		s2 = b2 * x_real64 - a2 * y;
		return static_cast<real32>(y);
	}

	// Don't: don't intermix this with process_single_sample() because the history won't be correct. Only use this if
	// b2 and a2 are compile-time constant zero.
	real32 process_first_order_single_sample(real32 b0, real32 b1, real32 a1, real32 x) {
		real64 x_real64 = static_cast<real64>(x);
		real64 y = b0 * x_real64 + s1;
		s1 = b1 * x_real64 - a1 * y;
		return static_cast<real32>(y);
	}
};

// This class is used to process an SOS in a re-entrant manner. It properly handles a mix of animated and non-animated
// coefficients.
class c_reentrant_iir_sos {
public:
	c_reentrant_iir_sos() = default;

	void initialize(
		const c_real_buffer *b0,
		const c_real_buffer *b1,
		const c_real_buffer *b2,
		const c_real_buffer *a1,
		const c_real_buffer *a2);
	void initialize_first_order(
		const c_real_buffer *b0,
		const c_real_buffer *b1,
		const c_real_buffer *a1);

	void process(s_iir_sos_state &state, const real32 *input, real32 *output, size_t sample_count);
	void process_first_order(s_iir_sos_state &state, const real32 *input, real32 *output, size_t sample_count);

private:
	struct s_buffer_state {
		const real32 *b0;
		const real32 *b1;
		const real32 *b2;
		const real32 *a1;
		const real32 *a2;
		uint8 b0_increment;
		uint8 b1_increment;
		uint8 b2_increment;
		uint8 a1_increment;
		uint8 a2_increment;
		bool all_constant;
	};

	s_buffer_state m_buffer_state;
};
