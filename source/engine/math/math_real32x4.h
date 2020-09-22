#pragma once

#include "common/common.h"

#include "engine/math/simd.h"

class int32x4;

class ALIGNAS_SIMD real32x4 {
public:
	using t_element = real32;

	inline real32x4();
	inline real32x4(real32 v);
	inline real32x4(real32 a, real32 b, real32 c, real32 d);
	inline real32x4(const real32 *ptr);
	inline real32x4(const t_simd_real32 &v);
	inline real32x4(const real32x4 &v);

	inline void load(const real32 *ptr);
	inline void load_unaligned(const real32 *ptr);
	inline void store(real32 *ptr) const;
	inline void store_unaligned(real32 *ptr) const;

	inline real32x4 &operator=(const t_simd_real32 &v);
	inline real32x4 &operator=(const real32x4 &v);
	// $TODO +=, etc.

	inline operator t_simd_real32() const;
	inline operator int32x4() const;
	inline real32x4 sum_elements() const;
	inline real32 first_element() const;

private:
	t_simd_real32 m_value;
};

// Unary operators
inline real32x4 operator+(const real32x4 &v);
inline real32x4 operator-(const real32x4 &v);

// Binary operators
inline real32x4 operator+(const real32x4 &lhs, const real32x4 &rhs);
inline real32x4 operator-(const real32x4 &lhs, const real32x4 &rhs);
inline real32x4 operator*(const real32x4 &lhs, const real32x4 &rhs);
inline real32x4 operator/(const real32x4 &lhs, const real32x4 &rhs);
inline real32x4 operator%(const real32x4 &lhs, const real32x4 &rhs);

inline int32x4 operator==(const real32x4 &lhs, const real32x4 &rhs);
inline int32x4 operator!=(const real32x4 &lhs, const real32x4 &rhs);
inline int32x4 operator>(const real32x4 &lhs, const real32x4 &rhs);
inline int32x4 operator<(const real32x4 &lhs, const real32x4 &rhs);
inline int32x4 operator>=(const real32x4 &lhs, const real32x4 &rhs);
inline int32x4 operator<=(const real32x4 &lhs, const real32x4 &rhs);

// Arithmetic/utility
inline real32x4 abs(const real32x4 &v);
inline real32x4 floor(const real32x4 &v);
inline real32x4 ceil(const real32x4 &v);
inline real32x4 round(const real32x4 &v);
inline real32x4 min(const real32x4 &a, const real32x4 &b);
inline real32x4 max(const real32x4 &a, const real32x4 &b);
inline real32x4 log(const real32x4 &v);
inline real32x4 exp(const real32x4 &v);
inline real32x4 sqrt(const real32x4 &v);
inline real32x4 pow(const real32x4 &a, const real32x4 &b);
inline real32x4 sin(const real32x4 &v);
inline real32x4 cos(const real32x4 &v);
inline void sincos(const real32x4 &v, real32x4 &sin_out, real32x4 &cos_out);

// Casting
template<> inline int32x4 reinterpret_bits(const real32x4 &v);
