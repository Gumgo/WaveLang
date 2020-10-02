#include "common/math/simd.h"

#if IS_TRUE(ARCHITECTURE_X86_64) && (IS_TRUE(COMPILER_GCC) || IS_TRUE(COMPILER_CLANG))
#include <cpuid.h>
#endif // IS_TRUE(ARCHITECTURE_X86_64) && (IS_TRUE(COMPILER_GCC) || IS_TRUE(COMPILER_CLANG))

#if IS_TRUE(ARCHITECTURE_X86_64)

static void cpuid(int32 cpu_info[4], int function) {
#if IS_TRUE(COMPILER_MSVC)
	__cpuidex(cpu_info, function, 0);
#elif IS_TRUE(COMPILER_GCC) || IS_TRUE(COMPILER_CLANG)
#else // COMPILER
	__get_cpuid(function, &cpu_info[3], &cpu_info[2], &cpu_info[1], &cpu_info[0]);
#error Unknown compiler
#endif // COMPILER
}

#endif // IS_TRUE(ARCHITECTURE_X86_64)

uint32 get_simd_requirements() {
	uint32 simd_requirements =
		(SIMD_IMPLEMENTATION_SSE_ENABLED << enum_index(e_simd_instruction_set::k_sse))
		| (SIMD_IMPLEMENTATION_SSE2_ENABLED << enum_index(e_simd_instruction_set::k_sse2))
		| (SIMD_IMPLEMENTATION_SSE3_ENABLED << enum_index(e_simd_instruction_set::k_sse3))
		| (SIMD_IMPLEMENTATION_SSSE3_ENABLED << enum_index(e_simd_instruction_set::k_ssse3))
		| (SIMD_IMPLEMENTATION_SSE4_1_ENABLED << enum_index(e_simd_instruction_set::k_sse4_1))
		| (SIMD_IMPLEMENTATION_SSE4_2_ENABLED << enum_index(e_simd_instruction_set::k_sse4_2))
		| (SIMD_IMPLEMENTATION_AVX_ENABLED << enum_index(e_simd_instruction_set::k_avx))
		| (SIMD_IMPLEMENTATION_AVX2_ENABLED << enum_index(e_simd_instruction_set::k_avx2))
		| (SIMD_IMPLEMENTATION_NEON_ENABLED << enum_index(e_simd_instruction_set::k_neon));

	return simd_requirements;
}

uint32 get_simd_availability() {
	uint32 simd_availability = 0;

#if IS_TRUE(ARCHITECTURE_X86_64)
	int32 cpu_info_0[4];
	cpuid(cpu_info_0, 0);

	int32 cpu_info_7[4];
	cpuid(cpu_info_7, 7);

	bool os_xsave_supported = (cpu_info_0[2] & (1 << 27)) != 0;
	uint64 xcr_feature_mask = 0;
	if (os_xsave_supported) {
		xcr_feature_mask = _xgetbv(0);
	}

	if (cpu_info_0[3] & (1 << 25)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_sse);
	}

	if (cpu_info_0[3] & (1 << 26)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_sse2);
	}

	if (cpu_info_0[2] & (1 << 0)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_sse3);
	}

	if (cpu_info_0[2] & (1 << 9)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_ssse3);
	}

	if (cpu_info_0[2] & (1 << 19)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_sse4_1);
	}

	if (cpu_info_0[2] & (1 << 20)) {
		simd_availability |= 1 << enum_index(e_simd_instruction_set::k_sse4_2);
	}

	if ((xcr_feature_mask & 0x6) == 0x6) {
		if (cpu_info_0[2] & (1 << 28)) {
			simd_availability |= 1 << enum_index(e_simd_instruction_set::k_avx);
		}

		if (cpu_info_7[1] & (1 << 5)) {
			simd_availability |= 1 << enum_index(e_simd_instruction_set::k_avx2);
		}
	}
#elif IS_TRUE(ARCHITECTURE_ARM)
#error Not yet implemented // $TODO $NEON
#else // ARCHITECTURE
#error Unknown architecture
#endif // ARCHITECTURE

	return simd_availability;
}
