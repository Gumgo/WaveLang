#pragma once

#include "common/common.h"
#include "common/math/math_constants.h"
#include "common/math/simd.h"
#include "common/math/utilities.h"

// Math type declaration includes
#include "common/math/int32x4.h"
#include "common/math/real32x4.h"

// Math type definition includes - each implementation has the appropriate ifdefs so only one will be compiled
#include "common/math/neon/int32x4_neon.h"
#include "common/math/neon/real32x4_neon.h"
#include "common/math/sse/int32x4_sse.h"
#include "common/math/sse/real32x4_sse.h"

#if 0 // $TODO $SIMD enable this soon
// These typedefs are for when the maximum size SIMD type should be used
#if IS_TRUE(SIMD_256_ENABLED)
using int32xN = int32x8;
using real32xN = real32x8;
#elif IS_TRUE(SIMD_128_ENABLED)
using int32xN = int32x4;
using real32xN = real32x4;
#else // SIMD
// No SIMD types are available
// $TODO add int32x1 and real32x1 types which just wrap an int and a float to support this case
#endif // SIMD
#endif