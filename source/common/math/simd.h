#pragma once

#include "common/common.h"

// SSE availability:
#define SIMD_IMPLEMENTATION_SSE_ENABLED 0
#define SIMD_IMPLEMENTATION_SSE2_ENABLED 0
#define SIMD_IMPLEMENTATION_SSE3_ENABLED 0
#define SIMD_IMPLEMENTATION_SSSE3_ENABLED 0
#define SIMD_IMPLEMENTATION_SSE4_1_ENABLED 0
#define SIMD_IMPLEMENTATION_SSE4_2_ENABLED 0
#define SIMD_IMPLEMENTATION_AVX_ENABLED 0
#define SIMD_IMPLEMENTATION_AVX2_ENABLED 0

// NEON availability: $TODO $NEON break this down further
#define SIMD_IMPLEMENTATION_NEON_ENABLED 0

#if IS_TRUE(ARCHITECTURE_X86_64)
	#if IS_TRUE(COMPILER_MSVC)
		#include <intrin.h>
		// These aren't defined on MSVC
		#ifndef SCRAPER_ENABLED
			#define __SSE__
			#define __SSE2__
			#if defined(__AVX__) || defined(__AVX2__)
				#define __SSE3__
				#define __SSSE3__
				#define __SSE4_1__
				#define __SSE4_2__
			#endif // defined(__AVX__) || defined(__AVX2__)
		#endif // SCRAPER_ENABLED
	#elif IS_TRUE(COMPILER_GCC) || IS_TRUE(COMPILER_CLANG)
		#include <x86intrin.h>
	#else // COMPILER
		#error Unknown compiler
	#endif // COMPILER
#elif IS_TRUE(ARCHITECTURE_ARM)
	#include <arm_neon.h>
#else // ARCHITECTURE
	#error Unknown architecture, no SIMD implementation provided
#endif // ARCHITECTURE

#ifdef __SSE__
	#undef SIMD_IMPLEMENTATION_SSE_ENABLED
	#define SIMD_IMPLEMENTATION_SSE_ENABLED 1
#endif // __SSE__
#ifdef __SSE2__
	#undef SIMD_IMPLEMENTATION_SSE2_ENABLED
	#define SIMD_IMPLEMENTATION_SSE2_ENABLED 1
#endif // __SSE2__
#ifdef __SSE3__
	#undef SIMD_IMPLEMENTATION_SSE3_ENABLED
	#define SIMD_IMPLEMENTATION_SSE3_ENABLED 1
#endif // __SSE3__
#ifdef __SSSE3__
	#undef SIMD_IMPLEMENTATION_SSSE3_ENABLED
	#define SIMD_IMPLEMENTATION_SSSE3_ENABLED 1
#endif // __SSSE3__
#ifdef __SSE4_1__
	#undef SIMD_IMPLEMENTATION_SSE4_1_ENABLED
	#define SIMD_IMPLEMENTATION_SSE4_1_ENABLED 1
#endif // __SSE4_1__
#ifdef __SSE4_2__
	#undef SIMD_IMPLEMENTATION_SSE4_2_ENABLED
	#define SIMD_IMPLEMENTATION_SSE4_2_ENABLED 1
#endif // __SSE4_2__
#ifdef __AVX__
	#undef SIMD_IMPLEMENTATION_AVX_ENABLED
	#define SIMD_IMPLEMENTATION_AVX_ENABLED 1
#endif // __AVX__
#ifdef __AVX2__
	#undef SIMD_IMPLEMENTATION_AVX2_ENABLED
	#define SIMD_IMPLEMENTATION_AVX2_ENABLED 1
#endif // __AVX2__

#ifdef __ARM_NEON // $TODO $NEON not sure if this is correct
	#undef SIMD_IMPLEMENTATION_NEON_ENABLED
	#define SIMD_IMPLEMENTATION_NEON_ENABLED 1
#endif // __ARM_NEON

#define SIMD_128_ENABLED (SIMD_IMPLEMENTATION_AVX_ENABLED || SIMD_IMPLEMENTATION_NEON_ENABLED)
#define SIMD_256_ENABLED SIMD_IMPLEMENTATION_AVX2_ENABLED

static constexpr size_t k_simd_128_size = 16;
static constexpr size_t k_simd_128_size_bits = 128;
static constexpr size_t k_simd_128_alignment = 16;

static constexpr size_t k_simd_256_size = 32;
static constexpr size_t k_simd_256_size_bits = 256;
static constexpr size_t k_simd_256_alignment = 32;

// Generic SIMD sizes/alignments default to the largest type
#if IS_TRUE(SIMD_256_ENABLED)
static constexpr size_t k_simd_size = k_simd_256_size;
static constexpr size_t k_simd_size_bits = k_simd_256_size_bits;
static constexpr size_t k_simd_alignment = k_simd_256_alignment;
#else // IS_TRUE(SIMD_256_ENABLED)
static constexpr size_t k_simd_size = k_simd_128_size;
static constexpr size_t k_simd_size_bits = k_simd_128_size_bits;
static constexpr size_t k_simd_alignment = k_simd_128_alignment;
#endif // IS_TRUE(SIMD_256_ENABLED)

#define ALIGNAS_SIMD_128 alignas(k_simd_128_alignment)
#define ALIGNAS_SIMD_256 alignas(k_simd_256_alignment)
#define ALIGNAS_SIMD alignas(k_simd_alignment)

#if IS_TRUE(SIMD_128_ENABLED)
	#if IS_TRUE(SIMD_IMPLEMENTATION_AVX_ENABLED)
		using t_simd_real32x4 = __m128;
		using t_simd_int32x4 = __m128i;

		#define USE_SSE2
		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(disable:4305)
		#pragma warning(disable:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
		#include "common/math/sse_mathfun.h"
		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(default:4305)
		#pragma warning(default:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
	#elif IS_TRUE(SIMD_IMPLEMENTATION_NEON_ENABLED)
		using t_simd_real32x4 = float32x4_t;
		using t_simd_int32x4 = int32x4_t;

		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(disable:4305)
		#pragma warning(disable:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
		#include "common/math/neon_mathfun.h"
		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(default:4305)
		#pragma warning(default:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
	#else // SIMD_IMPLEMENTATION
		#error Unsupported SIMD 128 implementation
	#endif // SIMD_IMPLEMENTATION
#endif // IS_TRUE(SIMD_128_ENABLED)

#if IS_TRUE(SIMD_256_ENABLED)
	#if IS_TRUE(SIMD_IMPLEMENTATION_AVX2_ENABLED)
		using t_simd_real32x8 = __m256;
		using t_simd_int32x8 = __m256i;

		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(disable:4305)
		#pragma warning(disable:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
		#include "common/math/avx_mathfun.h"
		#if IS_TRUE(COMPILER_MSVC)
		#pragma warning(default:4305)
		#pragma warning(default:4838)
		#endif // IS_TRUE(COMPILER_MSVC)
	#else // SIMD_IMPLEMENTATION
		#error Unsupported SIMD 256 implementation
	#endif // SIMD_IMPLEMENTATION
#endif // IS_TRUE(SIMD_256_ENABLED)

enum class e_simd_instruction_set {
	k_sse,
	k_sse2,
	k_sse3,
	k_ssse3,
	k_sse4_1,
	k_sse4_2,
	k_avx,
	k_avx2,
	k_neon,

	k_count
};

// $TODO call these
uint32 get_simd_requirements();
uint32 get_simd_availability();
