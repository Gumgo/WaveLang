#pragma once

#include "common/common.h"
#include "common/math/int32x8.h"
#include "common/math/real32x8.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_256_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX2_ENABLED)

inline int32x8::int32x8() {
}

inline int32x8::int32x8(int32 v)
	: m_value(_mm256_set1_epi32(v)) {
}

inline int32x8::int32x8(int32 a, int32 b, int32 c, int32 d, int32 e, int32 f, int32 g, int32 h)
	: m_value(_mm256_set_epi32(h, g, f, e, d, c, b, a)) {
}

inline int32x8::int32x8(const int32 *ptr) {
	load(ptr);
}

inline int32x8::int32x8(const t_simd_int32x8 &v)
	: m_value(v) {
}

inline int32x8::int32x8(const int32x8 &v)
	: m_value(v) {
}

inline void int32x8::load(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_256_alignment));
	m_value = _mm256_load_si256(reinterpret_cast<const t_simd_int32x8 *>(ptr));
}

inline void int32x8::store(int32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_256_alignment));
	_mm256_store_si256(reinterpret_cast<t_simd_int32x8 *>(ptr), m_value);
}

inline int32x8 &int32x8::operator=(const t_simd_int32x8 &v) {
	m_value = v;
	return *this;
}

inline int32x8 &int32x8::operator=(const int32x8 &v) {
	m_value = v;
	return *this;
}

inline int32x8::operator t_simd_int32x8() const {
	return m_value;
}

inline int32x8::operator real32x8() const {
	return _mm256_cvtepi32_ps(m_value);
}

inline int32x8 int32x8::sum_elements() const {
	// $TODO $SIMD:
	// https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction
	__m256i x = _mm256_permute2f128_si256(m_value, m_value, 1);
	x = _mm256_add_epi32(m_value, x);
	x = _mm256_hadd_epi32(x, x);
	return _mm256_hadd_epi32(x, x);
}

inline int32 int32x8::first_element() const {
	return _mm256_cvtsi256_si32(m_value);
}

inline int32x8 operator+(const int32x8 &v) {
	return v;
}

inline int32x8 operator-(const int32x8 &v) {
	return _mm256_sub_epi32(_mm256_set1_epi32(0), v);
}

inline int32x8 operator~(const int32x8 &v) {
	return _mm256_xor_si256(v, _mm256_cmpeq_epi32(v, v)); // cmpeq(v, v) sets all bits to 1
}

inline int32x8 operator+(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_add_epi32(lhs, rhs);
}

inline int32x8 operator-(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_sub_epi32(lhs, rhs);
}

inline int32x8 operator*(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_mullo_epi32(lhs, rhs);
}

inline int32x8 operator&(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_and_si256(lhs, rhs);
}

inline int32x8 operator|(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_or_si256(lhs, rhs);
}

inline int32x8 operator^(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_xor_si256(lhs, rhs);
}

inline int32x8 operator<<(const int32x8 &lhs, int32 rhs) {
	return _mm256_slli_epi32(lhs, rhs);
}

inline int32x8 operator<<(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_sllv_epi32(lhs, rhs);
}

inline int32x8 operator>>(const int32x8 &lhs, int32 rhs) {
	return _mm256_srai_epi32(lhs, rhs);
}

inline int32x8 operator>>(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_srav_epi32(lhs, rhs);
}

inline int32x8 int32x8::shift_right_unsigned(int32 rhs) const {
	return _mm256_srli_epi32(m_value, rhs);
}

inline int32x8 int32x8::shift_right_unsigned(const int32x8 &rhs) const {
	return _mm256_srlv_epi32(m_value, rhs);
}

inline int32x8 operator==(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_cmpeq_epi32(lhs, rhs);
}

inline int32x8 operator!=(const int32x8 &lhs, const int32x8 &rhs) {
	return ~int32x8(_mm256_cmpeq_epi32(lhs, rhs));
}

inline int32x8 operator>(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_cmpgt_epi32(lhs, rhs);
}

inline int32x8 operator<(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_cmpgt_epi32(rhs, lhs);
}

inline int32x8 operator>=(const int32x8 &lhs, const int32x8 &rhs) {
	return ~int32x8(_mm256_cmpgt_epi32(rhs, lhs));
}

inline int32x8 operator<=(const int32x8 &lhs, const int32x8 &rhs) {
	return ~int32x8(_mm256_cmpgt_epi32(lhs, rhs));
}

inline int32x8 greater_unsigned(const int32x8 &lhs, const int32x8 &rhs) {
	return ~less_equal_unsigned(lhs, rhs);
}

inline int32x8 less_unsigned(const int32x8 &lhs, const int32x8 &rhs) {
	return ~greater_equal_unsigned(lhs, rhs);
}

inline int32x8 greater_equal_unsigned(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_cmpeq_epi32(lhs, _mm256_max_epu32(lhs, rhs)); // (a == max(a,b)) == (a >= b)
}

inline int32x8 less_equal_unsigned(const int32x8 &lhs, const int32x8 &rhs) {
	return _mm256_cmpeq_epi32(lhs, _mm256_min_epu32(lhs, rhs)); // (a == min(a,b)) == (a <= b)
}

inline int32x8 abs(const int32x8 &v) {
	return _mm256_abs_epi32(v);
}

inline int32x8 min(const int32x8 &a, const int32x8 &b) {
	return _mm256_min_epi32(a, b);
}

inline int32x8 max(const int32x8 &a, const int32x8 &b) {
	return _mm256_max_epi32(a, b);
}

inline int32x8 min_unsigned(const int32x8 &a, const int32x8 &b) {
	return _mm256_min_epu32(a, b);
}

inline int32x8 max_unsigned(const int32x8 &a, const int32x8 &b) {
	return _mm256_max_epu32(a, b);
}

template<> inline real32x8 reinterpret_bits(const int32x8 &v) {
	return _mm256_castsi256_ps(v);
}

inline int32 mask_from_msb(const int32x8 &v) {
	return _mm256_movemask_ps(reinterpret_bits<real32x8>(v));
}

inline bool all_true(const int32x8 &v) {
	return mask_from_msb(v) == 0xff; // Alternative: _mm256_test_all_ones
}

inline bool all_false(const int32x8 &v) {
	return mask_from_msb(v) == 0x00; // Alternative: _mm256_test_all_zeros
}

inline bool any_true(const int32x8 &v) {
	return mask_from_msb(v) != 0x00;
}

inline bool any_false(const int32x8 &v) {
	return mask_from_msb(v) != 0xff;
}

#endif // IS_TRUE(SIMD_256_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX2_ENABLED)
