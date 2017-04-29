#ifndef WAVELANG_ENGINE_MATH_MATH_H__
#define WAVELANG_ENGINE_MATH_MATH_H__

#include "engine/math/math_constants.h"
#include "engine/math/simd.h"

// Math type declaration includes
#include "engine/math/math_int32_4.h"
#include "engine/math/math_real32_4.h"

// Math type definition includes
#if IS_TRUE(SIMD_IMPLEMENTATION_SSE_ENABLED)
#include "engine/math/math_int32_4_sse.inl"
#include "engine/math/math_real32_4_sse.inl"
#elif IS_TRUE(SIMD_IMPLEMENTATION_NEON_ENABLED)
#include "engine/math/math_int32_4_neon.inl"
#include "engine/math/math_real32_4_neon.inl"
#else // SIMD_IMPLEMENTATION_ENABLED
#error No SIMD implementation specified
#endif // SIMD_IMPLEMENTATION_ENABLED

#endif // WAVELANG_ENGINE_MATH_MATH_H__
