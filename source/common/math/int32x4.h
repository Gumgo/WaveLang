#pragma once

#include "common/common.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_128_ENABLED)

class real32x4;

class ALIGNAS_SIMD_128 int32x4 {
public:
	using t_element = int32;
	static constexpr size_t k_element_count = 4;
	static constexpr size_t k_element_size = sizeof(t_element);
	static constexpr size_t k_element_size_bits = sizeof(t_element) * 8;

	inline int32x4();
	inline int32x4(int32 v);
	inline int32x4(int32 a, int32 b, int32 c, int32 d);
	inline int32x4(const int32 *ptr);
	inline int32x4(const t_simd_int32x4 &v);
	inline int32x4(const int32x4 &v);

	inline void load(const int32 *ptr);
	inline void store(int32 *ptr) const;

	inline int32x4 &operator=(const t_simd_int32x4 &v);
	inline int32x4 &operator=(const int32x4 &v);

	inline int32x4 &operator+=(const int32x4 &rhs);
	inline int32x4 &operator-=(const int32x4 &rhs);
	inline int32x4 &operator*=(const int32x4 &rhs);
	inline int32x4 &operator&=(const int32x4 &rhs);
	inline int32x4 &operator|=(const int32x4 &rhs);
	inline int32x4 &operator^=(const int32x4 &rhs);
	inline int32x4 &operator<<=(int32 rhs);
	inline int32x4 &operator<<=(const int32x4 & rhs);
	inline int32x4 &operator>>=(int32 rhs);
	inline int32x4 &operator>>=(const int32x4 & rhs);

	inline operator t_simd_int32x4() const;
	inline operator real32x4() const;
	inline int32x4 sum_elements() const;
	inline int32 first_element() const;

	// Replacement for >>> operator
	inline int32x4 shift_right_unsigned(int32 rhs) const;
	inline int32x4 shift_right_unsigned(const int32x4 &rhs) const;

private:
	t_simd_int32x4 m_value;
};

// Unary operators
inline int32x4 operator+(const int32x4 &v);
inline int32x4 operator-(const int32x4 &v);
inline int32x4 operator~(const int32x4 &v);

// Binary operators
inline int32x4 operator+(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator-(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator*(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator&(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator|(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator^(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator<<(const int32x4 &lhs, int32 rhs);
inline int32x4 operator<<(const int32x4 &lhs, const int32x4 &rhs);
inline int32x4 operator>>(const int32x4 &lhs, int32 rhs);
inline int32x4 operator>>(const int32x4 &lhs, const int32x4 &rhs);

inline int32x4 &int32x4::operator+=(const int32x4 &rhs) { *this = *this + rhs; return *this; }
inline int32x4 &int32x4::operator-=(const int32x4 &rhs) { *this = *this - rhs; return *this; }
inline int32x4 &int32x4::operator*=(const int32x4 &rhs) { *this = *this * rhs; return *this; }
inline int32x4 &int32x4::operator&=(const int32x4 &rhs) { *this = *this & rhs; return *this; }
inline int32x4 &int32x4::operator|=(const int32x4 &rhs) { *this = *this | rhs; return *this; }
inline int32x4 &int32x4::operator^=(const int32x4 &rhs) { *this = *this ^ rhs; return *this; }
inline int32x4 &int32x4::operator<<=(int32 rhs) { *this = *this << rhs; return *this; }
inline int32x4 &int32x4::operator<<=(const int32x4 &rhs) { *this = *this << rhs; return *this; }
inline int32x4 &int32x4::operator>>=(int32 rhs) { *this = *this >> rhs; return *this; }
inline int32x4 &int32x4::operator>>=(const int32x4 &rhs) { *this = *this >> rhs; return *this; }

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

// Masking and bool testing
inline int32 mask_from_msb(const int32x4 &v);
inline bool all_true(const int32x4 &v);
inline bool all_false(const int32x4 &v);
inline bool any_true(const int32x4 &v);
inline bool any_false(const int32x4 &v);

#endif // IS_TRUE(SIMD_128_ENABLED)
