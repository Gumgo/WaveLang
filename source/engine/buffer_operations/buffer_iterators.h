#pragma once

#include "common/common.h"

#include "engine/buffer.h"
#include "engine/math/math.h"

// $TODO this should actually be called for all buffers being passed into any task, not in the task itself
inline void validate_buffer(const c_buffer *buffer) {
	wl_assert(buffer);
	wl_assert(is_pointer_aligned(buffer->get_data(), k_simd_alignment));
}

// Base iterator interface:
//class c_buffer_iterator {
//public:
//	typedef <simd_type> t_value;
//	typedef [const] c_buffer t_buffer;
//
//	// Number of elements contained in each value type (e.g. SSE reals are 4 elements, SSE bools are 128)
//	static const size_t k_elements_per_value;
//
//	// Constructs the iterator
//	c_buffer_iterator(t_buffer *buffer, size_t buffer_size);
//
//	// Advances to the next element
//	void next();
//
//	// Returns the current value
//	t_value get_value() const;
//
//	// [Non-const only] Updates the value in the buffer to the one provided
//	void set_value(const t_value &value);
//
//	// [Non-const only] If true, the buffer can safely be marked as constant
//	bool should_set_buffer_constant() const;
//};

// Iterates a real buffer 4 elements at a time
class c_real_buffer_iterator {
public:
	typedef c_real32_4 t_value;
	typedef c_buffer t_buffer;

	static const size_t k_elements_per_value = 4;

	c_real_buffer_iterator(c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<real32>()) {
		validate_buffer(buffer);
	}

	void next() {
		m_pointer += k_simd_block_elements;
	}

	c_real32_4 get_value() const {
		return c_real32_4(m_pointer);
	}

	void set_value(const c_real32_4 &value) {
		value.store(m_pointer);
	}

	bool should_set_buffer_constant() const {
		// No constant detection in this iterator
		return false;
	}

private:
	real32 *m_pointer;
};

// Const version of the above
class c_const_real_buffer_iterator {
public:
	typedef c_real32_4 t_value;
	typedef const c_buffer t_buffer;

	static const size_t k_elements_per_value = 4;

	c_const_real_buffer_iterator(const c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<real32>()) {
		validate_buffer(buffer);
		wl_assert(!buffer->is_constant());
	}

	void next() {
		m_pointer += k_simd_block_elements;
	}

	c_real32_4 get_value() const {
		return c_real32_4(m_pointer);
	}

private:
	const real32 *m_pointer;
};

// Iterates a bool buffer 4 elements at a time, value contains 4 elements (with all bits cleared or set)
class c_bool_buffer_iterator_4 {
public:
	typedef c_int32_4 t_value;
	typedef c_buffer t_buffer;

	static const size_t k_elements_per_value = 4;

	c_bool_buffer_iterator_4(c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<int32>())
		, m_offset(0)
		, m_buffer_size(buffer_size)
		, m_cached_value(0)
		, m_dirty_cache(false) {
		validate_buffer(buffer);
		read_cached_value_if_necessary();
	}

	~c_bool_buffer_iterator_4() {
		// Store unwritten elements on destruction if necessary
		write_cached_value_if_necessary();
	}

	void next() {
		m_offset += k_simd_block_elements;
		if ((m_offset % 128) == 0) {
			m_pointer += k_simd_block_elements;
		}

		read_cached_value_if_necessary();
	}

	c_int32_4 get_value() const {
		int32 element_index = cast_integer_verify<int32>(m_offset / 32) % 4;
		int32 shift_amount = cast_integer_verify<int32>(m_offset % 32);
		wl_assert(element_index < 4);

		// Copy the desired bits to all elements
		c_int32_4 unshifted_value_bits = single_element(m_cached_value, element_index);

		// Shift so that the correct 4 bits are in the front of the desired element, and then shift each bit to its
		// proper place within its element
		c_int32_4 element_shift_amounts = c_int32_4(shift_amount) + c_int32_4(0, 1, 2, 3);
		c_int32_4 value_bits = unshifted_value_bits >> element_shift_amounts;

		// Mask everything but the LSB and then negate, which will turn 0 => 0 and 1 => 0xffffffff
		return -(value_bits & c_int32_4(0x00000001));
	}

	void set_value(const c_int32_4 &value) {
		int32 element_index = cast_integer_verify<int32>(m_offset / 32) % 4;
		int32 shift_amount = cast_integer_verify<int32>(m_offset % 32);
		wl_assert(element_index < 4);

		// Mask only the correct element with 0xffffffff, all other bits are 0
		c_int32_4 element_mask = (c_int32_4(element_index) == c_int32_4(0, 1, 2, 3));

		// Mask only the correct bits in the correct element with 0xf, all other bits are 0
		c_int32_4 element_value_mask = element_mask & c_int32_4(0x0000000f << shift_amount);

		// Convert the MSB of the value into a 4-bit mask and shift it to the proper spot in the element
		int32 value_bits = mask_from_msb(value) << shift_amount;

		// Copy our value bits into each element and mask to only the desired element
		c_int32_4 element_value_bits(value_bits & element_value_mask);

		// Clear bits in the cached value, then overwrite them with the new value
		m_cached_value = (m_cached_value & ~element_value_mask) | element_value_bits;
		m_dirty_cache = true;

		if ((m_offset % 128) == 124) {
			// Write when we're at the end of the SSE block
			write_cached_value_if_necessary();
		}
	}

	bool should_set_buffer_constant() const {
		// No constant detection in this iterator
		return false;
	}

private:
	void read_cached_value_if_necessary() {
		if ((m_offset % 128) == 0 && m_offset < m_buffer_size) {
			// If we've read 128 elements, we need to load a new value into the cache, unless we're at the end
			m_cached_value.load(m_pointer);
		}
	}

	void write_cached_value_if_necessary() {
		if (m_dirty_cache) {
			m_cached_value.store(m_pointer);
			m_dirty_cache = false;
		}
	}

	int32 *m_pointer;
	size_t m_offset;
	size_t m_buffer_size;
	c_int32_4 m_cached_value;
	bool m_dirty_cache;
};

// Const version of the above
class c_const_bool_buffer_iterator_4 {
public:
	typedef c_int32_4 t_value;
	typedef const c_buffer t_buffer;

	static const size_t k_elements_per_value = 4;

	c_const_bool_buffer_iterator_4(const c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<int32>())
		, m_offset(0)
		, m_buffer_size(buffer_size)
		, m_cached_value(0) {
		validate_buffer(buffer);
		wl_assert(!buffer->is_constant());
		read_cached_value_if_necessary();
	}

	void next() {
		m_offset += k_simd_block_elements;
		if ((m_offset % 128) == 0) {
			m_pointer += k_simd_block_elements;
		}

		read_cached_value_if_necessary();
	}

	c_int32_4 get_value() const {
		int32 element_index = cast_integer_verify<int32>(m_offset / 32) % 4;
		int32 shift_amount = cast_integer_verify<int32>(m_offset % 32);
		wl_assert(element_index < 4);

		// Copy the desired bits to all elements
		c_int32_4 unshifted_value_bits = single_element(m_cached_value, element_index);

		// Shift so that the correct 4 bits are in the front of the desired element, and then shift each bit to its
		// proper place within its element
		c_int32_4 element_shift_amounts = c_int32_4(shift_amount) + c_int32_4(0, 1, 2, 3);
		c_int32_4 value_bits = unshifted_value_bits >> element_shift_amounts;

		// Mask everything but the LSB and then negate, which will turn 0 => 0 and 1 => 0xffffffff
		return -(value_bits & c_int32_4(0x00000001));
	}

private:
	void read_cached_value_if_necessary() {
		if ((m_offset % 128) == 0 && m_offset < m_buffer_size) {
			// If we've read 128 elements, we need to load a new value into the cache, unless we're at the end
			m_cached_value.load(m_pointer);
		}
	}

	const int32 *m_pointer;
	size_t m_offset;
	size_t m_buffer_size;
	c_int32_4 m_cached_value;
};

// Iterates a bool buffer 128 elements at a time, value contains 4x32 elements
class c_bool_buffer_iterator_128 {
public:
	typedef c_int32_4 t_value;
	typedef c_buffer t_buffer;

	static const size_t k_elements_per_value = 128;

	c_bool_buffer_iterator_128(c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<int32>())
		, m_elements_remaining(cast_integer_verify<int32>(buffer_size))
		, m_all_zero(0)
		, m_all_one(0xffffffff) {
		validate_buffer(buffer);
	}

	void next() {
		m_pointer += k_simd_block_elements;
		m_elements_remaining -= 128;
	}

	c_int32_4 get_value() const {
		wl_assert(m_elements_remaining > 0);
		return c_int32_4(m_pointer);
	}

	void set_value(const c_int32_4 &value) {
		wl_assert(m_elements_remaining > 0);
		value.store(m_pointer);

		// Mask out any bits at the end that go beyond the buffer
		c_int32_4 elements_remaining = c_int32_4(m_elements_remaining) - c_int32_4(0, 32, 64, 96);
		c_int32_4 clamped_elements_remaining = min(max(elements_remaining, c_int32_4(0)), c_int32_4(32));
		c_int32_4 zeros_to_shift = c_int32_4(32) - clamped_elements_remaining;
		c_int32_4 overflow_mask = c_int32_4(0xffffffff) << zeros_to_shift;

		// We now have zeros in the positions of all bits past the end of the buffer
		m_all_zero = m_all_zero | (value & overflow_mask);
		m_all_one = m_all_one & (value | ~overflow_mask);
	}

	bool should_set_buffer_constant() const {
		// If the buffer is constant, we expect m_all_zero to be all zeros or m_all_one to be all ones. Perform this
		// check and make an integer mask out of the resulting boolean masks (a 4-bit value). If either integer mask has
		// a value of 3 (i.e. all 4 bits set), the buffer is constant.
		c_int32_4 all_zero = (m_all_zero == c_int32_4(0));
		c_int32_4 all_one = (m_all_one == c_int32_4(0xffffffff));
		int32 all_zero_msb = mask_from_msb(all_zero);
		int32 all_one_msb = mask_from_msb(all_one);

		return (all_zero_msb == 15) || (all_one_msb == 15);
	}

private:
	int32 *m_pointer;
	int32 m_elements_remaining;

	c_int32_4 m_all_zero;
	c_int32_4 m_all_one;
};

// Const version of the above
class c_const_bool_buffer_iterator_128 {
public:
	typedef c_int32_4 t_value;
	typedef const c_buffer t_buffer;

	static const size_t k_elements_per_value = 128;

	c_const_bool_buffer_iterator_128(const c_buffer *buffer, size_t buffer_size)
		: m_pointer(buffer->get_data<int32>()) {
		validate_buffer(buffer);
		wl_assert(!buffer->is_constant());
	}

	void next() {
		m_pointer += k_simd_block_elements;
	}

	c_int32_4 get_value() const {
		return c_int32_4(m_pointer);
	}

private:
	const int32 *m_pointer;
};

// Multi-iterators:

template<typename t_iterator_a_typename>
class c_buffer_iterator_1 {
public:
	typedef t_iterator_a_typename t_iterator_a;

	c_buffer_iterator_1(typename t_iterator_a::t_buffer *buffer_a, size_t buffer_size)
		: m_offset(0)
		, m_buffer_size(buffer_size)
		, m_iterator_a(buffer_a, buffer_size) {
	}

	bool is_valid() {
		return m_offset < m_buffer_size;
	}

	void next() {
		m_offset += t_iterator_a::k_elements_per_value;
		m_iterator_a.next();
	}

	t_iterator_a &get_iterator_a() {
		return m_iterator_a;
	}

private:
	size_t m_offset;
	size_t m_buffer_size;
	t_iterator_a m_iterator_a;
};

template<typename t_iterator_a_typename, typename t_iterator_b_typename>
class c_buffer_iterator_2 {
public:
	typedef t_iterator_a_typename t_iterator_a;
	typedef t_iterator_b_typename t_iterator_b;

	static_assert(t_iterator_a::k_elements_per_value == t_iterator_b::k_elements_per_value,
		"k_elements_per_value mismatch");

	c_buffer_iterator_2(
		typename t_iterator_a::t_buffer *buffer_a, typename t_iterator_b::t_buffer *buffer_b, size_t buffer_size)
		: m_offset(0)
		, m_buffer_size(buffer_size)
		, m_iterator_a(buffer_a, buffer_size)
		, m_iterator_b(buffer_b, buffer_size) {
	}

	bool is_valid() {
		return m_offset < m_buffer_size;
	}

	void next() {
		m_offset += t_iterator_a::k_elements_per_value;
		m_iterator_a.next();
		m_iterator_b.next();
	}

	t_iterator_a &get_iterator_a() {
		return m_iterator_a;
	}

	t_iterator_b &get_iterator_b() {
		return m_iterator_b;
	}

private:
	size_t m_offset;
	size_t m_buffer_size;
	t_iterator_a m_iterator_a;
	t_iterator_b m_iterator_b;
};

template<typename t_iterator_a_typename, typename t_iterator_b_typename, typename t_iterator_c_typename>
class c_buffer_iterator_3 {
public:
	typedef t_iterator_a_typename t_iterator_a;
	typedef t_iterator_b_typename t_iterator_b;
	typedef t_iterator_c_typename t_iterator_c;

	static_assert(t_iterator_a::k_elements_per_value == t_iterator_b::k_elements_per_value,
		"k_elements_per_value mismatch");
	static_assert(t_iterator_b::k_elements_per_value == t_iterator_c::k_elements_per_value,
		"k_elements_per_value mismatch");

	c_buffer_iterator_3(
		typename t_iterator_a::t_buffer *buffer_a,
		typename t_iterator_b::t_buffer *buffer_b,
		typename t_iterator_c::t_buffer *buffer_c,
		size_t buffer_size)
		: m_offset(0)
		, m_buffer_size(buffer_size)
		, m_iterator_a(buffer_a, buffer_size)
		, m_iterator_b(buffer_b, buffer_size)
		, m_iterator_c(buffer_c, buffer_size) {
	}

	bool is_valid() {
		return m_offset < m_buffer_size;
	}

	void next() {
		m_offset += t_iterator_a::k_elements_per_value;
		m_iterator_a.next();
		m_iterator_b.next();
		m_iterator_c.next();
	}

	t_iterator_a &get_iterator_a() {
		return m_iterator_a;
	}

	t_iterator_b &get_iterator_b() {
		return m_iterator_b;
	}

	t_iterator_c &get_iterator_c() {
		return m_iterator_c;
	}

private:
	size_t m_offset;
	size_t m_buffer_size;
	t_iterator_a m_iterator_a;
	t_iterator_b m_iterator_b;
	t_iterator_c m_iterator_c;
};

template<typename t_iterator_a_typename, typename t_iterator_b_typename,
	typename t_iterator_c_typename, typename t_iterator_d_typename>
class c_buffer_iterator_4 {
public:
	typedef t_iterator_a_typename t_iterator_a;
	typedef t_iterator_b_typename t_iterator_b;
	typedef t_iterator_c_typename t_iterator_c;
	typedef t_iterator_d_typename t_iterator_d;

	static_assert(t_iterator_a::k_elements_per_value == t_iterator_b::k_elements_per_value,
		"k_elements_per_value mismatch");
	static_assert(t_iterator_b::k_elements_per_value == t_iterator_c::k_elements_per_value,
		"k_elements_per_value mismatch");
	static_assert(t_iterator_c::k_elements_per_value == t_iterator_d::k_elements_per_value,
		"k_elements_per_value mismatch");

	c_buffer_iterator_4(
		typename t_iterator_a::t_buffer *buffer_a,
		typename t_iterator_b::t_buffer *buffer_b,
		typename t_iterator_c::t_buffer *buffer_c,
		typename t_iterator_d::t_buffer *buffer_d,
		size_t buffer_size)
		: m_offset(0)
		, m_buffer_size(buffer_size)
		, m_iterator_a(buffer_a, buffer_size)
		, m_iterator_b(buffer_b, buffer_size)
		, m_iterator_c(buffer_c, buffer_size)
		, m_iterator_d(buffer_d, buffer_size) {
	}

	bool is_valid() {
		return m_offset < m_buffer_size;
	}

	void next() {
		m_offset += t_iterator_a::k_elements_per_value;
		m_iterator_a.next();
		m_iterator_b.next();
		m_iterator_c.next();
		m_iterator_d.next();
	}

	t_iterator_a &get_iterator_a() {
		return m_iterator_a;
	}

	t_iterator_b &get_iterator_b() {
		return m_iterator_b;
	}

	t_iterator_c &get_iterator_c() {
		return m_iterator_c;
	}

	t_iterator_d &get_iterator_d() {
		return m_iterator_d;
	}

private:
	size_t m_offset;
	size_t m_buffer_size;
	t_iterator_a m_iterator_a;
	t_iterator_b m_iterator_b;
	t_iterator_c m_iterator_c;
	t_iterator_d m_iterator_d;
};

// Mappings reassign iterator names to make multi-iterators easier to use in practice

struct s_buffer_iterator_1_mapping_a {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}
};

struct s_buffer_iterator_1_mapping_b {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}
};

struct s_buffer_iterator_1_mapping_c {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}
};

struct s_buffer_iterator_1_mapping_d {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}
};

struct s_buffer_iterator_2_mapping_ab {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_2_mapping_ac {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_2_mapping_ad {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_2_mapping_bc {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_2_mapping_bd {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_2_mapping_cd {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}
};

struct s_buffer_iterator_3_mapping_abc {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_c &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_c();
	}
};

struct s_buffer_iterator_3_mapping_abd {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_c &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_c();
	}
};

struct s_buffer_iterator_3_mapping_acd {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_a(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_c &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_c();
	}
};

struct s_buffer_iterator_3_mapping_bcd {
	template<typename t_iterator> typename t_iterator::t_iterator_a &get_iterator_b(t_iterator &iterator) {
		return iterator.get_iterator_a();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_b &get_iterator_c(t_iterator &iterator) {
		return iterator.get_iterator_b();
	}

	template<typename t_iterator> typename t_iterator::t_iterator_c &get_iterator_d(t_iterator &iterator) {
		return iterator.get_iterator_c();
	}
};

