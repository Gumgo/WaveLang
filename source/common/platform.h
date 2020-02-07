#pragma once

#include "common/macros.h"

// Exactly one of these is set for the platform
#define PLATFORM_WINDOWS			0
#define PLATFORM_APPLE				0
#define PLATFORM_LINUX				0

// Exactly one of these is set for the compiler
#define COMPILER_MSVC				0
#define COMPILER_GCC				0
#define COMPILER_CLANG				0

// Exactly one of these is set for the architecture
#define ARCHITECTURE_X86_64			0
#define ARCHITECTURE_ARM			0

// Exactly one of these is set for the endianness
#define ENDIANNESS_LITTLE			0
#define ENDIANNESS_BIG				0

// More detailed target info, some or none of these may be set
#define TARGET_RASPBERRY_PI			0

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
#elif defined(__linux__)
	#undef PLATFORM_LINUX
	#define PLATFORM_LINUX 1
	#include <unistd.h>
	#include <sys/types.h>
	#define CACHE_LINE_SIZE 64
#else // platform
	#error Unknown platform
#endif // platform

#if defined(_MSC_VER)
	#undef COMPILER_MSVC
	#define COMPILER_MSVC 1
	#if defined(_M_IX86) || defined(_M_X64)
		#undef ARCHITECTURE_X86_64
		#define ARCHITECTURE_X86_64 1
	#elif defined(_M_ARM) || defined(_M_ARM64)
		#undef ARCHITECTURE_ARM
		#define ARCHITECTURE_ARM 1
	#endif // ARCHITECTURE
#elif defined(__GNUC__) || defined(__GNUG__)
	#undef COMPILER_GCC
	#define COMPILER_GCC 1
	#if defined(__i386) || defined(__x86_64__)
		#undef ARCHITECTURE_X86_64
		#define ARCHITECTURE_X86_64 1
	#elif defined(__arm__)
		#undef ARCHITECTURE_ARM
		#define ARCHITECTURE_ARM 1
	#endif // ARCHITECTURE
#elif defined(__clang__)
	#undef COMPILER_CLANG
	#define COMPILER_CLANG 1
	#if defined(__i386) || defined(__x86_64__)
		#undef ARCHITECTURE_X86_64
		#define ARCHITECTURE_X86_64 1
	#elif defined(__ARM_ARCH)
		#undef ARCHITECTURE_ARM
		#define ARCHITECTURE_ARM 1
	#endif // ARCHITECTURE
#else // COMPILER
	#error Unknown compiler
#endif // COMPILER

#if IS_TRUE(PLATFORM_WINDOWS)
	#undef ENDIANNESS_LITTLE
	#define ENDIANNESS_LITTLE 1
#elif IS_TRUE(COMPILER_GCC)
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

#ifdef RASPBERRY_PI
	#undef TARGET_RASPBERRY_PI
	#define TARGET_RASPBERRY_PI 1
#endif // RASPBERRY_PI

#ifndef CACHE_LINE_SIZE
#error Cache line size not defined
#endif // CACHE_LINE_SIZE

