#include "common/math/floating_point.h"

#if IS_TRUE(ARCHITECTURE_X86_64)
	#if IS_TRUE(COMPILER_MSVC)
		#include <intrin.h>
	#elif IS_TRUE(COMPILER_GCC) || IS_TRUE(COMPILER_CLANG)
		#include <x86intrin.h>
	#else // COMPILER
		#error Unknown compiler
	#endif // COMPILER
#elif IS_TRUE(ARCHITECTURE_ARM)
#include <arm_neon.h>
#else // ARCHITECTURE
#error Unknown architecture
#endif // ARCHITECTURE

void initialize_floating_point_behavior() {
#if IS_TRUE(ARCHITECTURE_X86_64)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#elif IS_TRUE(ARCHITECTURE_ARM)
#error Not yet implemented // $TODO $NEON
#else // ARCHITECTURE
#error Unknown architecture
#endif // ARCHITECTURE
}
