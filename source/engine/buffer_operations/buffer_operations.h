#ifndef WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_H__
#define WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_H__

#include "common/common.h"

#include "engine/buffer.h"
#include "engine/task_function.h"
#include "engine/buffer_operations/buffer_iterators.h"
#include "engine/math/math.h"

typedef c_buffer *c_real_buffer_out;
typedef c_buffer *c_real_buffer_inout;
typedef const c_buffer *c_real_buffer_in;
typedef c_real_const_buffer_or_constant c_real_buffer_or_constant_in;

typedef c_buffer *c_bool_buffer_out;
typedef c_buffer *c_bool_buffer_inout;
typedef const c_buffer *c_bool_buffer_in;
typedef c_bool_const_buffer_or_constant c_bool_buffer_or_constant_in;

// Iterable buffer wrappers

class c_iterable_buffer_real_in {
public:
	typedef c_const_real_buffer_iterator t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_in;

	c_iterable_buffer_real_in(c_real_const_buffer_or_constant buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer.is_constant(); }
	const c_buffer *get_buffer() const { wl_assert(!is_constant()); return m_buffer.get_buffer(); }

	c_real32_4 get_constant() const {
		wl_assert(is_constant());
		return c_real32_4(m_buffer.get_constant());
	}

private:
	c_real_const_buffer_or_constant m_buffer;
};

class c_iterable_buffer_real_out {
public:
	typedef c_real_buffer_iterator t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_out;

	c_iterable_buffer_real_out(c_buffer *buffer) : m_buffer(buffer) {}
	c_buffer *get_buffer() const { return m_buffer; }

	void set_constant(const c_real32_4 &value) {
		value.store(m_buffer->get_data<real32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

class c_iterable_buffer_real_inout {
public:
	typedef c_real_buffer_iterator t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_inout;

	c_iterable_buffer_real_inout(c_buffer *buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer->is_constant(); }
	c_buffer *get_buffer() const { return m_buffer; }

	c_real32_4 get_constant() const {
		wl_assert(m_buffer->is_constant());
		return c_real32_4(*m_buffer->get_data<real32>());
	}

	void set_constant(const c_real32_4 &value) {
		value.store(m_buffer->get_data<real32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

class c_iterable_buffer_bool_4_in {
public:
	typedef c_const_bool_buffer_iterator_4 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_in;

	c_iterable_buffer_bool_4_in(c_bool_const_buffer_or_constant buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer.is_constant(); }
	const c_buffer *get_buffer() const { wl_assert(!is_constant()); return m_buffer.get_buffer(); }

	c_int32_4 get_constant() const {
		wl_assert(is_constant());
		return c_int32_4(-static_cast<int32>(m_buffer.get_constant()));
	}

private:
	c_bool_const_buffer_or_constant m_buffer;
};

class c_iterable_buffer_bool_4_out {
public:
	typedef c_bool_buffer_iterator_4 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_out;

	c_iterable_buffer_bool_4_out(c_buffer *buffer) : m_buffer(buffer) {}
	c_buffer *get_buffer() const { return m_buffer; }

	void set_constant(const c_int32_4 &value) {
		value.store(m_buffer->get_data<int32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

class c_iterable_buffer_bool_4_inout {
public:
	typedef c_bool_buffer_iterator_4 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_inout;

	c_iterable_buffer_bool_4_inout(c_buffer *buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer->is_constant(); }
	c_buffer *get_buffer() const { return m_buffer; }

	c_int32_4 get_constant() const {
		wl_assert(m_buffer->is_constant());
		return c_int32_4(-(*m_buffer->get_data<int32>() & 1));
	}

	void set_constant(const c_int32_4 &value) {
		value.store(m_buffer->get_data<int32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

class c_iterable_buffer_bool_128_in {
public:
	typedef c_const_bool_buffer_iterator_128 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_in;

	c_iterable_buffer_bool_128_in(c_bool_const_buffer_or_constant buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer.is_constant(); }
	const c_buffer *get_buffer() const { wl_assert(!is_constant()); return m_buffer.get_buffer(); }

	c_int32_4 get_constant() const {
		wl_assert(is_constant());
		return c_int32_4(-static_cast<int32>(m_buffer.get_constant()));
	}

private:
	c_bool_const_buffer_or_constant m_buffer;
};

class c_iterable_buffer_bool_128_out {
public:
	typedef c_bool_buffer_iterator_128 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_out;

	c_iterable_buffer_bool_128_out(c_buffer *buffer) : m_buffer(buffer) {}
	c_buffer *get_buffer() const { return m_buffer; }

	void set_constant(const c_int32_4 &value) {
		value.store(m_buffer->get_data<int32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

class c_iterable_buffer_bool_128_inout {
public:
	typedef c_bool_buffer_iterator_128 t_iterator;
	static const e_task_qualifier k_task_qualifier = k_task_qualifier_inout;

	c_iterable_buffer_bool_128_inout(c_buffer *buffer) : m_buffer(buffer) {}
	bool is_constant() const { return m_buffer->is_constant(); }
	c_buffer *get_buffer() const { return m_buffer; }

	c_int32_4 get_constant() const {
		wl_assert(m_buffer->is_constant());
		return c_int32_4(-(*m_buffer->get_data<int32>() & 1));
	}

	void set_constant(const c_int32_4 &value) {
		value.store(m_buffer->get_data<int32>());
		m_buffer->set_constant(true);
	}

private:
	c_buffer *m_buffer;
};

// These helper templates are used to perform pure functions only, with no memory or side effects. i.e. making a call
// with the same set of inputs should always produce the same outputs.

// Note that buffer operation count will be rounded up to the block size, so they can write past buffer_size. Make sure
// that buffers are allocated as a multiple of buffer_size, even if the provided size is not.

template<typename t_operation, typename t_iterable_buffer_a>
void buffer_operator_out(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;

	t_value_a value_a = op();
	t_iterator_a::set_constant_value(iterable_buffer_a.get_buffer(), value_a);
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b>
void buffer_operator_in_out(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_out, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;

	if (iterable_buffer_a.is_constant()) {
		t_value_a value_a = iterable_buffer_a.get_constant();
		t_value_b value_b = op(value_a);
		iterable_buffer_b.set_constant(value_b);
	} else {
		c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
			iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);

		for (; iterator.is_valid(); iterator.next()) {
			t_value_a value_a = iterator.get_iterator_a().get_value();
			t_value_b value_b = op(value_a);
			iterator.get_iterator_b().set_value(value_b);
		}

		iterable_buffer_b.get_buffer()->set_constant(iterator.get_iterator_b().should_set_buffer_constant());
	}
}

template<typename t_operation, typename t_iterable_buffer_a>
void buffer_operator_inout(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;

	if (iterable_buffer_a.is_constant()) {
		t_value_a value_a_in = iterable_buffer_a.get_constant();
		t_value_a value_a_out = op(value_a_in);
		iterable_buffer_a.set_constant(value_a_out);
	} else {
		c_buffer_iterator_1<t_iterator_a> iterator(
			iterable_buffer_a.get_buffer(), buffer_size);

		for (; iterator.is_valid(); iterator.next()) {
			t_value_a value_a_in = iterator.get_iterator_a().get_value();
			t_value_a value_a_out = op(value_a_in);
			iterator.get_iterator_a().set_value(value_a_out);
		}

		iterable_buffer_a.get_buffer()->set_constant(iterator.get_iterator_a().should_set_buffer_constant());
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b, typename t_iterable_buffer_c>
void buffer_operator_in_in_out(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b,
	t_iterable_buffer_c iterable_buffer_c) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_c::k_task_qualifier == k_task_qualifier_out, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;
	typedef typename t_iterable_buffer_c::t_iterator t_iterator_c;
	typedef typename t_iterator_c::t_value t_value_c;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			t_value_a value_a = iterable_buffer_a.get_constant();
			t_value_b value_b = iterable_buffer_b.get_constant();
			t_value_c value_c = op(value_a, value_b);
			iterable_buffer_c.set_constant(value_c);
		} else {
			c_buffer_iterator_2<t_iterator_b, t_iterator_c> iterator(
				iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
			s_buffer_iterator_2_mapping_bc iterator_mapping;

			t_value_a value_a = iterable_buffer_a.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
				t_value_c value_c = op(value_a, value_b);
				iterator_mapping.get_iterator_c(iterator).set_value(value_c);
			}

			iterable_buffer_c.get_buffer()->set_constant(
				iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			c_buffer_iterator_2<t_iterator_a, t_iterator_c> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
			s_buffer_iterator_2_mapping_ac iterator_mapping;

			t_value_b value_b = iterable_buffer_b.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
				t_value_c value_c = op(value_a, value_b);
				iterator_mapping.get_iterator_c(iterator).set_value(value_c);
			}

			iterable_buffer_c.get_buffer()->set_constant(
				iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
		} else {
			c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
				buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator.get_iterator_a().get_value();
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_c value_c = op(value_a, value_b);
				iterator.get_iterator_c().set_value(value_c);
			}

			iterable_buffer_c.get_buffer()->set_constant(iterator.get_iterator_c().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b>
void buffer_operator_inout_in(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			t_value_a value_a_in = iterable_buffer_a.get_constant();
			t_value_b value_b = iterable_buffer_b.get_constant();
			t_value_a value_a_out = op(value_a_in, value_b);
			iterable_buffer_a.set_constant(value_a_out);
		} else {
			c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);

			t_value_a value_a_in = iterable_buffer_a.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_a value_a_out = op(value_a_in, value_b);
				iterator.get_iterator_a().set_value(value_a_out);
			}

			iterable_buffer_a.get_buffer()->set_constant(iterator.get_iterator_a().should_set_buffer_constant());
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			c_buffer_iterator_1<t_iterator_a> iterator(
				iterable_buffer_a.get_buffer(), buffer_size);

			t_value_b value_b = iterable_buffer_b.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a_in = iterator.get_iterator_a().get_value();
				t_value_a value_a_out = op(value_a_in, value_b);
				iterator.get_iterator_a().set_value(value_a_out);
			}

			iterable_buffer_a.get_buffer()->set_constant(iterator.get_iterator_a().should_set_buffer_constant());
		} else {
			c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a_in = iterator.get_iterator_a().get_value();
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_a value_a_out = op(value_a_in, value_b);
				iterator.get_iterator_a().set_value(value_a_out);
			}

			iterable_buffer_a.get_buffer()->set_constant(iterator.get_iterator_a().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b>
void buffer_operator_in_inout(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			t_value_a value_a = iterable_buffer_a.get_constant();
			t_value_b value_b_in = iterable_buffer_b.get_constant();
			t_value_b value_b_out = op(value_a, value_b_in);
			iterable_buffer_b.set_constant(value_b_out);
		} else {
			c_buffer_iterator_1<t_iterator_b> iterator(
				iterable_buffer_b.get_buffer(), buffer_size);
			s_buffer_iterator_1_mapping_b iterator_mapping;

			t_value_a value_a = iterable_buffer_a.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_b value_b_in = iterator_mapping.get_iterator_b(iterator).get_value();
				t_value_b value_b_out = op(value_a, value_b_in);
				iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
			}

			iterable_buffer_b.get_buffer()->set_constant(
				iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			c_buffer_iterator_1<t_iterator_b> iterator(
				iterable_buffer_b.get_buffer(), buffer_size);
			s_buffer_iterator_1_mapping_b iterator_mapping;

			t_value_b value_b_in = iterable_buffer_b.get_constant();
			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator_mapping.get_iterator_b(iterator).get_value();
				t_value_b value_b_out = op(value_a, value_b_in);
				iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
			}

			iterable_buffer_b.get_buffer()->set_constant(
				iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
		} else {
			c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator.get_iterator_a().get_value();
				t_value_b value_b_in = iterator.get_iterator_b().get_value();
				t_value_b value_b_out = op(value_a, value_b_in);
				iterator.get_iterator_b().set_value(value_b_out);
			}

			iterable_buffer_b.get_buffer()->set_constant(iterator.get_iterator_b().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b,
	typename t_iterable_buffer_c, typename t_iterable_buffer_d>
void buffer_operator_in_in_in_out(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b,
	t_iterable_buffer_c iterable_buffer_c,
	t_iterable_buffer_d iterable_buffer_d) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_c::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_d::k_task_qualifier == k_task_qualifier_out, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;
	typedef typename t_iterable_buffer_c::t_iterator t_iterator_c;
	typedef typename t_iterator_c::t_value t_value_c;
	typedef typename t_iterable_buffer_d::t_iterator t_iterator_d;
	typedef typename t_iterator_d::t_value t_value_d;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				t_value_d value_d = op(value_a, value_b, value_c);
				iterable_buffer_d.set_constant(value_d);
			} else {
				c_buffer_iterator_2<t_iterator_c, t_iterator_d> iterator(
					iterable_buffer_c.get_buffer(), iterable_buffer_d.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_cd iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_d value_d = op(value_a, value_b, value_c);
					iterator_mapping.get_iterator_d(iterator).set_value(value_d);
				}

				iterable_buffer_d.get_buffer()->set_constant(
					iterator_mapping.get_iterator_d(iterator).should_set_buffer_constant());
			}
		} else {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_b, t_iterator_d> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_d.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_bd iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_d value_d = op(value_a, value_b, value_c);
					iterator_mapping.get_iterator_d(iterator).set_value(value_d);
				}

				iterable_buffer_d.get_buffer()->set_constant(
					iterator_mapping.get_iterator_d(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_3<t_iterator_b, t_iterator_c, t_iterator_d> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), iterable_buffer_d.get_buffer(),
					buffer_size);
				s_buffer_iterator_3_mapping_bcd iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_d value_d = op(value_a, value_b, value_c);
					iterator_mapping.get_iterator_d(iterator).set_value(value_d);
				}

				iterable_buffer_d.get_buffer()->set_constant(
					iterator_mapping.get_iterator_d(iterator).should_set_buffer_constant());
			}
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_a, t_iterator_d> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_d.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ad iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_d value_d = op(value_a, value_b, value_c);
					iterator_mapping.get_iterator_d(iterator).set_value(value_d);
				}

				iterable_buffer_d.get_buffer()->set_constant(
					iterator_mapping.get_iterator_d(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_3<t_iterator_a, t_iterator_c, t_iterator_d> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), iterable_buffer_d.get_buffer(),
					buffer_size);
				s_buffer_iterator_3_mapping_acd iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_d value_d = op(value_a, value_b, value_c);
					iterator_mapping.get_iterator_d(iterator).set_value(value_d);
				}

				iterable_buffer_d.get_buffer()->set_constant(
					iterator_mapping.get_iterator_d(iterator).should_set_buffer_constant());
			}
		} else {
			c_buffer_iterator_4<t_iterator_a, t_iterator_b, t_iterator_c, t_iterator_d> iterator(
				iterable_buffer_a.get_buffer(),
				iterable_buffer_b.get_buffer(),
				iterable_buffer_c.get_buffer(),
				iterable_buffer_d.get_buffer(),
				buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator.get_iterator_a().get_value();
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_c value_c = iterator.get_iterator_c().get_value();
				t_value_d value_d = op(value_a, value_b, value_c);
				iterator.get_iterator_d().set_value(value_d);
			}

			iterable_buffer_d.get_buffer()->set_constant(iterator.get_iterator_d().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b, typename t_iterable_buffer_c>
void buffer_operator_inout_in_in(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b,
	t_iterable_buffer_c iterable_buffer_c) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_c::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;
	typedef typename t_iterable_buffer_c::t_iterator t_iterator_c;
	typedef typename t_iterator_c::t_value t_value_c;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				t_value_a value_a_in = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				t_value_a value_a_out = op(value_a_in, value_b, value_c);
				iterable_buffer_a.set_constant(value_a_out);
			} else {
				c_buffer_iterator_2<t_iterator_a, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ac iterator_mapping;

				t_value_a value_a_in = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_a value_a_out = op(value_a_in, value_b, value_c);
					iterator_mapping.get_iterator_a(iterator).set_value(value_a_out);
				}

				iterable_buffer_a.get_buffer()->set_constant(
					iterator_mapping.get_iterator_a(iterator).should_set_buffer_constant());
			}
		} else {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ab iterator_mapping;

				t_value_a value_a_in = iterable_buffer_a.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_a value_a_out = op(value_a_in, value_b, value_c);
					iterator_mapping.get_iterator_a(iterator).set_value(value_a_out);
				}

				iterable_buffer_a.get_buffer()->set_constant(
					iterator_mapping.get_iterator_a(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
					buffer_size);
				s_buffer_iterator_3_mapping_abc iterator_mapping;

				t_value_a value_a_in = iterable_buffer_a.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_a value_a_out = op(value_a_in, value_b, value_c);
					iterator_mapping.get_iterator_a(iterator).set_value(value_a_out);
				}

				iterable_buffer_a.get_buffer()->set_constant(
					iterator_mapping.get_iterator_a(iterator).should_set_buffer_constant());
			}
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_1<t_iterator_a> iterator(
					iterable_buffer_a.get_buffer(), buffer_size);
				s_buffer_iterator_1_mapping_a iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a_in = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_a value_a_out = op(value_a_in, value_b, value_c);
					iterator_mapping.get_iterator_a(iterator).set_value(value_a_out);
				}

				iterable_buffer_a.get_buffer()->set_constant(
					iterator_mapping.get_iterator_a(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_2<t_iterator_a, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ac iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a_in = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_a value_a_out = op(value_a_in, value_b, value_c);
					iterator_mapping.get_iterator_a(iterator).set_value(value_a_out);
				}

				iterable_buffer_a.get_buffer()->set_constant(
					iterator_mapping.get_iterator_a(iterator).should_set_buffer_constant());
			}
		} else {
			c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
				buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a_in = iterator.get_iterator_a().get_value();
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_c value_c = iterator.get_iterator_c().get_value();
				t_value_a value_a_out = op(value_a_in, value_b, value_c);
				iterator.get_iterator_a().set_value(value_a_out);
			}

			iterable_buffer_a.get_buffer()->set_constant(iterator.get_iterator_a().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b, typename t_iterable_buffer_c>
void buffer_operator_in_inout_in(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b,
	t_iterable_buffer_c iterable_buffer_c) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");
	static_assert(t_iterable_buffer_c::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;
	typedef typename t_iterable_buffer_c::t_iterator t_iterator_c;
	typedef typename t_iterator_c::t_value t_value_c;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b_in = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				t_value_b value_b_out = op(value_a, value_b_in, value_c);
				iterable_buffer_b.set_constant(value_b_out);
			} else {
				c_buffer_iterator_2<t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_bc iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b_in = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_b value_b_out = op(value_a, value_b_in, value_c);
					iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
				}

				iterable_buffer_b.get_buffer()->set_constant(
					iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
			}
		} else {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_1<t_iterator_b> iterator(iterable_buffer_b.get_buffer(), buffer_size);
				s_buffer_iterator_1_mapping_b iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b_in = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_b value_b_out = op(value_a, value_b_in, value_c);
					iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
				}

				iterable_buffer_b.get_buffer()->set_constant(
					iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_2<t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_bc iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b_in = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_b value_b_out = op(value_a, value_b_in, value_c);
					iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
				}

				iterable_buffer_b.get_buffer()->set_constant(
					iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
			}
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_a, t_iterator_b> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ab iterator_mapping;

				t_value_b value_b_in = iterable_buffer_b.get_constant();
				t_value_c value_c = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_b value_b_out = op(value_a, value_b_in, value_c);
					iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
				}

				iterable_buffer_b.get_buffer()->set_constant(
					iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
					buffer_size);
				s_buffer_iterator_3_mapping_abc iterator_mapping;

				t_value_b value_b_in = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_c value_c = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_b value_b_out = op(value_a, value_b_in, value_c);
					iterator_mapping.get_iterator_b(iterator).set_value(value_b_out);
				}

				iterable_buffer_b.get_buffer()->set_constant(
					iterator_mapping.get_iterator_b(iterator).should_set_buffer_constant());
			}
		} else {
			c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
				buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator.get_iterator_a().get_value();
				t_value_b value_b_in = iterator.get_iterator_b().get_value();
				t_value_c value_c = iterator.get_iterator_c().get_value();
				t_value_b value_b_out = op(value_a, value_b_in, value_c);
				iterator.get_iterator_b().set_value(value_b_out);
			}

			iterable_buffer_b.get_buffer()->set_constant(iterator.get_iterator_b().should_set_buffer_constant());
		}
	}
}

template<typename t_operation, typename t_iterable_buffer_a, typename t_iterable_buffer_b, typename t_iterable_buffer_c>
void buffer_operator_in_in_inout(const t_operation &op, size_t buffer_size,
	t_iterable_buffer_a iterable_buffer_a,
	t_iterable_buffer_b iterable_buffer_b,
	t_iterable_buffer_c iterable_buffer_c) {
	static_assert(t_iterable_buffer_a::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_b::k_task_qualifier == k_task_qualifier_in, "Incorrect qualifier");
	static_assert(t_iterable_buffer_c::k_task_qualifier == k_task_qualifier_inout, "Incorrect qualifier");

	typedef typename t_iterable_buffer_a::t_iterator t_iterator_a;
	typedef typename t_iterator_a::t_value t_value_a;
	typedef typename t_iterable_buffer_b::t_iterator t_iterator_b;
	typedef typename t_iterator_b::t_value t_value_b;
	typedef typename t_iterable_buffer_c::t_iterator t_iterator_c;
	typedef typename t_iterator_c::t_value t_value_c;

	if (iterable_buffer_a.is_constant()) {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c_in = iterable_buffer_c.get_constant();
				t_value_c value_c_out = op(value_a, value_b, value_c_in);
				iterable_buffer_c.set_constant(value_c_out);
			} else {
				c_buffer_iterator_1<t_iterator_c> iterator(iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_1_mapping_c iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_c value_c_in = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_c value_c_out = op(value_a, value_b, value_c_in);
					iterator_mapping.get_iterator_c(iterator).set_value(value_c_out);
				}

				iterable_buffer_c.get_buffer()->set_constant(
					iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
			}
		} else {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_bc iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				t_value_c value_c_in = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_c value_c_out = op(value_a, value_b, value_c_in);
					iterator_mapping.get_iterator_c(iterator).set_value(value_c_out);
				}

				iterable_buffer_c.get_buffer()->set_constant(
					iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_2<t_iterator_b, t_iterator_c> iterator(
					iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_bc iterator_mapping;

				t_value_a value_a = iterable_buffer_a.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_b value_b = iterator_mapping.get_iterator_b(iterator).get_value();
					t_value_c value_c_in = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_c value_c_out = op(value_a, value_b, value_c_in);
					iterator_mapping.get_iterator_c(iterator).set_value(value_c_out);
				}

				iterable_buffer_c.get_buffer()->set_constant(
					iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
			}
		}
	} else {
		if (iterable_buffer_b.is_constant()) {
			if (iterable_buffer_c.is_constant()) {
				c_buffer_iterator_2<t_iterator_a, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ac iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				t_value_c value_c_in = iterable_buffer_c.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_c value_c_out = op(value_a, value_b, value_c_in);
					iterator_mapping.get_iterator_c(iterator).set_value(value_c_out);
				}

				iterable_buffer_c.get_buffer()->set_constant(
					iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
			} else {
				c_buffer_iterator_2<t_iterator_a, t_iterator_c> iterator(
					iterable_buffer_a.get_buffer(), iterable_buffer_c.get_buffer(), buffer_size);
				s_buffer_iterator_2_mapping_ac iterator_mapping;

				t_value_b value_b = iterable_buffer_b.get_constant();
				for (; iterator.is_valid(); iterator.next()) {
					t_value_a value_a = iterator_mapping.get_iterator_a(iterator).get_value();
					t_value_c value_c_in = iterator_mapping.get_iterator_c(iterator).get_value();
					t_value_c value_c_out = op(value_a, value_b, value_c_in);
					iterator_mapping.get_iterator_c(iterator).set_value(value_c_out);
				}

				iterable_buffer_c.get_buffer()->set_constant(
					iterator_mapping.get_iterator_c(iterator).should_set_buffer_constant());
			}
		} else {
			c_buffer_iterator_3<t_iterator_a, t_iterator_b, t_iterator_c> iterator(
				iterable_buffer_a.get_buffer(), iterable_buffer_b.get_buffer(), iterable_buffer_c.get_buffer(),
				buffer_size);

			for (; iterator.is_valid(); iterator.next()) {
				t_value_a value_a = iterator.get_iterator_a().get_value();
				t_value_b value_b = iterator.get_iterator_b().get_value();
				t_value_c value_c_in = iterator.get_iterator_c().get_value();
				t_value_c value_c_out = op(value_a, value_b, value_c_in);
				iterator.get_iterator_c().set_value(value_c_out);
			}

			iterable_buffer_c.get_buffer()->set_constant(iterator.get_iterator_c().should_set_buffer_constant());
		}
	}
}

template<typename t_operation>
struct s_buffer_operation_real_real {
	static void in_out(size_t buffer_size, c_real_buffer_or_constant_in a, c_real_buffer_out b) {
		buffer_operator_in_out(t_operation(), buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_out(b));
	}

	static void inout(size_t buffer_size, c_real_buffer_inout a) {
		buffer_operator_inout(t_operation(), buffer_size,
			c_iterable_buffer_real_inout(a));
	}
};

template<typename t_operation>
struct s_buffer_operation_real_real_real {
	static void in_in_out(size_t buffer_size,
		c_real_buffer_or_constant_in a, c_real_buffer_or_constant_in b, c_real_buffer_out c) {
		buffer_operator_in_in_out(t_operation(), buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_in(b),
			c_iterable_buffer_real_out(c));
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout a, c_real_buffer_or_constant_in b) {
		buffer_operator_inout_in(t_operation(), buffer_size,
			c_iterable_buffer_real_inout(a),
			c_iterable_buffer_real_in(b));
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in a, c_real_buffer_inout b) {
		buffer_operator_in_inout(t_operation(), buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_inout(b));
	}
};

template<typename t_operation>
struct s_buffer_operation_real_real_bool {
	static void in_in_out(size_t buffer_size,
		c_real_buffer_or_constant_in a, c_real_buffer_or_constant_in b, c_bool_buffer_out c) {
		buffer_operator_in_in_out(t_operation(), buffer_size,
			c_iterable_buffer_real_in(a),
			c_iterable_buffer_real_in(b),
			c_iterable_buffer_bool_4_out(c));
	}
};

template<typename t_operation>
struct s_buffer_operation_bool_bool {
	static void in_out(size_t buffer_size, c_bool_buffer_or_constant_in a, c_bool_buffer_out b) {
		buffer_operator_in_out(t_operation(), buffer_size,
			c_iterable_buffer_bool_128_in(a),
			c_iterable_buffer_bool_128_out(b));
	}

	static void inout(size_t buffer_size, c_bool_buffer_inout a) {
		buffer_operator_inout(t_operation(), buffer_size,
			c_iterable_buffer_bool_128_inout(a));
	}
};

template<typename t_operation>
struct s_buffer_operation_bool_bool_bool {
	static void in_in_out(size_t buffer_size,
		c_bool_buffer_or_constant_in a, c_bool_buffer_or_constant_in b, c_bool_buffer_out c) {
		buffer_operator_in_in_out(t_operation(), buffer_size,
			c_iterable_buffer_bool_128_in(a),
			c_iterable_buffer_bool_128_in(b),
			c_iterable_buffer_bool_128_out(c));
	}

	static void inout_in(size_t buffer_size, c_bool_buffer_inout a, c_bool_buffer_or_constant_in b) {
		buffer_operator_inout_in(t_operation(), buffer_size,
			c_iterable_buffer_bool_128_inout(a),
			c_iterable_buffer_bool_128_in(b));
	}

	static void in_inout(size_t buffer_size, c_bool_buffer_or_constant_in a, c_bool_buffer_inout b) {
		buffer_operator_in_inout(t_operation(), buffer_size,
			c_iterable_buffer_bool_128_in(a),
			c_iterable_buffer_bool_128_inout(b));
	}
};

#endif // WAVELANG_ENGINE_BUFFER_OPERATIONS_BUFFER_OPERATIONS_H__
