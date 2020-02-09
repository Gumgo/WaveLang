#pragma once

#include "common/common.h"

#include "engine/math/simd.h"

class real32x4;

class ALIGNAS_SIMD int32x4 {
public:
	using t_element = int32;

	inline int32x4();
	inline int32x4(int32 v);
	inline int32x4(int32 a, int32 b, int32 c, int32 d);
	inline int32x4(const int32 *ptr);
	inline int32x4(const t_simd_int32 &v);
	inline int32x4(const int32x4 &v);

	inline void load(const int32 *ptr);
	inline void store(int32 *ptr) const;

	inline int32x4 &operator=(const t_simd_int32 &v);
	inline int32x4 &operator=(const int32x4 &v);
	// $TODO +=, etc.

	inline operator t_simd_int32() const;
	inline operator real32x4() const;

	// Replacement for >>> operator
	inline int32x4 shift_right_unsigned(int32 rhs) const;
	inline int32x4 shift_right_unsigned(const int32x4 &rhs) const;

private:
	t_simd_int32 m_value;
};

// Unary operators
inline int32x4 operator+(const int32x4 &v);
inline int32x4 operator-(const int32x4 &v);
inline int32x4 operator~(const int32x4 &v);

// Binary operators
inline int32x4 operator+(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator-(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator&(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator|(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator^(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator<<(const int32x4 &lhs, int32 rhs);
inline int32x4 operator<<(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator>>(const int32x4 &lhs, int32 rhs);
inline int32x4 operator>>(const int32x4 &lhs, const int32x4 &rhs);

inline int32x4 operator==(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator!=(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator>(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator<(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator>=(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator<=(const int32x4 &lhs, const int32x4 &rhs);

inline int32x4 greater_unsigned(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 less_unsigned(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 greater_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 less_equal_unsigned(const int32x4 &lhs, const int32x4 &rhs);

// Arithmetic/utility
inline int32x4 abs(const int32x4 &v);
inline int32x4 min(const int32x4 &a, const int32x4 &b);
inline int32x4 max(const int32x4 &a, const int32x4 &b);
inline int32x4 min_unsigned(const int32x4 &a, const int32x4 &b);
inline int32x4 max_unsigned(const int32x4 &a, const int32x4 &b);

// Casting
template<> inline real32x4 reinterpret_bits(const int32x4 &v);

// Masking
inline int32 mask_from_msb(const int32x4 &v);

// Shuffle
inline int32x4 single_element(const int32x4 &v, int32 pos);

#if IS_TRUE(SIMD_IMPLEMENTATION_SSE_ENABLED)
template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
int32x4 shuffle(const int32x4 &v);
template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
int32x4 shuffle(const int32x4 &a, const int32x4 &b);
#endif // IS_TRUE(SIMD_IMPLEMENTATION_SSE)

// Extract:
// Treats a and b as a contiguous block of 8 values and shifts left by the given amount; returns only the first 4
template<int32 k_shift_amount>
int32x4 extract(const int32x4 &a, const int32x4 &b);

