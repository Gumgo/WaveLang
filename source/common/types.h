#pragma once

#include <cstddef>
#include <stddef.h> // Also need this for GCC for some reason

using int8 = signed char;
using uint8 = unsigned char;
using int16 = signed short;
using uint16 = unsigned short;
using int32 = signed int;
using uint32 = unsigned int;
using int64 = signed long long;
using uint64 = unsigned long long;
using real32 = float;
using real64 = double;

STATIC_ASSERT(sizeof(int8) == 1);
STATIC_ASSERT(sizeof(uint8) == 1);
STATIC_ASSERT(sizeof(int16) == 2);
STATIC_ASSERT(sizeof(uint16) == 2);
STATIC_ASSERT(sizeof(int32) == 4);
STATIC_ASSERT(sizeof(uint32) == 4);
STATIC_ASSERT(sizeof(int64) == 8);
STATIC_ASSERT(sizeof(uint64) == 8);
STATIC_ASSERT(sizeof(real32) == 4);
STATIC_ASSERT(sizeof(real64) == 8);

template<size_t k_size, bool k_is_unsigned> struct s_integer_type {};
template<> struct s_integer_type<1, true> { using type = uint8; };
template<> struct s_integer_type<1, false> { using type = int8; };
template<> struct s_integer_type<2, true> { using type = uint16; };
template<> struct s_integer_type<2, false> { using type = int16; };
template<> struct s_integer_type<4, true> { using type = uint32; };
template<> struct s_integer_type<4, false> { using type = int32; };
template<> struct s_integer_type<8, true> { using type = uint64; };
template<> struct s_integer_type<8, false> { using type = int64; };

