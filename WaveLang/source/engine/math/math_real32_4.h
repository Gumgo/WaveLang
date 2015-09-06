#ifndef WAVELANG_MATH_REAL32_4_H__
#define WAVELANG_MATH_REAL32_4_H__

#include "common/common.h"
#include "engine/math/sse.h"

class c_int32_4;

class ALIGNAS_SSE c_real32_4 {
public:
	inline c_real32_4();
	inline c_real32_4(real32 v);
	inline c_real32_4(real32 a, real32 b, real32 c, real32 d);
	inline c_real32_4(const real32 *ptr);
	inline c_real32_4(const __m128 &v);
	inline c_real32_4(const c_real32_4 &v);

	inline void load(const real32 *ptr);
	inline void store(real32 *ptr) const;

	inline c_real32_4 &operator=(const __m128 &v);
	inline c_real32_4 &operator=(const c_real32_4 &v);
	// $TODO +=, etc.

	inline operator __m128() const;
	inline c_int32_4 int32_4_from_bits() const;
	inline c_real32_4 sum_elements() const;

private:
	__m128 m_value;
};

// Unary operators
inline c_real32_4 operator+(const c_real32_4 &v);
inline c_real32_4 operator-(const c_real32_4 &v);

// Binary operators
inline c_real32_4 operator+(const c_real32_4 &lhs, const c_real32_4 &rhs);
inline c_real32_4 operator-(const c_real32_4 &lhs, const c_real32_4 &rhs);
inline c_real32_4 operator*(const c_real32_4 &lhs, const c_real32_4 &rhs);
inline c_real32_4 operator/(const c_real32_4 &lhs, const c_real32_4 &rhs);
inline c_real32_4 operator%(const c_real32_4 &lhs, const c_real32_4 &rhs);

// Arithmetic/utility
inline c_real32_4 abs(const c_real32_4 &v);
inline c_real32_4 floor(const c_real32_4 &v);
inline c_real32_4 ceil(const c_real32_4 &v);
inline c_real32_4 round(const c_real32_4 &v);
inline c_real32_4 min(const c_real32_4 &a, const c_real32_4 &b);
inline c_real32_4 max(const c_real32_4 &a, const c_real32_4 &b);
inline c_real32_4 log(const c_real32_4 &v);
inline c_real32_4 exp(const c_real32_4 &v);
inline c_real32_4 sqrt(const c_real32_4 &v);
inline c_real32_4 pow(const c_real32_4 &a, const c_real32_4 &b);
inline c_real32_4 sin(const c_real32_4 &v);
inline c_real32_4 cos(const c_real32_4 &v);

// Shuffle
template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_real32_4 shuffle(const c_real32_4 &a, const c_real32_4 &b);

#endif // WAVELANG_MATH_REAL32_4_H__