#pragma once

#include "engine/math/math_constants.h"
#include "engine/math/simd.h"

// Math type declaration includes
#include "engine/math/math_int32x4.h"
#include "engine/math/math_real32x4.h"

// Math type definition includes
#if IS_TRUE(SIMD_IMPLEMENTATION_SSE_ENABLED)
#include "engine/math/math_int32x4_sse.inl"
#include "engine/math/math_real32x4_sse.inl"
#elif IS_TRUE(SIMD_IMPLEMENTATION_NEON_ENABLED)
#include "engine/math/math_int32x4_neon.inl"
#include "engine/math/math_real32x4_neon.inl"
#else // SIMD_IMPLEMENTATION_ENABLED
#error No SIMD implementation specified
#endif // SIMD_IMPLEMENTATION_ENABLED

