#pragma once

#include "common/common.h"
#include "common/math/int32x8.h"
#include "common/math/real32x8.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_256_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX2_ENABLED)

inline real32x8::real32x8() {}

inline real32x8::real32x8(real32 v)
	: m_value(_mm256_set1_ps(v)) {}

inline real32x8::real32x8(real32 a, real32 b, real32 c, real32 d, real32 e, real32 f, real32 g, real32 h)
	: m_value(_mm256_set_ps(h, g, f, e, d, c, b, a)) {}

inline real32x8::real32x8(const real32 *ptr) {
	load(ptr);
}

inline real32x8::real32x8(const t_simd_real32x8 &v)
	: m_value(v) {
}

inline real32x8::real32x8(const real32x8 &v)
	: m_value(v.m_value) {}

inline void real32x8::load(const real32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_256_alignment));
	m_value = _mm256_load_ps(ptr);
}

inline void real32x8::load_unaligned(const real32 *ptr) {
	m_value = _mm256_loadu_ps(ptr);
}

inline void real32x8::store(real32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_256_alignment));
	_mm256_store_ps(ptr, m_value);
}

inline void real32x8::store_unaligned(real32 *ptr) const {
	_mm256_storeu_ps(ptr, m_value);
}

inline real32x8 &real32x8::real32x8::operator=(const t_simd_real32x8 &v) {
	m_value = v;
	return *this;
}

inline real32x8 &real32x8::operator=(const real32x8 &v) {
	m_value = v.m_value;
	return *this;
}

inline real32x8::operator t_simd_real32x8() const {
	return m_value;
}

inline real32x8::operator int32x8() const {
	return _mm256_cvtps_epi32(m_value);
}

inline real32x8 real32x8::sum_elements() const {
	// More efficient alternative to hadd:
	__m128 low = _mm256_castps256_ps128(m_value);		// 0, 1, 2, 3
	__m128 high = _mm256_extractf128_ps(m_value, 1);	// 4, 5, 6, 7
	__m128 sum = _mm_add_ps(low, high);					// 0+4, 1+5, 2+6, 3+7
	__m128 shuffled = _mm_movehdup_ps(sum);				// 1+5, 1+5, 3+7, 3+7
	sum = _mm_add_ps(sum, shuffled);					// 0+1+4+5, 1+1+5+5, 2+3+6+7, 3+3+7+7
	shuffled = _mm_movehl_ps(shuffled, sum);			// 2+3+6+7, 2+3+6+7, 3+3+7+7, 3+3+7+7
	sum = _mm_add_ss(sum, shuffled);					// 0+1+2+3+4+5+6+7, x, x, x
	return _mm_cvtss_f32(sum);
}

inline real32 real32x8::first_element() const {
	//return _mm256_cvtss_f32(m_value); // Doesn't seem to exist on clang
	return _mm_cvtss_f32(_mm256_castps256_ps128(m_value));
}

inline real32x8 operator+(const real32x8 &v) {
	return v;
}

inline real32x8 operator-(const real32x8 &v) {
	return _mm256_sub_ps(_mm256_set1_ps(0.0f), v);
}

inline real32x8 operator+(const real32x8 &lhs, const real32x8 &rhs) {
	return _mm256_add_ps(lhs, rhs);
}

inline real32x8 operator-(const real32x8 &lhs, const real32x8 &rhs) {
	return _mm256_sub_ps(lhs, rhs);
}

inline real32x8 operator*(const real32x8 &lhs, const real32x8 &rhs) {
	return _mm256_mul_ps(lhs, rhs);
}

inline real32x8 operator/(const real32x8 &lhs, const real32x8 &rhs) {
	return _mm256_div_ps(lhs, rhs);
}

inline real32x8 operator%(const real32x8 &lhs, const real32x8 &rhs) {
	// Rounds toward 0
	__m256 c = _mm256_div_ps(lhs, rhs);
	__m256i i = _mm256_cvttps_epi32(c);
	__m256 c_trunc = _mm256_cvtepi32_ps(i);
	__m256 base = _mm256_mul_ps(c_trunc, rhs);
	return _mm256_sub_ps(lhs, base);
}

// $TODO $SIMD do we want ordered or unordered comparisons?

inline int32x8 operator==(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_EQ_OQ)));
}

inline int32x8 operator!=(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_NEQ_OQ)));
}

inline int32x8 operator>(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_GT_OQ)));
}

inline int32x8 operator<(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_LT_OQ)));
}

inline int32x8 operator>=(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_GE_OQ)));
}

inline int32x8 operator<=(const real32x8 &lhs, const real32x8 &rhs) {
	return reinterpret_bits<int32x8>(real32x8(_mm256_cmp_ps(lhs, rhs, _CMP_LE_OQ)));
}

inline real32x8 abs(const real32x8 &v) {
	// Mask off the sign bit for fast abs
	const __m256 k_sign_mask = _mm256_set1_ps(-0.0f);
	return _mm256_andnot_ps(k_sign_mask, v);
}

inline real32x8 floor(const real32x8 &v) {
	return _mm256_floor_ps(v);
}

inline real32x8 ceil(const real32x8 &v) {
	return _mm256_ceil_ps(v);
}

inline real32x8 round(const real32x8 &v) {
	return _mm256_round_ps(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

inline real32x8 min(const real32x8 &a, const real32x8 &b) {
	return _mm256_min_ps(a, b);
}

inline real32x8 max(const real32x8 &a, const real32x8 &b) {
	return _mm256_max_ps(a, b);
}

inline real32x8 log(const real32x8 &v) {
	return log256_ps(v);
}

inline real32x8 exp(const real32x8 &v) {
	return exp256_ps(v);
}

inline real32x8 sqrt(const real32x8 &v) {
	return _mm256_sqrt_ps(v);
}

inline real32x8 pow(const real32x8 &a, const real32x8 &b) {
	return exp(b * log(a));
}

inline real32x8 sin(const real32x8 &v) {
	return sin256_ps(v);
}

inline real32x8 cos(const real32x8 &v) {
	return cos256_ps(v);
}

inline void sincos(const real32x8 &v, real32x8 &sin_out, real32x8 &cos_out) {
	sincos256_ps(v, reinterpret_cast<__m256 *>(&sin_out), reinterpret_cast<__m256 *>(&cos_out));
}

template<> inline int32x8 reinterpret_bits(const real32x8 &v) {
	return _mm256_castps_si256(v);
}

#endif // IS_TRUE(SIMD_256_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX2_ENABLED)
