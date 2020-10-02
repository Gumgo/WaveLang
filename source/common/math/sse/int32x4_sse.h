#pragma once

#include "common/common.h"
#include "common/math/int32x4.h"
#include "common/math/real32x4.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_128_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX_ENABLED)

inline int32x4::int32x4() {
}

inline int32x4::int32x4(int32 v)
	: m_value(_mm_set1_epi32(v)) {
}

inline int32x4::int32x4(int32 a, int32 b, int32 c, int32 d)
	: m_value(_mm_set_epi32(d, c, b, a)) {
}

inline int32x4::int32x4(const int32 *ptr) {
	load(ptr);
}

inline int32x4::int32x4(const t_simd_int32x4 &v)
	: m_value(v) {
}

inline int32x4::int32x4(const int32x4 &v)
	: m_value(v) {
}

inline void int32x4::load(const int32 *ptr) {
	wl_assert(is_pointer_aligned(ptr, k_simd_128_alignment));
	m_value = _mm_load_si128(reinterpret_cast<const t_simd_int32x4 *>(ptr));
}

inline void int32x4::store(int32 *ptr) const {
	wl_assert(is_pointer_aligned(ptr, k_simd_128_alignment));
	_mm_store_si128(reinterpret_cast<t_simd_int32x4 *>(ptr), m_value);
}

inline int32x4 &int32x4::operator=(const t_simd_int32x4 &v) {
	m_value = v;
	return *this;
}

inline int32x4 &int32x4::operator=(const int32x4 &v) {
	m_value = v;
	return *this;
}

inline int32x4::operator t_simd_int32x4() const {
	return m_value;
}

inline int32x4::operator real32x4() const {
	return _mm_cvtepi32_ps(m_value);
}

inline int32x4 int32x4::sum_elements() const {
	// $TODO $SIMD:
	// https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction
	__m128i x = _mm_hadd_epi32(m_value, m_value);
	x = _mm_hadd_epi32(x, x);
	return x;
}

inline int32 int32x4::first_element() const {
	return _mm_cvtsi128_si32(m_value);
}

inline int32x4 operator+(const int32x4 &v) {
	return v;
}

inline int32x4 operator-(const int32x4 &v) {
	return _mm_sub_epi32(_mm_set1_epi32(0), v);
}

inline int32x4 operator~(const int32x4 &v) {
	return _mm_xor_si128(v, _mm_cmpeq_epi32(v, v)); // cmpeq(v, v) sets all bits to 1
}

inline int32x4 operator+(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_add_epi32(lhs, rhs);
}

inline int32x4 operator-(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_sub_epi32(lhs, rhs);
}

inline int32x4 operator*(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_mullo_epi32(lhs, rhs);
}

inline int32x4 operator&(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_and_si128(lhs, rhs);
}

inline int32x4 operator|(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_or_si128(lhs, rhs);
}

inline int32x4 operator^(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_xor_si128(lhs, rhs);
}

inline int32x4 operator<<(const int32x4 &lhs, int32 rhs) {
	return _mm_slli_epi32(lhs, rhs);
}

inline int32x4 operator<<(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_sllv_epi32(lhs, rhs);
}

inline int32x4 operator>>(const int32x4 &lhs, int32 rhs) {
	return _mm_srai_epi32(lhs, rhs);
}

inline int32x4 operator>>(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_srav_epi32(lhs, rhs);
}

inline int32x4 int32x4::shift_right_unsigned(int32 rhs) const {
	return _mm_srli_epi32(m_value, rhs);
}

inline int32x4 int32x4::shift_right_unsigned(const int32x4 &rhs) const {
	return _mm_srlv_epi32(m_value, rhs);
}

inline int32x4 operator==(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_cmpeq_epi32(lhs, rhs);
}

inline int32x4 operator!=(const int32x4 &lhs, const int32x4 &rhs) {
	return ~int32x4(_mm_cmpeq_epi32(lhs, rhs));
}

inline int32x4 operator>(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_cmpgt_epi32(lhs, rhs);
}

inline int32x4 operator<(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_cmplt_epi32(lhs, rhs);
}

inline int32x4 operator>=(const int32x4 &lhs, const int32x4 &rhs) {
	return ~int32x4(_mm_cmplt_epi32(lhs, rhs));
}

inline int32x4 operator<=(const int32x4 &lhs, const int32x4 &rhs) {
	return ~int32x4(_mm_cmpgt_epi32(lhs, rhs));
}

inline int32x4 greater_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return ~less_equal_unsigned(lhs, rhs);
}

inline int32x4 less_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return ~greater_equal_unsigned(lhs, rhs);
}

inline int32x4 greater_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_cmpeq_epi32(lhs, _mm_max_epu32(lhs, rhs)); // (a == max(a,b)) == (a >= b)
}

inline int32x4 less_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs) {
	return _mm_cmpeq_epi32(lhs, _mm_min_epu32(lhs, rhs)); // (a == min(a,b)) == (a <= b)
}

inline int32x4 abs(const int32x4 &v) {
	return _mm_abs_epi32(v);
}

inline int32x4 min(const int32x4 &a, const int32x4 &b) {
	return _mm_min_epi32(a, b);
}

inline int32x4 max(const int32x4 &a, const int32x4 &b) {
	return _mm_max_epi32(a, b);
}

inline int32x4 min_unsigned(const int32x4 &a, const int32x4 &b) {
	return _mm_min_epu32(a, b);
}

inline int32x4 max_unsigned(const int32x4 &a, const int32x4 &b) {
	return _mm_max_epu32(a, b);
}

template<> inline real32x4 reinterpret_bits(const int32x4 &v) {
	return _mm_castsi128_ps(v);
}

inline int32 mask_from_msb(const int32x4 &v) {
	return _mm_movemask_ps(reinterpret_bits<real32x4>(v));
}

inline bool all_true(const int32x4 &v) {
	return mask_from_msb(v) == 0xf; // Alternative: _mm_test_all_ones
}

inline bool all_false(const int32x4 &v) {
	return mask_from_msb(v) == 0x0; // Alternative: _mm_test_all_zeros
}

inline bool any_true(const int32x4 &v) {
	return mask_from_msb(v) != 0x0;
}

inline bool any_false(const int32x4 &v) {
	return mask_from_msb(v) != 0xf;
}

#endif // IS_TRUE(SIMD_128_ENABLED) && IS_TRUE(SIMD_IMPLEMENTATION_AVX_ENABLED)
