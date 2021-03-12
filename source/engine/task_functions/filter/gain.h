#pragma once

#include "common/common.h"

#include "engine/buffer.h"

class c_gain {
public:
	c_gain() = default;

	void initialize(const c_real_buffer *gain);

	void process_in_place(real32 *input_output, size_t sample_count);

private:
	const real32 *m_gain;
	bool m_is_constant;
};
