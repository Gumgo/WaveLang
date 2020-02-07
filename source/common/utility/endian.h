#pragma once

#include "common/common.h"

template<typename t_value>
t_value swap_byte_order(t_value value) {
	// $TODO optimize
	t_value result;
	const uint8 *bytes = reinterpret_cast<const uint8 *>(&value);
	uint8 *result_bytes = reinterpret_cast<uint8 *>(&result);

	for (uintptr index = 0; index < sizeof(t_value); index++) {
		result_bytes[index] = bytes[sizeof(t_value) - index - 1];
	}

	return result;
}

#if IS_TRUE(ENDIANNESS_BIG)

template<typename t_value> t_value native_to_big_endian(t_value value) { return value; }
template<typename t_value> t_value native_to_little_endian(t_value value) { return swap_byte_order(value); }
template<typename t_value> t_value big_to_native_endian(t_value value) { return value; }
template<typename t_value> t_value little_to_native_endian(t_value value) { return swap_byte_order(value); }

#elif IS_TRUE(ENDIANNESS_LITTLE)

template<typename t_value> t_value native_to_big_endian(t_value value) { return swap_byte_order(value); }
template<typename t_value> t_value native_to_little_endian(t_value value) { return value; }
template<typename t_value> t_value big_to_native_endian(t_value value) { return swap_byte_order(value); }
template<typename t_value> t_value little_to_native_endian(t_value value) { return value; }

#else // ENDIANNESS

#error Unknown endianness

#endif // ENDIANNESS

