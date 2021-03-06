#include "common/common.h"
#include "common/math/math.h"

#include <gtest/gtest.h>

#if IS_TRUE(SIMD_128_ENABLED)

TEST(Math, Real32x4) {
	ALIGNAS_SIMD_128 static constexpr real32 k_elements[] = { 1.0f, 2.0f, 3.0f, 4.0f };
	ALIGNAS_SIMD_128 static constexpr real32 k_constant_elements[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	EXPECT_TRUE(all_true(real32x4(k_elements) == real32x4(1.0f, 2.0f, 3.0f, 4.0f)));
	EXPECT_TRUE(all_true(real32x4(k_constant_elements) == real32x4(1.0f)));

	ALIGNAS_SIMD_128 real32 elements[4];
	zero_type(elements, array_count(elements));
	real32x4(k_elements).store(elements);
	EXPECT_EQ(memcmp(elements, k_elements, sizeof(elements)), 0);

	EXPECT_TRUE(all_true(real32x4(1.0f, 2.0f, 3.0f, 4.0f).sum_elements() == real32x4(10.0f)));
	EXPECT_TRUE(all_true(-real32x4(1.0f) == real32x4(-1.0f)));
	EXPECT_TRUE(all_true(real32x4(1.0f) + real32x4(2.0f) == real32x4(3.0f)));
	EXPECT_TRUE(all_true(real32x4(1.0f) - real32x4(2.0f) == real32x4(-1.0f)));
	EXPECT_TRUE(all_true(real32x4(2.0f) * real32x4(3.0f) == real32x4(6.0f)));
	EXPECT_TRUE(all_true(real32x4(8.0f) / real32x4(4.0f) == real32x4(2.0f)));
	EXPECT_TRUE(all_true(real32x4(8.0f) % real32x4(3.0f) == real32x4(2.0f)));

	EXPECT_TRUE(all_true(real32x4(1.0f) == real32x4(1.0f)));
	EXPECT_TRUE(all_false(real32x4(0.0f) == real32x4(1.0f)));
	EXPECT_TRUE(all_true(real32x4(0.0f) != real32x4(1.0f)));
	EXPECT_TRUE(all_false(real32x4(1.0f) != real32x4(1.0f)));
	EXPECT_TRUE(all_true(real32x4(1.0f) > real32x4(0.0f)));
	EXPECT_TRUE(all_false(real32x4(0.0f) > real32x4(1.0f)));
	EXPECT_TRUE(all_false(real32x4(0.0f) > real32x4(0.0f)));
	EXPECT_TRUE(all_true(real32x4(0.0f) < real32x4(1.0f)));
	EXPECT_TRUE(all_false(real32x4(1.0f) < real32x4(0.0f)));
	EXPECT_TRUE(all_false(real32x4(0.0f) < real32x4(0.0f)));
	EXPECT_TRUE(all_true(real32x4(1.0f) >= real32x4(0.0f)));
	EXPECT_TRUE(all_false(real32x4(0.0f) >= real32x4(1.0f)));
	EXPECT_TRUE(all_true(real32x4(0.0f) >= real32x4(0.0f)));
	EXPECT_TRUE(all_true(real32x4(0.0f) <= real32x4(1.0f)));
	EXPECT_TRUE(all_false(real32x4(1.0f) <= real32x4(0.0f)));
	EXPECT_TRUE(all_true(real32x4(0.0f) <= real32x4(0.0f)));

	EXPECT_TRUE(all_true(abs(real32x4(-1.0f)) == real32x4(1.0f)));
	EXPECT_TRUE(all_true(min(real32x4(1.0f), real32x4(2.0f)) == real32x4(1.0f)));
	EXPECT_TRUE(all_true(max(real32x4(1.0f), real32x4(2.0f)) == real32x4(2.0f)));
	EXPECT_TRUE(all_true(floor(real32x4(1.75f)) == real32x4(1.0f)));
	EXPECT_TRUE(all_true(floor(real32x4(-1.75f)) == real32x4(-2.0f)));
	EXPECT_TRUE(all_true(floor(real32x4(-1.25f)) == real32x4(-2.0f)));
	EXPECT_TRUE(all_true(ceil(real32x4(1.25f)) == real32x4(2.0f)));
	EXPECT_TRUE(all_true(ceil(real32x4(-1.25f)) == real32x4(-1.0f)));
	EXPECT_TRUE(all_true(ceil(real32x4(-1.75f)) == real32x4(-1.0f)));
	EXPECT_TRUE(all_true(round(real32x4(1.25)) == real32x4(1.0f)));
	EXPECT_TRUE(all_true(round(real32x4(1.75)) == real32x4(2.0f)));
	EXPECT_TRUE(all_true(round(real32x4(-1.25)) == real32x4(-1.0f)));
	EXPECT_TRUE(all_true(round(real32x4(-1.75)) == real32x4(-2.0f)));

	static constexpr real32 k_epsilon = 0.00001f;
	EXPECT_TRUE(all_true(abs(log(real32x4(5.0f)) - real32x4(log(5.0f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(exp(real32x4(5.0f)) - real32x4(exp(5.0f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(sqrt(real32x4(5.0f)) - real32x4(sqrt(5.0f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(pow(real32x4(2.5f), real32x4(3.5f)) - real32x4(pow(2.5f, 3.5f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(sin(real32x4(2.0f)) - real32x4(sin(2.0f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(cos(real32x4(2.0f)) - real32x4(cos(2.0f))) <= real32x4(k_epsilon)));

	real32x4 sin_result;
	real32x4 cos_result;
	sincos(real32x4(2.0f), sin_result, cos_result);
	EXPECT_TRUE(all_true(abs(sin_result - real32x4(sin(2.0f))) <= real32x4(k_epsilon)));
	EXPECT_TRUE(all_true(abs(cos_result - real32x4(cos(2.0f))) <= real32x4(k_epsilon)));
}

TEST(Math, Int32x4) {
	ALIGNAS_SIMD_128 static constexpr int32 k_elements[] = { 1, 2, 3, 4 };
	ALIGNAS_SIMD_128 static constexpr int32 k_constant_elements[] = { 1, 1, 1, 1 };

	EXPECT_TRUE(all_true(int32x4(k_elements) == int32x4(1, 2, 3, 4)));
	EXPECT_TRUE(all_true(int32x4(k_constant_elements) == int32x4(1)));

	ALIGNAS_SIMD_128 int32 elements[4];
	zero_type(elements, array_count(elements));
	int32x4(k_elements).store(elements);
	EXPECT_EQ(memcmp(elements, k_elements, sizeof(elements)), 0);

	EXPECT_TRUE(all_true(int32x4(1, 2, 3, 4).sum_elements() == int32x4(10)));
	EXPECT_TRUE(all_true(~int32x4(0) == int32x4(-1)));
	EXPECT_TRUE(all_true(-int32x4(1) == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(1) + int32x4(2) == int32x4(3)));
	EXPECT_TRUE(all_true(int32x4(1) - int32x4(2) == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(2) * int32x4(3) == int32x4(6)));
	EXPECT_TRUE(all_true((int32x4(0x0f0f0f0f) & int32x4(0x0000ffff)) == int32x4(0x00000f0f)));
	EXPECT_TRUE(all_true((int32x4(0x0f0f0f0f) | int32x4(0x0000ffff)) == int32x4(0x0f0fffff)));
	EXPECT_TRUE(all_true((int32x4(0x0f0f0f0f) ^ int32x4(0x0000ffff)) == int32x4(0x0f0ff0f0)));
	EXPECT_TRUE(all_true(int32x4(1) << 4 == int32x4(16)));
	EXPECT_TRUE(all_true(int32x4(1) << int32x4(4) == int32x4(16)));
	EXPECT_TRUE(all_true(int32x4(1) << 32 == int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(1) << int32x4(32) == int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(16) >> 4 == int32x4(1)));
	EXPECT_TRUE(all_true(int32x4(16) >> 32 == int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(16) >> int32x4(4) == int32x4(1)));
	EXPECT_TRUE(all_true(int32x4(16) >> int32x4(32) == int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(-16) >> 4 == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(-16) >> 32 == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(-16) >> int32x4(4) == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(-16) >> int32x4(32) == int32x4(-1)));
	EXPECT_TRUE(all_true(int32x4(-16).shift_right_unsigned(4) == int32x4(0x0fffffff)));
	EXPECT_TRUE(all_true(int32x4(-16).shift_right_unsigned(int32x4(4)) == int32x4(0x0fffffff)));
	EXPECT_TRUE(all_true(int32x4(-16).shift_right_unsigned(32) == int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(-16).shift_right_unsigned(int32x4(32)) == int32x4(0)));

	EXPECT_TRUE(all_true(int32x4(-1, -1, -1, -1)));
	EXPECT_FALSE(all_true(int32x4(0, -1, -1, -1)));
	EXPECT_TRUE(all_false(int32x4(0, 0, 0, 0)));
	EXPECT_FALSE(all_false(int32x4(0, -1, 0, 0)));
	EXPECT_TRUE(any_true(int32x4(0, 0, -1, 0)));
	EXPECT_FALSE(any_true(int32x4(0, 0, 0, 0)));
	EXPECT_TRUE(any_false(int32x4(-1, -1, -1, 0)));
	EXPECT_FALSE(any_false(int32x4(-1, -1, -1, -1)));

	EXPECT_TRUE(all_true(int32x4(1) == int32x4(1)));
	EXPECT_TRUE(all_false(int32x4(0) == int32x4(1)));
	EXPECT_TRUE(all_true(int32x4(0) != int32x4(1)));
	EXPECT_TRUE(all_false(int32x4(1) != int32x4(1)));
	EXPECT_TRUE(all_true(int32x4(1) > int32x4(0)));
	EXPECT_TRUE(all_false(int32x4(0) > int32x4(1)));
	EXPECT_TRUE(all_false(int32x4(0) > int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(0) < int32x4(1)));
	EXPECT_TRUE(all_false(int32x4(1) < int32x4(0)));
	EXPECT_TRUE(all_false(int32x4(0) < int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(1) >= int32x4(0)));
	EXPECT_TRUE(all_false(int32x4(0) >= int32x4(1)));
	EXPECT_TRUE(all_true(int32x4(0) >= int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(0) <= int32x4(1)));
	EXPECT_TRUE(all_false(int32x4(1) <= int32x4(0)));
	EXPECT_TRUE(all_true(int32x4(0) <= int32x4(0)));

	EXPECT_TRUE(all_true(abs(int32x4(-1)) == int32x4(1)));
	EXPECT_TRUE(all_true(min(int32x4(1), int32x4(2)) == int32x4(1)));
	EXPECT_TRUE(all_true(max(int32x4(1), int32x4(2)) == int32x4(2)));
}

#endif // IS_TRUE(SIMD_128_ENABLED)

#if IS_TRUE(SIMD_256_ENABLED)

TEST(Math, Real32x8) {
	ALIGNAS_SIMD_256 static constexpr real32 k_elements[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
	ALIGNAS_SIMD_256 static constexpr real32 k_constant_elements[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

	EXPECT_TRUE(all_true(real32x8(k_elements) == real32x8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)));
	EXPECT_TRUE(all_true(real32x8(k_constant_elements) == real32x8(1.0f)));

	ALIGNAS_SIMD_256 real32 elements[8];
	zero_type(elements, array_count(elements));
	real32x8(k_elements).store(elements);
	EXPECT_EQ(memcmp(elements, k_elements, sizeof(elements)), 0);

	EXPECT_TRUE(all_true(real32x8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f).sum_elements() == real32x8(36.0f)));
	EXPECT_TRUE(all_true(-real32x8(1.0f) == real32x8(-1.0f)));
	EXPECT_TRUE(all_true(real32x8(1.0f) + real32x8(2.0f) == real32x8(3.0f)));
	EXPECT_TRUE(all_true(real32x8(1.0f) - real32x8(2.0f) == real32x8(-1.0f)));
	EXPECT_TRUE(all_true(real32x8(2.0f) * real32x8(3.0f) == real32x8(6.0f)));
	EXPECT_TRUE(all_true(real32x8(8.0f) / real32x8(4.0f) == real32x8(2.0f)));
	EXPECT_TRUE(all_true(real32x8(8.0f) % real32x8(3.0f) == real32x8(2.0f)));

	EXPECT_TRUE(all_true(real32x8(1.0f) == real32x8(1.0f)));
	EXPECT_TRUE(all_false(real32x8(0.0f) == real32x8(1.0f)));
	EXPECT_TRUE(all_true(real32x8(0.0f) != real32x8(1.0f)));
	EXPECT_TRUE(all_false(real32x8(1.0f) != real32x8(1.0f)));
	EXPECT_TRUE(all_true(real32x8(1.0f) > real32x8(0.0f)));
	EXPECT_TRUE(all_false(real32x8(0.0f) > real32x8(1.0f)));
	EXPECT_TRUE(all_false(real32x8(0.0f) > real32x8(0.0f)));
	EXPECT_TRUE(all_true(real32x8(0.0f) < real32x8(1.0f)));
	EXPECT_TRUE(all_false(real32x8(1.0f) < real32x8(0.0f)));
	EXPECT_TRUE(all_false(real32x8(0.0f) < real32x8(0.0f)));
	EXPECT_TRUE(all_true(real32x8(1.0f) >= real32x8(0.0f)));
	EXPECT_TRUE(all_false(real32x8(0.0f) >= real32x8(1.0f)));
	EXPECT_TRUE(all_true(real32x8(0.0f) >= real32x8(0.0f)));
	EXPECT_TRUE(all_true(real32x8(0.0f) <= real32x8(1.0f)));
	EXPECT_TRUE(all_false(real32x8(1.0f) <= real32x8(0.0f)));
	EXPECT_TRUE(all_true(real32x8(0.0f) <= real32x8(0.0f)));

	EXPECT_TRUE(all_true(abs(real32x8(-1.0f)) == real32x8(1.0f)));
	EXPECT_TRUE(all_true(min(real32x8(1.0f), real32x8(2.0f)) == real32x8(1.0f)));
	EXPECT_TRUE(all_true(max(real32x8(1.0f), real32x8(2.0f)) == real32x8(2.0f)));
	EXPECT_TRUE(all_true(floor(real32x8(1.75f)) == real32x8(1.0f)));
	EXPECT_TRUE(all_true(floor(real32x8(-1.75f)) == real32x8(-2.0f)));
	EXPECT_TRUE(all_true(floor(real32x8(-1.25f)) == real32x8(-2.0f)));
	EXPECT_TRUE(all_true(ceil(real32x8(1.25f)) == real32x8(2.0f)));
	EXPECT_TRUE(all_true(ceil(real32x8(-1.25f)) == real32x8(-1.0f)));
	EXPECT_TRUE(all_true(ceil(real32x8(-1.75f)) == real32x8(-1.0f)));
	EXPECT_TRUE(all_true(round(real32x8(1.25)) == real32x8(1.0f)));
	EXPECT_TRUE(all_true(round(real32x8(1.75)) == real32x8(2.0f)));
	EXPECT_TRUE(all_true(round(real32x8(-1.25)) == real32x8(-1.0f)));
	EXPECT_TRUE(all_true(round(real32x8(-1.75)) == real32x8(-2.0f)));

	static constexpr real32 k_epsilon = 0.00001f;
	EXPECT_TRUE(all_true(abs(log(real32x8(5.0f)) - real32x8(log(5.0f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(exp(real32x8(5.0f)) - real32x8(exp(5.0f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(sqrt(real32x8(5.0f)) - real32x8(sqrt(5.0f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(pow(real32x8(2.5f), real32x8(3.5f)) - real32x8(pow(2.5f, 3.5f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(sin(real32x8(2.0f)) - real32x8(sin(2.0f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(cos(real32x8(2.0f)) - real32x8(cos(2.0f))) <= real32x8(k_epsilon)));

	real32x8 sin_result;
	real32x8 cos_result;
	sincos(real32x8(2.0f), sin_result, cos_result);
	EXPECT_TRUE(all_true(abs(sin_result - real32x8(sin(2.0f))) <= real32x8(k_epsilon)));
	EXPECT_TRUE(all_true(abs(cos_result - real32x8(cos(2.0f))) <= real32x8(k_epsilon)));
}

TEST(Math, Int32x8) {
	ALIGNAS_SIMD_256 static constexpr int32 k_elements[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	ALIGNAS_SIMD_256 static constexpr int32 k_constant_elements[] = { 1, 1, 1, 1, 1, 1, 1, 1 };

	EXPECT_TRUE(all_true(int32x8(k_elements) == int32x8(1, 2, 3, 4, 5, 6, 7, 8)));
	EXPECT_TRUE(all_true(int32x8(k_constant_elements) == int32x8(1)));

	ALIGNAS_SIMD_256 int32 elements[8];
	zero_type(elements, array_count(elements));
	int32x8(k_elements).store(elements);
	EXPECT_EQ(memcmp(elements, k_elements, sizeof(elements)), 0);

	EXPECT_TRUE(all_true(int32x8(1, 2, 3, 4, 5, 6, 7, 8).sum_elements() == int32x8(36)));
	EXPECT_TRUE(all_true(~int32x8(0) == int32x8(-1)));
	EXPECT_TRUE(all_true(-int32x8(1) == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(1) + int32x8(2) == int32x8(3)));
	EXPECT_TRUE(all_true(int32x8(1) - int32x8(2) == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(2) * int32x8(3) == int32x8(6)));
	EXPECT_TRUE(all_true((int32x8(0x0f0f0f0f) & int32x8(0x0000ffff)) == int32x8(0x00000f0f)));
	EXPECT_TRUE(all_true((int32x8(0x0f0f0f0f) | int32x8(0x0000ffff)) == int32x8(0x0f0fffff)));
	EXPECT_TRUE(all_true((int32x8(0x0f0f0f0f) ^ int32x8(0x0000ffff)) == int32x8(0x0f0ff0f0)));
	EXPECT_TRUE(all_true(int32x8(1) << 4 == int32x8(16)));
	EXPECT_TRUE(all_true(int32x8(1) << int32x8(4) == int32x8(16)));
	EXPECT_TRUE(all_true(int32x8(1) << 64 == int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(1) << int32x8(64) == int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(16) >> 4 == int32x8(1)));
	EXPECT_TRUE(all_true(int32x8(16) >> int32x8(4) == int32x8(1)));
	EXPECT_TRUE(all_true(int32x8(16) >> 64 == int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(16) >> int32x8(64) == int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(-16) >> 4 == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(-16) >> int32x8(4) == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(-16) >> 64 == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(-16) >> int32x8(64) == int32x8(-1)));
	EXPECT_TRUE(all_true(int32x8(-16).shift_right_unsigned(4) == int32x8(0x0fffffff)));
	EXPECT_TRUE(all_true(int32x8(-16).shift_right_unsigned(int32x8(4)) == int32x8(0x0fffffff)));
	EXPECT_TRUE(all_true(int32x8(-16).shift_right_unsigned(64) == int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(-16).shift_right_unsigned(int32x8(64)) == int32x8(0)));

	EXPECT_TRUE(all_true(int32x8(-1, -1, -1, -1, -1, -1, -1, -1)));
	EXPECT_FALSE(all_true(int32x8(0, -1, -1, -1, -1, -1, -1, -1)));
	EXPECT_TRUE(all_false(int32x8(0, 0, 0, 0, 0, 0, 0, 0)));
	EXPECT_FALSE(all_false(int32x8(0, -1, 0, 0, 0, 0, 0, 0)));
	EXPECT_TRUE(any_true(int32x8(0, 0, -1, 0, 0, 0, 0, 0)));
	EXPECT_FALSE(any_true(int32x8(0, 0, 0, 0, 0, 0, 0, 0)));
	EXPECT_TRUE(any_false(int32x8(-1, -1, -1, 0, -1, -1, -1, -1)));
	EXPECT_FALSE(any_false(int32x8(-1, -1, -1, -1, -1, -1, -1, -1)));

	EXPECT_TRUE(all_true(int32x8(1) == int32x8(1)));
	EXPECT_TRUE(all_false(int32x8(0) == int32x8(1)));
	EXPECT_TRUE(all_true(int32x8(0) != int32x8(1)));
	EXPECT_TRUE(all_false(int32x8(1) != int32x8(1)));
	EXPECT_TRUE(all_true(int32x8(1) > int32x8(0)));
	EXPECT_TRUE(all_false(int32x8(0) > int32x8(1)));
	EXPECT_TRUE(all_false(int32x8(0) > int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(0) < int32x8(1)));
	EXPECT_TRUE(all_false(int32x8(1) < int32x8(0)));
	EXPECT_TRUE(all_false(int32x8(0) < int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(1) >= int32x8(0)));
	EXPECT_TRUE(all_false(int32x8(0) >= int32x8(1)));
	EXPECT_TRUE(all_true(int32x8(0) >= int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(0) <= int32x8(1)));
	EXPECT_TRUE(all_false(int32x8(1) <= int32x8(0)));
	EXPECT_TRUE(all_true(int32x8(0) <= int32x8(0)));

	EXPECT_TRUE(all_true(abs(int32x8(-1)) == int32x8(1)));
	EXPECT_TRUE(all_true(min(int32x8(1), int32x8(2)) == int32x8(1)));
	EXPECT_TRUE(all_true(max(int32x8(1), int32x8(2)) == int32x8(2)));
}

#endif // IS_TRUE(SIMD_256_ENABLED)

TEST(Math, SanitizeInfNan) {
	EXPECT_EQ(sanitize_inf_nan(1.0f), 1.0f);
	EXPECT_EQ(sanitize_inf_nan(-1.0f), -1.0f);
	EXPECT_EQ(sanitize_inf_nan(std::numeric_limits<real32>::infinity()), 0.0f);
	EXPECT_EQ(sanitize_inf_nan(-std::numeric_limits<real32>::infinity()), 0.0f);
	EXPECT_EQ(sanitize_inf_nan(std::numeric_limits<real32>::quiet_NaN()), 0.0f);
}
