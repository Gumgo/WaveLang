#ifndef WAVELANG_MATH_INT32_4_H__
#define WAVELANG_MATH_INT32_4_H__

#include "common/common.h"

#include "engine/math/sse.h"

class c_real32_4;

class ALIGNAS_SSE c_int32_4 {
public:
	typedef int32 t_element;

	inline c_int32_4();
	inline c_int32_4(int32 v);
	inline c_int32_4(int32 a, int32 b, int32 c, int32 d);
	inline c_int32_4(const int32 *ptr);
	inline c_int32_4(const __m128i &v);
	inline c_int32_4(const c_int32_4 &v);

	inline void load(const int32 *ptr);
	inline void store(int32 *ptr) const;

	inline c_int32_4 &operator=(const __m128i &v);
	inline c_int32_4 &operator=(const c_int32_4 &v);
	// $TODO +=, etc.

	inline operator __m128i() const;
	inline c_real32_4 real32_4_from_bits() const;

	// Replacement for >>> operator
	inline c_int32_4 shift_right_unsigned(int32 rhs) const;
	inline c_int32_4 shift_right_unsigned(const c_int32_4 &rhs) const;

private:
	__m128i m_value;
};

// Unary operators
inline c_int32_4 operator+(const c_int32_4 &v);
inline c_int32_4 operator-(const c_int32_4 &v);
inline c_int32_4 operator~(const c_int32_4 &v);

// Binary operators
inline c_int32_4 operator+(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator-(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator&(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator|(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator^(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator<<(const c_int32_4 &lhs, int32 rhs);
inline c_int32_4 operator<<(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator>>(const c_int32_4 &lhs, int32 rhs);
inline c_int32_4 operator>>(const c_int32_4 &lhs, const c_int32_4 &rhs);

inline c_int32_4 operator==(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator!=(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator>(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator<(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator>=(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 operator<=(const c_int32_4 &lhs, const c_int32_4 &rhs);

inline c_int32_4 greater_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 less_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 greater_equal_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs);
inline c_int32_4 less_equal_unsigned(const c_int32_4 &lhs, const c_int32_4 &rhs);

// Arithmetic/utility
inline c_int32_4 abs(const c_int32_4 &v);
inline c_int32_4 min(const c_int32_4 &a, const c_int32_4 &b);
inline c_int32_4 max(const c_int32_4 &a, const c_int32_4 &b);
inline c_int32_4 min_unsigned(const c_int32_4 &a, const c_int32_4 &b);
inline c_int32_4 max_unsigned(const c_int32_4 &a, const c_int32_4 &b);

// Masking
inline int32 mask_from_msb(const c_int32_4 &v);

// Shuffle
inline c_int32_4 single_element(const c_int32_4 &v, int32 pos);
template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_int32_4 shuffle(const c_int32_4 &v);
template<int32 k_pos_0, int32 k_pos_1, int32 k_pos_2, int32 k_pos_3>
c_int32_4 shuffle(const c_int32_4 &a, const c_int32_4 &b);

#endif // WAVELANG_MATH_INT32_4_H__