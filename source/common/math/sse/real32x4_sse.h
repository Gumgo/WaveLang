#pragma once

#include "common/common.h"
#include "common/math/int32x4.h"
#include "common/math/real32x4.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_128_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX_ENABLED)

inline real32x4::real32x4() {}

inline real32x4::real32x4(real32 v)
	: m_value(_mm_set1_ps(v)) {}

inline real32x4::real32x4(real32 a, real32 b, real32 c, real32 d)
	: m_value(_mm_set_ps(d, c, b, a)) {}

inline real32x4::real32x4(const real32 *ptr) {
	load(ptr);
}

inline real32x4::real32x4(const t_simd_real32x4 &v)
	: m_value(v) {}

inline real32x4::real32x4(const real32x4 &v)
	: m_value(v.m_value) {}

inline void real32x4::load(const real32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_128_alignment));
	m_value = _mm_load_ps(ptr);
}

inline void real32x4::load_unaligned(const real32 *ptr) {
	m_value = _mm_loadu_ps(ptr);
}

inline void real32x4::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_128_alignment));
	_mm_store_ps(ptr, m_value);
}

inline void real32x4::store_unaligned(real32 *ptr) const {
	_mm_storeu_ps(ptr, m_value);
}

inline real32x4 &real32x4::real32x4::operator=(const t_simd_real32x4 &v) {
	m_value = v;
	return *this;
}

inline real32x4 &real32x4::operator=(const real32x4 &v) {
	m_value = v.m_value;
	return *this;
}

inline real32x4::operator t_simd_real32x4() const {
	return m_value;
}

inline real32x4::operator int32x4() const {
	return _mm_cvtps_epi32(m_value);
}

inline real32x4 real32x4::sum_elements() const {
	// More efficient alternative to hadd:
	__m128 shuffled = _mm_movehdup_ps(m_value);	// 1, 1, 3, 3
	__m128 sum = _mm_add_ps(m_value, shuffled);	// 0+1, 1+1, 2+3, 3+3
	shuffled = _mm_movehl_ps(shuffled, sum);	// 2+3, 2+3, 3+3, 3+3
	sum = _mm_add_ss(sum, shuffled);			// 0+1+2+3, x, x, x
	return _mm_cvtss_f32(sum);
}

inline real32 real32x4::first_element() const {
	return _mm_cvtss_f32(m_value);
}

inline real32x4 operator+(const real32x4 &v) {
	return v;
}

inline real32x4 operator-(const real32x4 &v) {
	return _mm_sub_ps(_mm_set1_ps(0.0f), v);
}

inline real32x4 operator+(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_add_ps(lhs, rhs);
}

inline real32x4 operator-(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_sub_ps(lhs, rhs);
}

inline real32x4 operator*(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_mul_ps(lhs, rhs);
}

inline real32x4 operator/(const real32x4 &lhs, const real32x4 &rhs) {
	return _mm_div_ps(lhs, rhs);
}

inline real32x4 operator%(const real32x4 &lhs, const real32x4 &rhs) {
	// Rounds toward 0
	__m128 c = _mm_div_ps(lhs, rhs);
	__m128i i = _mm_cvttps_epi32(c);
	__m128 c_trunc = _mm_cvtepi32_ps(i);
	__m128 base = _mm_mul_ps(c_trunc, rhs);
	return _mm_sub_ps(lhs, base);
}

inline int32x4 operator==(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpeq_ps(lhs, rhs)));
}

inline int32x4 operator!=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpneq_ps(lhs, rhs)));
}

inline int32x4 operator>(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpgt_ps(lhs, rhs)));
}

inline int32x4 operator<(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmplt_ps(lhs, rhs)));
}

inline int32x4 operator>=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmpge_ps(lhs, rhs)));
}

inline int32x4 operator<=(const real32x4 &lhs, const real32x4 &rhs) {
	return reinterpret_bits<int32x4>(real32x4(_mm_cmple_ps(lhs, rhs)));
}

inline real32x4 abs(const real32x4 &v) {
	// Mask off the sign bit for fast abs
	const __m128 k_sign_mask = _mm_set1_ps(-0.0f);
	return _mm_andnot_ps(k_sign_mask, v);
}

// $TODO do we want to come up with an SSE3 version of round/floor/ceil?

inline real32x4 floor(const real32x4 &v) {
	return _mm_floor_ps(v);
}

inline real32x4 ceil(const real32x4 &v) {
	return _mm_ceil_ps(v);
}

inline real32x4 round(const real32x4 &v) {
	return _mm_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

inline real32x4 min(const real32x4 &a, const real32x4 &b) {
	return _mm_min_ps(a, b);
}

inline real32x4 max(const real32x4 &a, const real32x4 &b) {
	return _mm_max_ps(a, b);
}

inline real32x4 log(const real32x4 &v) {
	return log_ps(v);
}

inline real32x4 exp(const real32x4 &v) {
	return exp_ps(v);
}

inline real32x4 sqrt(const real32x4 &v) {
	return _mm_sqrt_ps(v);
}

inline real32x4 pow(const real32x4 &a, const real32x4 &b) {
	return exp(b * log(a));
}

inline real32x4 sin(const real32x4 &v) {
	return sin_ps(v);
}

inline real32x4 cos(const real32x4 &v) {
	return cos_ps(v);
}

inline void sincos(const real32x4 &v, real32x4 &sin_out, real32x4 &cos_out) {
	sincos_ps(v, reinterpret_cast<__m128 *>(&sin_out), reinterpret_cast<__m128 *>(&cos_out));
}

template<> inline int32x4 reinterpret_bits(const real32x4 &v) {
	return _mm_castps_si128(v);
}

#endif // IS_TRUE(SIMD_128_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX_ENABLED)
