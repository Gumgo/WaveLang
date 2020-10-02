#pragma once

#include "common/common.h"
#include "common/math/simd.h"

#if IS_TRUE(SIMD_256_ENABLED)

class int32x8;

class ALIGNAS_SIMD_256 real32x8 {
public:
	using t_element = real32;
	static constexpr size_t k_element_count = 8;
	static constexpr size_t k_element_size = sizeof(t_element);
	static constexpr size_t k_element_size_bits = sizeof(t_element) * 8;

	inline real32x8();
	inline real32x8(real32 v);
	inline real32x8(real32 a, real32 b, real32 c, real32 d, real32 e, real32 f, real32 g, real32 h);
	inline real32x8(const real32 *ptr);
	inline real32x8(const t_simd_real32x8 &v);
	inline real32x8(const real32x8 &v);

	inline void load(const real32 *ptr);
	inline void load_unaligned(const real32 *ptr);
	inline void store(real32 *ptr) const;
	inline void store_unaligned(real32 *ptr) const;

	inline real32x8 &operator=(const t_simd_real32x8 &v);
	inline real32x8 &operator=(const real32x8 &v);

	inline real32x8 &operator+=(const real32x8 &rhs);
	inline real32x8 &operator-=(const real32x8 &rhs);
	inline real32x8 &operator*=(const real32x8 &rhs);
	inline real32x8 &operator/=(const real32x8 &rhs);
	inline real32x8 &operator%=(const real32x8 &rhs);

	inline operator t_simd_real32x8() const;
	inline operator int32x8() const;
	inline real32x8 sum_elements() const;
	inline real32 first_element() const;

private:
	t_simd_real32x8 m_value;
};

// Unary operators
inline real32x8 operator+(const real32x8 &v);
inline real32x8 operator-(const real32x8 &v);

// Binary operators
inline real32x8 operator+(const real32x8 &lhs, const real32x8 &rhs);
inline real32x8 operator-(const real32x8 &lhs, const real32x8 &rhs);
inline real32x8 operator*(const real32x8 &lhs, const real32x8 &rhs);
inline real32x8 operator/(const real32x8 &lhs, const real32x8 &rhs);
inline real32x8 operator%(const real32x8 &lhs, const real32x8 &rhs);

inline real32x8 &real32x8::operator+=(const real32x8 &rhs) { *this = *this + rhs; return *this; }
inline real32x8 &real32x8::operator-=(const real32x8 &rhs) { *this = *this - rhs; return *this; }
inline real32x8 &real32x8::operator*=(const real32x8 &rhs) { *this = *this * rhs; return *this; }
inline real32x8 &real32x8::operator/=(const real32x8 &rhs) { *this = *this / rhs; return *this; }
inline real32x8 &real32x8::operator%=(const real32x8 &rhs) { *this = *this % rhs; return *this; }

inline int32x8 operator==(const real32x8 &lhs, const real32x8 &rhs);
inline int32x8 operator!=(const real32x8 &lhs, const real32x8 &rhs);
inline int32x8 operator>(const real32x8 &lhs, const real32x8 &rhs);
inline int32x8 operator<(const real32x8 &lhs, const real32x8 &rhs);
inline int32x8 operator>=(const real32x8 &lhs, const real32x8 &rhs);
inline int32x8 operator<=(const real32x8 &lhs, const real32x8 &rhs);

// Arithmetic/utility
inline real32x8 abs(const real32x8 &v);
inline real32x8 floor(const real32x8 &v);
inline real32x8 ceil(const real32x8 &v);
inline real32x8 round(const real32x8 &v);
inline real32x8 min(const real32x8 &a, const real32x8 &b);
inline real32x8 max(const real32x8 &a, const real32x8 &b);
inline real32x8 log(const real32x8 &v);
inline real32x8 exp(const real32x8 &v);
inline real32x8 sqrt(const real32x8 &v);
inline real32x8 pow(const real32x8 &a, const real32x8 &b);
inline real32x8 sin(const real32x8 &v);
inline real32x8 cos(const real32x8 &v);
inline void sincos(const real32x8 &v, real32x8 &sin_out, real32x8 &cos_out);

// Casting
template<> inline int32x8 reinterpret_bits(const real32x8 &v);

#endif // IS_TRUE(SIMD_256_ENABLED)
