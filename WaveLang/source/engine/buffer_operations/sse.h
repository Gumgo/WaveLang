#ifndef WAVELANG_SSE_H__
#define WAVELANG_SSE_H__

#include "common/common.h"
#include <algorithm>

#define SSE_ENABLED 1

#define SSE_ALIGNMENT 16
#define SSE_BLOCK_SIZE 4

// More type-friendly version:
static const size_t k_sse_alignment = SSE_ALIGNMENT;
static const size_t k_sse_block_size = SSE_BLOCK_SIZE;

#if PREDEFINED(SSE_ENABLED)

#if PREDEFINED(COMPILER_MSVC)
#include <intrin.h>
#else // PREDEFINED(COMPILER_MSVC)
#include <x86intrin.h>
#endif // PREDEFINED(COMPILER_MSVC)

#ifndef WAVELANG_SSE_MATHFUN_H__
#define WAVELANG_SSE_MATHFUN_H__
#define USE_SSE2
#pragma warning(disable:4305)
#include "sse_mathfun.h"
#pragma warning(default:4305)
#endif // WAVELANG_SSE_MATHFUN_H__

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

inline c_real32_4 abs(const c_real32_4 &v) {
	// Mask off the sign bit for fast abs
	static const __m128 k_sign_mask = _mm_set1_ps(-0.0f);
	return _mm_andnot_ps(k_sign_mask, v);
}

// $TODO write an SSE version which doesn't rely on SSE4.1 for floor, ceil, and round

inline c_real32_4 floor(const c_real32_4 &v) {
	//return _mm_floor_ps(v);
    alignas(SSE_ALIGNMENT) real32 val[4];
    _mm_store_ps(val, v);
    val[0] = std::floor(val[0]);
    val[1] = std::floor(val[1]);
    val[2] = std::floor(val[2]);
    val[3] = std::floor(val[3]);
    return c_real32_4(val);
}

inline c_real32_4 ceil(const c_real32_4 &v) {
	//return _mm_ceil_ps(v);
    alignas(SSE_ALIGNMENT) real32 val[4];
    _mm_store_ps(val, v);
    val[0] = std::ceil(val[0]);
    val[1] = std::ceil(val[1]);
    val[2] = std::ceil(val[2]);
    val[3] = std::ceil(val[3]);
    return c_real32_4(val);
}

inline c_real32_4 round(const c_real32_4 &v) {
	//return _mm_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    alignas(SSE_ALIGNMENT) real32 val[4];
    _mm_store_ps(val, v);
    val[0] = std::round(val[0]);
    val[1] = std::round(val[1]);
    val[2] = std::round(val[2]);
    val[3] = std::round(val[3]);
    return c_real32_4(val);
}

inline c_real32_4 min(const c_real32_4 &a, const c_real32_4 &b) {
	return _mm_min_ps(a, b);
}

inline c_real32_4 max(const c_real32_4 &a, const c_real32_4 &b) {
	return _mm_max_ps(a, b);
}

inline c_real32_4 log(const c_real32_4 &v) {
	return log_ps(v);
}

inline c_real32_4 exp(const c_real32_4 &v) {
	return exp_ps(v);
}

inline c_real32_4 sqrt(const c_real32_4 &v) {
	return _mm_sqrt_ps(v);
}

inline c_real32_4 pow(const c_real32_4 &a, const c_real32_4 &b) {
	return exp(b * log(a));
}

inline c_real32_4 sin(const c_real32_4 &v) {
	return sin_ps(v);
}

inline c_real32_4 cos(const c_real32_4 &v) {
	return cos_ps(v);
}

#else // PREDEFINED(SSE_ENABLED)

#error Implement non-SSE operations

#endif // PREDEFINED(SSE_ENABLED)

#endif // WAVELANG_SSE_H__
