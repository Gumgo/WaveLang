#ifndef WAVELANG_TYPES_H__
#define WAVELANG_TYPES_H__

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

#endif // WAVELANG_TYPES_H__