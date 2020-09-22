#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/task_functions/task_functions_filter.h"

namespace filter_task_functions {

	struct s_biquad_context {
		// See http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt for algorithm details
		real32 x1, x2;
		real32 y1, y2;
	};

	size_t biquad_memory_query(const s_task_function_context &context) {
		return sizeof(s_biquad_context);
	}

	void biquad_voice_initializer(const s_task_function_context &context) {
		zero_type(static_cast<s_biquad_context *>(context.task_memory));
	}

	void biquad(
		const s_task_function_context &context,
		const c_real_buffer *a1,
		const c_real_buffer *a2,
		const c_real_buffer *b0,
		const c_real_buffer *b1,
		const c_real_buffer *b2,
		const c_real_buffer *signal,
		c_real_buffer *result) {
		s_biquad_context *biquad_context = static_cast<s_biquad_context *>(context.task_memory);
		real32 x1 = biquad_context->x1;
		real32 x2 = biquad_context->x2;
		real32 y1 = biquad_context->y1;
		real32 y2 = biquad_context->y2;
		iterate_buffers<1, false>(context.buffer_size, b0, b1, b2, a1, a2, signal, result,
			[&x1, &x2, &y1, &y2](
				size_t i,
				real32 b0,
				real32 b1,
				real32 b2,
				real32 a1,
				real32 a2,
				real32 signal,
				real32 &result) {
				// $TODO optimize this with SIMD
				real32 y0 = b0 * signal + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

				// Shift the accumulator
				x2 = x1;
				x1 = signal;
				y2 = y1;
				y1 = y0;

				result = y0;
			});
	}

}
