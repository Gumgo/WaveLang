#pragma once

#include <cstddef>
#include <stddef.h> // Also need this for GCC for some reason

typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;
typedef float real32;
typedef double real64;

static_assert(sizeof(int8) == 1, "Incorrect primitive type size");
static_assert(sizeof(uint8) == 1, "Incorrect primitive type size");
static_assert(sizeof(int16) == 2, "Incorrect primitive type size");
static_assert(sizeof(uint16) == 2, "Incorrect primitive type size");
static_assert(sizeof(int32) == 4, "Incorrect primitive type size");
static_assert(sizeof(uint32) == 4, "Incorrect primitive type size");
static_assert(sizeof(int64) == 8, "Incorrect primitive type size");
static_assert(sizeof(uint64) == 8, "Incorrect primitive type size");
static_assert(sizeof(real32) == 4, "Incorrect primitive type size");
static_assert(sizeof(real64) == 8, "Incorrect primitive type size");

template<size_t k_size, bool k_is_unsigned> struct s_integer_type {};
template<> struct s_integer_type<1, true> { typedef uint8 type; };
template<> struct s_integer_type<1, false> { typedef int8 type; };
template<> struct s_integer_type<2, true> { typedef uint16 type; };
template<> struct s_integer_type<2, false> { typedef int16 type; };
template<> struct s_integer_type<4, true> { typedef uint32 type; };
template<> struct s_integer_type<4, false> { typedef int32 type; };
template<> struct s_integer_type<8, true> { typedef uint64 type; };
template<> struct s_integer_type<8, false> { typedef int64 type; };

