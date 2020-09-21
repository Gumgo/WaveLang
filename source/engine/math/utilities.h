#pragma once

#include "common/common.h"

// Returns 0 if x is inf or nan, otherwise returns x
inline real32 sanitize_inf_nan(real32 x) {
	static constexpr int32 k_exponent_mask = 0x7f800000;

	// If x is inf or nan, all exponent bits are 1
	int32 x_int = reinterpret_bits<int32>(x);

	// This mask is 0 if x is inf or nan and 0xffffffff otherwise
	int32 mask = -static_cast<int32>((x_int & k_exponent_mask) != k_exponent_mask);
	return reinterpret_bits<real32>(x_int & mask);
}
