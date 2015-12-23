#ifndef WAVELANG_BUFFER_OPERATIONS_H__
#define WAVELANG_BUFFER_OPERATIONS_H__

#include "common/common.h"
#include "engine/buffer.h"
#include "engine/math/math.h"

// Shorthand:
typedef c_buffer *c_real_buffer_out;
typedef c_buffer *c_real_buffer_inout;
typedef const c_buffer *c_real_buffer_in;
typedef c_real_const_buffer_or_constant c_real_buffer_or_constant_in;

class c_buffer_iterator_real {
public:
	c_buffer_iterator_real(size_t buffer_size, real32 *pointer) {
		m_pointer = pointer;
		m_pointer_end = m_pointer + align_size(buffer_size, k_sse_block_elements);
	}

	bool is_valid() const {
		return m_pointer < m_pointer_end;
	}

	void next() {
		m_pointer += k_sse_block_elements;
	}

	real32 *get_pointer() const {
		return m_pointer;
	}

private:
	real32 *m_pointer;
	real32 *m_pointer_end;
};

class c_buffer_iterator_real_real {
public:
	c_buffer_iterator_real_real(size_t buffer_size, real32 *pointer_a, const real32 *pointer_b) {
		m_pointer_a = pointer_a;
		m_pointer_a_end = m_pointer_a + align_size(buffer_size, k_sse_block_elements);
		m_pointer_b = pointer_b;
	}

	bool is_valid() const {
		return m_pointer_a < m_pointer_a_end;
	}

	void next() {
		m_pointer_a += k_sse_block_elements;
	}

	real32 *get_pointer_a() const {
		return m_pointer_a;
	}

	const real32 *get_pointer_b() const {
		return m_pointer_b;
	}

private:
	real32 *m_pointer_a;
	real32 *m_pointer_a_end;
	const real32 *m_pointer_b;
};

class c_buffer_iterator_real_real_real {
public:
	c_buffer_iterator_real_real_real(
		size_t buffer_size, real32 *pointer_a, const real32 *pointer_b, const real32 *pointer_c) {
		m_pointer_a = pointer_a;
		m_pointer_a_end = m_pointer_a + align_size(buffer_size, k_sse_block_elements);
		m_pointer_b = pointer_b;
		m_pointer_c = pointer_c;
	}

	bool is_valid() const {
		return m_pointer_a < m_pointer_a_end;
	}

	void next() {
		m_pointer_a += k_sse_block_elements;
	}

	real32 *get_pointer_a() const {
		return m_pointer_a;
	}

	const real32 *get_pointer_b() const {
		return m_pointer_b;
	}

	const real32 *get_pointer_c() const {
		return m_pointer_c;
	}

private:
	real32 *m_pointer_a;
	real32 *m_pointer_a_end;
	const real32 *m_pointer_b;
	const real32 *m_pointer_c;
};

#if PREDEFINED(ASSERTS_ENABLED)
void validate_buffer(const c_buffer *buffer);
void validate_buffer(const c_real_buffer_or_constant_in &buffer_or_constant);
#else // PREDEFINED(ASSERTS_ENABLED)
#define validate_buffer(buffer)
#endif // PREDEFINED(ASSERTS_ENABLED)

// These helper templates are used to perform pure functions only, with no memory or side effects. i.e. making a call
// with the same set of inputs should always produce the same outputs.

// Note that buffer operation count will be rounded up to the block size, so they can write past buffer_size. Make sure
// that buffers are allocated as a multiple of buffer_size, even if the provided size is not.

template<typename t_operation>
void buffer_operator_real_out(const t_operation &op, size_t buffer_size,
	c_real_buffer_out out) {
	validate_buffer(out);

	// Reminder: t_operation MUST be a pure function with no side effects or memory!
	real32 *out_ptr = out->get_data<real32>();
	c_real32_4 out_val = op();
	out_val.store(out_ptr);
	out->set_constant(true);
}

template<typename t_operation>
void buffer_operator_real_inout(const t_operation &op, size_t buffer_size,
	c_real_buffer_inout inout) {
	validate_buffer(inout);

	if (inout->is_constant()) {
		real32 *inout_ptr = inout->get_data<real32>();
		c_real32_4 in_val(inout_ptr);
		c_real32_4 out_val = op(in_val);
		out_val.store(inout_ptr);
	} else {
		for (c_buffer_iterator_real it(buffer_size, inout->get_data<real32>()); it.is_valid(); it.next()) {
			real32 *inout_ptr = it.get_pointer();
			c_real32_4 in_val(inout_ptr);
			c_real32_4 out_val = op(in_val);
			out_val.store(inout_ptr);
		}
	}
}

template<typename t_operation>
void buffer_operator_real_in_real_out(const t_operation &op, size_t buffer_size,
	c_real_buffer_or_constant_in in,
	c_real_buffer_out out) {
	validate_buffer(in);
	validate_buffer(out);

	if (in.is_constant()) {
		c_real32_4 in_val(in.get_constant());
		c_real32_4 out_val = op(in_val);
		real32 *out_ptr = out->get_data<real32>();
		out_val.store(out_ptr);
		out->set_constant(true);
	} else {
		const real32 *in_ptr = in.get_buffer()->get_data<real32>();
		real32 *out_ptr = out->get_data<real32>();
		real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
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
void buffer_operator_real_in_real_in_real_out(const t_operation &op, size_t buffer_size,
	c_real_buffer_or_constant_in in_a,
	c_real_buffer_or_constant_in in_b,
	c_real_buffer_out out) {
	validate_buffer(in_a);
	validate_buffer(in_b);
	validate_buffer(out);

	if (in_a.is_constant()) {
		c_real32_4 in_a_val(in_a.get_constant());

		if (in_b.is_constant()) {
			c_real32_4 in_b_val(in_b.get_constant());
			real32 *out_ptr = out->get_data<real32>();
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(out_ptr);
			out->set_constant(true);
		} else {
			const real32 *in_b_ptr = in_b.get_buffer()->get_data<real32>();
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			for (; out_ptr < out_ptr_end;
				 out_ptr += k_sse_block_elements, in_b_ptr += k_sse_block_elements) {
				c_real32_4 in_b_val(in_b_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(out_ptr);
			}
			out->set_constant(false);
		}
	} else {
		const real32 *in_a_ptr = in_a.get_buffer()->get_data<real32>();

		if (in_b.is_constant()) {
			c_real32_4 in_b_val(in_b.get_constant());
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
			for (; out_ptr < out_ptr_end;
				 out_ptr += k_sse_block_elements, in_a_ptr += k_sse_block_elements) {
				c_real32_4 in_a_val(in_a_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(out_ptr);
			}
			out->set_constant(false);
		} else {
			const real32 *in_b_ptr = in_b.get_buffer()->get_data<real32>();
			real32 *out_ptr = out->get_data<real32>();
			real32 *out_ptr_end = out_ptr + align_size(buffer_size, k_sse_block_elements);
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
void buffer_operator_real_inout_real_in(const t_operation &op, size_t buffer_size,
	c_real_buffer_inout inout,
	c_real_buffer_or_constant_in in) {
	validate_buffer(inout);
	validate_buffer(in);

	if (inout->is_constant()) {
		c_real32_4 in_a_val(*inout->get_data<real32>());

		if (in.is_constant()) {
			c_real32_4 in_b_val(in.get_constant());
			real32 *inout_ptr = inout->get_data<real32>();
			c_real32_4 out_val = op(in_a_val, in_b_val);
			out_val.store(inout_ptr);
		} else {
			const real32 *in_ptr = in.get_buffer()->get_data<real32>();
			real32 *inout_ptr = inout->get_data<real32>();
			real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
			for (; inout_ptr < inout_ptr_end;
				 inout_ptr += k_sse_block_elements, in_ptr += k_sse_block_elements) {
				c_real32_4 in_b_val(in_ptr);
				c_real32_4 out_val = op(in_a_val, in_b_val);
				out_val.store(inout_ptr);
			}
			inout->set_constant(false);
		}
	} else {
		if (in.is_constant()) {
			c_real32_4 in_b_val(in.get_constant());
			real32 *inout_ptr = inout->get_data<real32>();
			real32 *inout_ptr_end = inout_ptr + align_size(buffer_size, k_sse_block_elements);
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
			const real32 *in_ptr = in.get_buffer()->get_data<real32>();
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
struct s_buffer_operation_real {
	static void in_out(size_t buffer_size, c_real_buffer_or_constant_in in, c_real_buffer_out out) {
		buffer_operator_real_in_real_out(t_operation(), buffer_size, in, out);
	}

	static void inout(size_t buffer_size, c_real_buffer_inout inout) {
		buffer_operator_real_inout(t_operation(), buffer_size, inout);
	}
};

template<typename t_operation, typename t_operation_reverse = t_operation>
struct s_buffer_operation_real_real {
	static void in_in_out(size_t buffer_size, c_real_buffer_or_constant_in in_a, c_real_buffer_or_constant_in in_b,
		c_real_buffer_out out) {
		buffer_operator_real_in_real_in_real_out(t_operation(), buffer_size, in_a, in_b, out);
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout inout_a, c_real_buffer_or_constant_in in_b) {
		buffer_operator_real_inout_real_in(t_operation(), buffer_size, inout_a, in_b);
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in in_a, c_real_buffer_inout inout_b) {
		buffer_operator_real_inout_real_in(t_operation_reverse(), buffer_size, inout_b, in_a);
	}
};

#endif // WAVELANG_BUFFER_OPERATIONS_H__