#pragma once

#include "common/common.h"

#define SIMD_IMPLEMENTATION_SSE_ENABLED 0
#define SIMD_IMPLEMENTATION_NEON_ENABLED 0

#if IS_TRUE(ARCHITECTURE_X86_64)
	#undef SIMD_IMPLEMENTATION_SSE_ENABLED
	#define SIMD_IMPLEMENTATION_SSE_ENABLED 1
	#if IS_TRUE(COMPILER_MSVC)
		#include <intrin.h>
	#else // IS_TRUE(COMPILER_MSVC)
		#include <x86intrin.h>
	#endif // IS_TRUE(COMPILER_MSVC)
#elif IS_TRUE(ARCHITECTURE_ARM)
	#undef SIMD_IMPLEMENTATION_NEON_ENABLED
	#define SIMD_IMPLEMENTATION_NEON_ENABLED 1
	#include <arm_neon.h>
#else // ARCHITECTURE
	#error Unknown architecture, no SIMD implementation provided
#endif // ARCHITECTURE

#define SIMD_ALIGNMENT 16
#define SIMD_BLOCK_ELEMENTS 4
#define SIMD_BLOCK_SIZE 16
#define ALIGNAS_SIMD alignas(SIMD_ALIGNMENT)

// More type-friendly version:
static const size_t k_simd_alignment = SIMD_ALIGNMENT;
static const size_t k_simd_block_elements = SIMD_BLOCK_ELEMENTS;
static const size_t k_simd_block_size = SIMD_BLOCK_SIZE;

#if IS_TRUE(SIMD_IMPLEMENTATION_SSE_ENABLED)

typedef __m128 t_simd_real32;
typedef __m128i t_simd_int32;

#pragma once
#define USE_SSE2
#pragma warning(disable:4305)
#pragma warning(disable:4838)
#include "engine/math/sse_mathfun.h"
#pragma warning(default:4305)
#pragma warning(default:4838)

#elif IS_TRUE(SIMD_IMPLEMENTATION_NEON_ENABLED)

typedef float32x4_t t_simd_real32;
typedef int32x4_t t_simd_int32;

#pragma once
#if IS_TRUE(COMPILER_MSVC)
#pragma warning(disable:4305)
#pragma warning(disable:4838)
#endif // IS_TRUE(COMPILER_MSVC)
#include "engine/math/neon_mathfun.h"
#if IS_TRUE(COMPILER_MSVC)
#pragma warning(default:4305)
#pragma warning(default:4838)
#endif // IS_TRUE(COMPILER_MSVC)

#endif // SIMD_IMPLEMENTATION

