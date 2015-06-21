#ifndef WAVELANG_PLATFORM_H__
#define WAVELANG_PLATFORM_H__

// Exactly one of these is set for the platform
#define PLATFORM_WINDOWS			0
#define PLATFORM_APPLE				0
#define PLATFORM_LINUX				0

// Exactly one of these is set for the compiler
#define COMPILER_MSVC				0
#define COMPILER_GCC				0
#define COMPILER_CLANG				0

// Exactly one of these is set for the endianness
#define ENDIANNESS_LITTLE			0
#define ENDIANNESS_BIG				0

#if defined(_WIN32)
#undef PLATFORM_WINDOWS
#define PLATFORM_WINDOWS 1
#define CACHE_LINE_SIZE 64
#define NOMINMAX
#include <Windows.h>
#include <WinNT.h>
#include <WinBase.h>
#elif defined(__APPLE__)
#undef PLATFORM_APPLE
#define PLATFORM_APPLE 1
#define CACHE_LINE_SIZE 64
#elif defined(__LINUX__)
#undef PLATFORM_LINUX
#define PLATFORM_LINUX 1
#define CACHE_LINE_SIZE 64
#else // platform
#error Unknown platform
#endif // platform

#if defined(_MSC_VER)
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#elif defined(__GNUC__) || defined(__GNUG__)
#undef COMPILER_GCC
#define COMPILER_GCC 1
#elif defined(__clang__)
#undef COMPILER_CLANG
#define COMPILER_CLANG 1
#else // COMPILER
#error Unknown compiler
#endif // COMPILER

#if PLATFORM_WINDOWS
#undef ENDIANNESS_LITTLE
#define ENDIANNESS_LITTLE 1
#elif COMPILER_GCC
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#undef ENDIANNESS_LITTLE
#define ENDIANNESS_LITTLE 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#undef ENDIANNESS_BIG
#define ENDIANNESS_BIG 1
#else // __BYTE_ORDER__
#error Unknown endianness
#endif // __BYTE_ORDER__
#else // endianness detection
#error Unknown endianness
#endif // endianness detection

#ifndef CACHE_LINE_SIZE
#error Cache line size not defined
#endif // CACHE_LINE_SIZE

#endif // WAVELANG_PLATFORM_H__