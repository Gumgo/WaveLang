#ifndef WAVELANG_BUFFER_OPERATIONS_H__
#define WAVELANG_BUFFER_OPERATIONS_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/math/math.h"

#if PREDEFINED(ASSERTS_ENABLED)
void validate_buffer(const c_buffer *buffer);
#else // PREDEFINED(ASSERTS_ENABLED)
#define validate_buffer(buffer)
#endif // PREDEFINED(ASSERTS_ENABLED)

// Shorthand:
typedef c_buffer *c_buffer_out;
typedef c_buffer *c_buffer_inout;
typedef const c_buffer *c_buffer_in;

// These helper templates are used to perform pure functions only, with no memory or side effects. i.e. making a call
// with the same set of inputs should always produce the same outputs.

// Note that buffer operation count will be rounded up to the block size, so they can write past buffer_size. Make sure
// that buffers are allocated as a multiple of buffer_size, even if the provided size is not.

template<typename t_operation>
void buffer_operator_out(const t_operation &op, size_t buffer_size,
	c_buffer_out out) {
	validate_buffer(out);

	// Reminder: t_operation MUST be a pure function with no side effects or memory!
	real32 *out_ptr = out->get_data<real32>();
	c_real32_4 out_val = op();
	out_val.store(out_ptr);
	out->set_constant(true);
}


template<typename t_operation>
void buffer_operator_inout(const t_operation &op, size_t buffer_size,
	c_buffer_inout inout) {
	validate_buffer(inout);

	if (inout->is_constant()) {
		real32 *inout_ptr = inout->get_data<real32>();
		c_real32_4 in_val(inout_ptr);
		c_real32_4 out_val = op(in_val);
		out_val.store(inout_ptr);
	} else {
		real32 *inout_ptr = inout->get_data<real32>();
		real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
		for (; inout_ptr < inout_ptr_end;
			 inout_ptr += k_sse_block_elements) {
			c_real32_4 in_val(inout_ptr);
			c_real32_4 out_val = op(in_val);
			out_val.store(inout_ptr);
		}
	}
}

template<typename t_operation>
void buffer_operator_out_in(const t_operation &op, size_t buffer_size,
	c_buffer_out out, c_buffer_in in) {
	validate_buffer(out);
	validate_buffer(in);

	if (in->is_constant()) {
		real32 *out_ptr = out->get_data<real32>();
		const real32 *in_ptr = in->get_data<real32>();
		c_real32_4 in_val(in_ptr);
		c_real32_4 out_val = op(in_val);
		out_val.store(out_ptr);
		out->set_constant(true);
	} else {
		real32 *out_ptr = out->get_data<real32>();
		real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
		const real32 *in_ptr = in->get_data<real32>();
		for (; out_ptr < out_ptr_end;
			 out_ptr += k_sse_block_elements, in_ptr += k_sse_block_elements) {
			c_real32_4 in_val(in_ptr);
			c_real32_4 out_val = op(in_val);
			out_val.store(out_ptr);
		}
		out->set_constant(false);
	}
}

template<typename t_operation>
void buffer_operator_out_in(const t_operation &op, size_t buffer_size,
	c_buffer_out out, const real32 in) {
	validate_buffer(out);

	real32 *out_ptr = out->get_data<real32>();
	c_real32_4 in_val(in);
	c_real32_4 out_val = op(in_val);
	out_val.store(out_ptr);
	out->set_constant(true);
}

template<typename t_operation>
void buffer_operator_out_in_in(const t_operation &op, size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
	validate_buffer(out);
	validate_buffer(in_a);
	validate_buffer(in_b);

	if (in_a->is_constant()) {
		if (in_b->is_constant()) {
			real32 *out_ptr = out->get_data<real32>();
			const real32 *in_a_ptr = in_a->get_data<real32>();
			const real32 *in_b_ptr = in_b->get_data<real32>();
			c_real32_4 in_a_val(in_a_ptr);
			c_real32_4 in_b_val(in_b_ptr);
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(out_ptr);
			out->set_constant(true);
		} else {
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_a_ptr = in_a->get_data<real32>();
			const real32 *in_b_ptr = in_b->get_data<real32>();
			c_real32_4 in_a_val(in_a_ptr);
			for (; out_ptr < out_ptr_end;
				 out_ptr += k_sse_block_elements, in_b_ptr += k_sse_block_elements) {
				c_real32_4 in_b_val(in_b_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(out_ptr);
			}
			out->set_constant(false);
		}
	} else {
		if (in_b->is_constant()) {
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_a_ptr = in_a->get_data<real32>();
			const real32 *in_b_ptr = in_b->get_data<real32>();
			c_real32_4 in_b_val(in_b_ptr);
			for (; out_ptr < out_ptr_end;
				 out_ptr += k_sse_block_elements, in_a_ptr += k_sse_block_elements) {
				c_real32_4 in_a_val(in_a_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(out_ptr);
			}
			out->set_constant(false);
		} else {
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_a_ptr = in_a->get_data<real32>();
			const real32 *in_b_ptr = in_b->get_data<real32>();
			for (; out_ptr < out_ptr_end;
				 out_ptr += k_sse_block_elements, in_a_ptr += k_sse_block_elements, in_b_ptr += k_sse_block_elements) {
				c_real32_4 in_a_val(in_a_ptr);
				c_real32_4 in_b_val(in_b_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(out_ptr);
			}
			out->set_constant(false);
		}
	}
}

template<typename t_operation>
void buffer_operator_out_in_in(const t_operation &op, size_t buffer_size,
	c_buffer_out out, c_buffer_in in_a, const real32 in_b) {
	validate_buffer(out);
	validate_buffer(in_a);

	if (in_a->is_constant()) {
		real32 *out_ptr = out->get_data<real32>();
		const real32 *in_a_ptr = in_a->get_data<real32>();
		c_real32_4 in_a_val(in_a_ptr);
		c_real32_4 in_b_val(in_b);
		c_real32_4 out_val = op(in_a_val, in_b_val);
		out_val.store(out_ptr);
		out->set_constant(true);
	} else {
		real32 *out_ptr = out->get_data<real32>();
		real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
		const real32 *in_a_ptr = in_a->get_data<real32>();
		c_real32_4 in_b_val(in_b);
		for (; out_ptr < out_ptr_end;
			 out_ptr += k_sse_block_elements, in_a_ptr += k_sse_block_elements) {
			c_real32_4 in_a_val(in_a_ptr);
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(out_ptr);
		}
		out->set_constant(false);
	}
}

template<typename t_operation>
void buffer_operator_inout_in(const t_operation &op, size_t buffer_size,
	c_buffer_inout inout, c_buffer_in in) {
	validate_buffer(inout);
	validate_buffer(in);

	if (inout->is_constant()) {
		if (in->is_constant()) {
			real32 *inout_ptr = inout->get_data<real32>();
			const real32 *in_ptr = in->get_data<real32>();
			c_real32_4 in_a_val(inout_ptr);
			c_real32_4 in_b_val(in_ptr);
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(inout_ptr);
		} else {
			real32 *inout_ptr = inout->get_data<real32>();
			real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_ptr = in->get_data<real32>();
			c_real32_4 in_a_val(inout_ptr);
			for (; inout_ptr < inout_ptr_end;
				 inout_ptr += k_sse_block_elements, in_ptr += k_sse_block_elements) {
				c_real32_4 in_b_val(in_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(inout_ptr);
			}
			inout->set_constant(false);
		}
	} else {
		if (in->is_constant()) {
			real32 *inout_ptr = inout->get_data<real32>();
			real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_ptr = in->get_data<real32>();
			c_real32_4 in_b_val(in_ptr);
			for (; inout_ptr < inout_ptr_end;
				 inout_ptr += k_sse_block_elements) {
				c_real32_4 in_a_val(inout_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(inout_ptr);
			}
			inout->set_constant(false);
		} else {
			real32 *inout_ptr = inout->get_data<real32>();
			real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
			const real32 *in_ptr = in->get_data<real32>();
			for (; inout_ptr < inout_ptr_end;
				 inout_ptr += k_sse_block_elements, in_ptr += k_sse_block_elements) {
				c_real32_4 in_a_val(inout_ptr);
				c_real32_4 in_b_val(in_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(inout_ptr);
			}
			inout->set_constant(false);
		}
	}
}

template<typename t_operation>
void buffer_operator_inout_in(const t_operation &op, size_t buffer_size,
	c_buffer_inout inout, const real32 in) {
	validate_buffer(inout);

	if (inout->is_constant()) {
		real32 *inout_ptr = inout->get_data<real32>();
		c_real32_4 in_a_val(inout_ptr);
		c_real32_4 in_b_val(in);
		c_real32_4 out_val = op(in_a_val, in_b_val);
		out_val.store(inout_ptr);
	} else {
		real32 *inout_ptr = inout->get_data<real32>();
		real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
		c_real32_4 in_b_val(in);
		for (; inout_ptr < inout_ptr_end;
			 inout_ptr += k_sse_block_elements) {
			c_real32_4 in_a_val(inout_ptr);
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(inout_ptr);
		}
		inout->set_constant(false);
	}
}

template<typename t_operation>
struct s_buffer_operation_1_input {
	static void buffer(size_t buffer_size, c_buffer_out out, c_buffer_in in) {
		buffer_operator_out_in(t_operation(), buffer_size, out, in);
	}

	static void bufferio(size_t buffer_size, c_buffer_inout inout) {
		buffer_operator_inout(t_operation(), buffer_size, inout);
	}

	static void constant(size_t buffer_size, c_buffer_out out, real32 in) {
		buffer_operator_out_in(t_operation(), buffer_size, out, in);
	}
};

template<typename t_operation, typename t_operation_reverse = t_operation>
struct s_buffer_operation_2_inputs {
	static void buffer_buffer(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
		buffer_operator_out_in_in(t_operation(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_buffer(size_t buffer_size, c_buffer_inout inout, c_buffer_in in) {
		buffer_operator_inout_in(t_operation(), buffer_size, inout, in);
	}

	static void buffer_bufferio(size_t buffer_size, c_buffer_in in, c_buffer_inout inout) {
		buffer_operator_inout_in(t_operation_reverse(), buffer_size, inout, in);
	}

	static void buffer_constant(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, real32 in_b) {
		buffer_operator_out_in_in(t_operation(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_constant(size_t buffer_size, c_buffer_inout inout, real32 in) {
		buffer_operator_inout_in(t_operation(), buffer_size, inout, in);
	}

	static void constant_buffer(size_t buffer_size, c_buffer_out out, real32 in_a, c_buffer_in in_b) {
		buffer_operator_out_in_in(t_operation_reverse(), buffer_size, out, in_b, in_a);
	}

	static void constant_bufferio(size_t buffer_size, real32 in, c_buffer_inout inout) {
		buffer_operator_inout_in(t_operation_reverse(), buffer_size, inout, in);
	}
};

#endif // WAVELANG_BUFFER_OPERATIONS_H__