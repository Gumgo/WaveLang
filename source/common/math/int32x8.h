#pragma once

#include "common/common.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_256_ENABLED)

class real32x8;

class ALIGNAS_SIMD_256 int32x8 {
public:
	using t_element = int32;
	static constexpr size_t k_element_count = 8;
	static constexpr size_t k_element_size = sizeof(t_element);
	static constexpr size_t k_element_size_bits = sizeof(t_element) * 8;

	inline int32x8();
	inline int32x8(int32 v);
	inline int32x8(int32 a, int32 b, int32 c, int32 d, int32 e, int32 f, int32 g, int32 h);
	inline int32x8(const int32 *ptr);
	inline int32x8(const t_simd_int32x8 &v);
	inline int32x8(const int32x8 &v);

	inline void load(const int32 *ptr);
	inline void store(int32 *ptr) const;

	inline int32x8 &operator=(const t_simd_int32x8 &v);
	inline int32x8 &operator=(const int32x8 &v);

	inline int32x8 &operator+=(const int32x8 &rhs);
	inline int32x8 &operator-=(const int32x8 &rhs);
	inline int32x8 &operator*=(const int32x8 &rhs);
	inline int32x8 &operator&=(const int32x8 &rhs);
	inline int32x8 &operator|=(const int32x8 &rhs);
	inline int32x8 &operator^=(const int32x8 &rhs);
	inline int32x8 &operator<<=(int32 rhs);
	inline int32x8 &operator<<=(const int32x8 & rhs);
	inline int32x8 &operator>>=(int32 rhs);
	inline int32x8 &operator>>=(const int32x8 & rhs);

	inline operator t_simd_int32x8() const;
	inline operator real32x8() const;
	inline int32x8 sum_elements() const;
	inline int32 first_element() const;

	// Replacement for >>> operator
	inline int32x8 shift_right_unsigned(int32 rhs) const;
	inline int32x8 shift_right_unsigned(const int32x8 &rhs) const;

private:
	t_simd_int32x8 m_value;
};

// Unary operators
inline int32x8 operator+(const int32x8 &v);
inline int32x8 operator-(const int32x8 &v);
inline int32x8 operator~(const int32x8 &v);

// Binary operators
inline int32x8 operator+(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator-(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator*(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator&(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator|(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator^(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator<<(const int32x8 &lhs, int32 rhs);
inline int32x8 operator<<(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator>>(const int32x8 &lhs, int32 rhs);
inline int32x8 operator>>(const int32x8 &lhs, const int32x8 &rhs);

inline int32x8 &int32x8::operator+=(const int32x8 &rhs) { *this = *this + rhs; return *this; }
inline int32x8 &int32x8::operator-=(const int32x8 &rhs) { *this = *this - rhs; return *this; }
inline int32x8 &int32x8::operator*=(const int32x8 &rhs) { *this = *this * rhs; return *this; }
inline int32x8 &int32x8::operator&=(const int32x8 &rhs) { *this = *this & rhs; return *this; }
inline int32x8 &int32x8::operator|=(const int32x8 &rhs) { *this = *this | rhs; return *this; }
inline int32x8 &int32x8::operator^=(const int32x8 &rhs) { *this = *this ^ rhs; return *this; }
inline int32x8 &int32x8::operator<<=(int32 rhs) { *this = *this << rhs; return *this; }
inline int32x8 &int32x8::operator<<=(const int32x8 &rhs) { *this = *this << rhs; return *this; }
inline int32x8 &int32x8::operator>>=(int32 rhs) { *this = *this >> rhs; return *this; }
inline int32x8 &int32x8::operator>>=(const int32x8 &rhs) { *this = *this >> rhs; return *this; }

inline int32x8 operator==(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator!=(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator>(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator<(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator>=(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 operator<=(const int32x8 &lhs, const int32x8 &rhs);

inline int32x8 greater_unsigned(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 less_unsigned(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 greater_equal_unsigned(const int32x8 &lhs, const int32x8 &rhs);
inline int32x8 less_equal_unsigned(const int32x8 &lhs, const int32x8 &rhs);

// Arithmetic/utility
inline int32x8 abs(const int32x8 &v);
inline int32x8 min(const int32x8 &a, const int32x8 &b);
inline int32x8 max(const int32x8 &a, const int32x8 &b);
inline int32x8 min_unsigned(const int32x8 &a, const int32x8 &b);
inline int32x8 max_unsigned(const int32x8 &a, const int32x8 &b);

// Casting
template<> inline real32x8 reinterpret_bits(const int32x8 &v);

// Masking and bool testing
inline int32 mask_from_msb(const int32x8 &v);
inline bool all_true(const int32x8 &v);
inline bool all_false(const int32x8 &v);
inline bool any_true(const int32x8 &v);
inline bool any_false(const int32x8 &v);

#endif // IS_TRUE(SIMD_256_ENABLED)
