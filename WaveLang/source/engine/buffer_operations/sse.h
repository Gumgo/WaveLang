#ifndef WAVELANG_SSE_H__
#define WAVELANG_SSE_H__

#include "common/common.h"

#define SSE_ENABLED 1

#define SSE_ALIGNMENT 16
#define SSE_BLOCK_SIZE 4

// More type-friendly version:
static const size_t k_sse_alignment = SSE_ALIGNMENT;
static const size_t k_sse_block_size = SSE_BLOCK_SIZE;

#if PREDEFINED(SSE_ENABLED)

#if PREDEFINED(COMPILER_MSVC)
#include <intrin.h>
#else PREDEFINED(COMPILER_MSVC)
#include <x86intrin.h>
#endif // PREDEFINED(COMPILER_MSVC)

class c_real32_4 {
public:
	c_real32_4() {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
	}

	c_real32_4(real32 v) {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
		m_value = _mm_set1_ps(v);
	}

	c_real32_4(real32 a, real32 b, real32 c, real32 d) {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
		m_value = _mm_set_ps(a, b, c, d);
	}

	c_real32_4(const real32 *ptr) {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
		wl_assert(is_pointer_aligned(ptr, k_sse_alignment));
		m_value = *reinterpret_cast<const __m128 *>(ptr);
	}

	c_real32_4(const __m128 &v)
		: m_value(v) {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
	}

	c_real32_4(const c_real32_4 &v)
		: m_value(v.m_value) {
		wl_assert(is_pointer_aligned(this, k_sse_alignment));
	}

	c_real32_4 &operator=(const __m128 &v) {
		m_value = v;
	}

	c_real32_4 &operator=(const c_real32_4 &v) {
		m_value = v.m_value;
		return *this;
	}

	operator __m128() const {
		return m_value;
	}

	// $TODO could implement +=, etc.

private:
	__m128 m_value;
};

// Unary:

inline c_real32_4 operator-(const c_real32_4 &v) {
	return _mm_sub_ps(_mm_set1_ps(0.0f), v);
}

// Binary:

inline c_real32_4 operator+(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_add_ps(lhs, rhs);
}

inline c_real32_4 operator-(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_sub_ps(lhs, rhs);
}

inline c_real32_4 operator*(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_mul_ps(lhs, rhs);
}

inline c_real32_4 operator/(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return _mm_div_ps(lhs, rhs);
}

inline c_real32_4 operator%(const c_real32_4 &lhs, const c_real32_4 &rhs) {
	return 0.0f; // $TODO unimplemented!!!
}

#else // PREDEFINED(SSE_ENABLED)

#error Implement non-SSE operations

#endif // PREDEFINED(SSE_ENABLED)

#endif // WAVELANG_SSE_H__