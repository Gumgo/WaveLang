#pragma once

#include "common/common.h"

#include "engine/math/math.h"

// Fast approximation to log2. Result is divided into integer and fraction parts.
inline c_int32_4 fast_log2(const c_real32_4 &x, c_real32_4 &out_frac) {
	// x = M * 2^E
	// log2(x) = log2(M) + log2(2^E)
	//         = log2(M) + E
	//         = log2(M) + E
	// Since the mantissa is in the range [1,2) (for non-denormalized floats) which maps continuously to [0,1), we split
	// our result such that:
	// integer_part    = E
	// fractional_part = log2(M)
	// We approximate fractional_part using a 2nd-degree polynomial; see find_fast_log2_coefficients for details.

	// Cast to raw bits
	c_int32_4 x_e = x.int32_4_from_bits();
	// Shift and mask to obtain the exponent; subtract 127 for bias
	// [sign 1][exp 8][mant 23]
	x_e = (x_e.shift_right_unsigned(23) & c_int32_4(0x000000ff)) - c_int32_4(127);

	// We now want to set the exponent to 0 so that we obtain the float value M * 2^0 = M. With bias, this is 127.
	c_int32_4 x_m_bits = x.int32_4_from_bits();
	// Clear the sign and exponent, then OR in 127
	x_m_bits = (x_m_bits & 0x007fffff) | 0x3f800000;

	// Apply the polynomial approximation
	c_real32_4 x_m = x_m_bits.real32_4_from_bits();
	out_frac = (c_real32_4(-0.346555382f) * x_m + c_real32_4(2.03966618f)) * x_m + c_real32_4(-1.69311082f);
	return x_e;
}

