#ifndef WAVELANG_ENGINE_MATH_SSE_H__
#define WAVELANG_ENGINE_MATH_SSE_H__

#include "common/common.h"

#define SSE_ALIGNMENT 16
#define SSE_BLOCK_ELEMENTS 4
#define SSE_BLOCK_SIZE 16
#define ALIGNAS_SSE alignas(SSE_ALIGNMENT)

// More type-friendly version:
static const size_t k_sse_alignment = SSE_ALIGNMENT;
static const size_t k_sse_block_elements = SSE_BLOCK_ELEMENTS;
static const size_t k_sse_block_size = SSE_BLOCK_SIZE;

#if IS_TRUE(COMPILER_MSVC)
#include <intrin.h>
#else // IS_TRUE(COMPILER_MSVC)
#include <x86intrin.h>
#endif // IS_TRUE(COMPILER_MSVC)

#ifndef WAVELANG_SSE_MATHFUN_H__
#define WAVELANG_SSE_MATHFUN_H__
#define USE_SSE2
#pragma warning(disable:4305)
#pragma warning(disable:4838)
#include "engine/math/sse_mathfun.h"
#pragma warning(default:4305)
#pragma warning(default:4838)
#endif // WAVELANG_SSE_MATHFUN_H__

#endif // WAVELANG_ENGINE_MATH_SSE_H__
