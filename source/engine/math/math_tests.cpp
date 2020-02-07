#include "engine/math/math_tests.h"

#ifdef _DEBUG

#include "engine/math/math.h"

#include <iostream>

static void test(const char *name, const c_real32_4 &actual, const c_real32_4 &expected) {
	ALIGNAS_SIMD s_static_array<real32, k_simd_block_elements> actual_val;
	ALIGNAS_SIMD s_static_array<real32, k_simd_block_elements> expected_val;
	actual.store(actual_val.get_elements());
	expected.store(expected_val.get_elements());

	static const real32 k_test_epsilon = 0.0001f;
	bool valid = true;
	for (size_t index = 0; valid && index < k_simd_block_elements; index++) {
		valid = std::abs(expected_val[index] - actual_val[index]) <= k_test_epsilon;
	}

	if (!valid) {
		std::cerr << "TEST FAILED: '" << name <<
			"', expected = [" <<
			expected_val[0] << ", " <<
			expected_val[1] << ", " <<
			expected_val[2] << ", " <<
			expected_val[3] <<
			"], actual = [" <<
			actual_val[0] << ", " <<
			actual_val[1] << ", " <<
			actual_val[2] << ", " <<
			actual_val[3] << "]\n";
	}
}

static void test(const char *name, const c_int32_4 &actual, const c_int32_4 &expected) {
	ALIGNAS_SIMD s_static_array<int32, k_simd_block_elements> actual_val;
	ALIGNAS_SIMD s_static_array<int32, k_simd_block_elements> expected_val;
	actual.store(actual_val.get_elements());
	expected.store(expected_val.get_elements());

	bool valid = true;
	for (size_t index = 0; valid && index < k_simd_block_elements; index++) {
		valid = (expected_val[index] == actual_val[index]);
	}

	if (!valid) {
		std::cerr << "TEST FAILED: '" << name <<
			"', expected = [" <<
			expected_val[0] << ", " <<
			expected_val[1] << ", " <<
			expected_val[2] << ", " <<
			expected_val[3] <<
			"], actual = [" <<
			actual_val[0] << ", " <<
			actual_val[1] << ", " <<
			actual_val[2] << ", " <<
			actual_val[3] << "]\n";
	}
}

static void test(const char *name, int32 actual, int32 expected) {
	if (expected != actual) {
		std::cerr << "TEST FAILED: '" << name <<
			"', expected = [" << expected << "], actual = [" << actual << "]\n";
	}
}

void run_math_tests() {
	// c_real32_4:

	{
		ALIGNAS_SIMD real32 k_set_elements[] = { 1.0f, 2.0f, 3.0f, 4.0f };
		c_real32_4 set_test;
		set_test.load(k_set_elements);
		test("set", set_test, c_real32_4(1.0f, 2.0f, 3.0f, 4.0f));

		ALIGNAS_SIMD real32 k_set_constant_elements[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		c_real32_4 set_constant_test;
		set_constant_test.load(k_set_constant_elements);
		test("set_constant", set_constant_test, c_real32_4(1.0f));
	}

	test("sum_elements", c_real32_4(1.0f, 2.0f, 3.0f, 4.0f).sum_elements(), c_real32_4(10.0f));
	test("negate", -c_real32_4(1.0f), c_real32_4(-1.0f));
	test("add", c_real32_4(1.0f) + c_real32_4(2.0f), c_real32_4(3.0f));
	test("subtract", c_real32_4(1.0f) - c_real32_4(2.0f), c_real32_4(-1.0f));
	test("multiply", c_real32_4(2.0f) * c_real32_4(3.0f), c_real32_4(6.0f));
	test("divide", c_real32_4(8.0f) / c_real32_4(4.0f), c_real32_4(2.0f));
	test("mod", c_real32_4(8.0f) % c_real32_4(3.0f), c_real32_4(2.0f));

	test("eq_1", c_real32_4(1.0f) == c_real32_4(1.0f), c_int32_4(-1));
	test("eq_2", c_real32_4(0.0f) == c_real32_4(1.0f), c_int32_4(0));
	test("neq_1", c_real32_4(0.0f) != c_real32_4(1.0f), c_int32_4(-1));
	test("neq_2", c_real32_4(1.0f) != c_real32_4(1.0f), c_int32_4(0));
	test("gt_1", c_real32_4(1.0f) > c_real32_4(0.0f), c_int32_4(-1));
	test("gt_2", c_real32_4(0.0f) > c_real32_4(1.0f), c_int32_4(0));
	test("gt_3", c_real32_4(0.0f) > c_real32_4(0.0f), c_int32_4(0));
	test("lt_1", c_real32_4(0.0f) < c_real32_4(1.0f), c_int32_4(-1));
	test("lt_2", c_real32_4(1.0f) < c_real32_4(0.0f), c_int32_4(0));
	test("lt_3", c_real32_4(0.0f) < c_real32_4(0.0f), c_int32_4(0));
	test("ge_1", c_real32_4(1.0f) >= c_real32_4(0.0f), c_int32_4(-1));
	test("ge_2", c_real32_4(0.0f) >= c_real32_4(1.0f), c_int32_4(0));
	test("ge_3", c_real32_4(0.0f) >= c_real32_4(0.0f), c_int32_4(-1));
	test("le_1", c_real32_4(0.0f) <= c_real32_4(1.0f), c_int32_4(-1));
	test("le_2", c_real32_4(1.0f) <= c_real32_4(0.0f), c_int32_4(0));
	test("le_3", c_real32_4(0.0f) <= c_real32_4(0.0f), c_int32_4(-1));
	test("abs", abs(c_real32_4(-1.0f)), c_real32_4(1.0f));
	test("floor_1", floor(c_real32_4(1.75f)), c_real32_4(1.0f));
	test("floor_2", floor(c_real32_4(-1.75f)), c_real32_4(-2.0f));
	test("floor_3", floor(c_real32_4(-1.25f)), c_real32_4(-2.0f));
	test("ceil_1", ceil(c_real32_4(1.25f)), c_real32_4(2.0f));
	test("ceil_2", ceil(c_real32_4(-1.25f)), c_real32_4(-1.0f));
	test("ceil_2", ceil(c_real32_4(-1.75f)), c_real32_4(-1.0f));
	test("round_1", round(c_real32_4(1.25)), c_real32_4(1.0f));
	test("round_2", round(c_real32_4(1.75)), c_real32_4(2.0f));
	test("round_3", round(c_real32_4(-1.25)), c_real32_4(-1.0f));
	test("round_4", round(c_real32_4(-1.75)), c_real32_4(-2.0f));
	test("log", log(c_real32_4(5.0f)), c_real32_4(log(5.0f)));
	test("exp", exp(c_real32_4(5.0f)), c_real32_4(exp(5.0f)));
	test("sqrt", sqrt(c_real32_4(5.0f)), c_real32_4(sqrt(5.0f)));
	test("pow", pow(c_real32_4(2.5f), c_real32_4(3.5f)), c_real32_4(pow(2.5f, 3.5f)));
	test("sin", sin(c_real32_4(2.0f)), c_real32_4(sin(2.0f)));
	test("cos", cos(c_real32_4(2.0f)), c_real32_4(cos(2.0f)));

	{
		c_real32_4 sincos_sin;
		c_real32_4 sincos_cos;
		sincos(c_real32_4(2.0f), sincos_sin, sincos_cos);
		test("sincos_1", sincos_sin, c_real32_4(sin(2.0f)));
		test("sincos_2", sincos_cos, c_real32_4(cos(2.0f)));
	}

	test(
		"extract_0",
		extract<0>(c_real32_4(0.0f, 1.0f, 2.0f, 3.0f), c_real32_4(4.0f, 5.0f, 6.0f, 7.0f)),
		c_real32_4(0.0f, 1.0f, 2.0f, 3.0f));
	test(
		"extract_1",
		extract<1>(c_real32_4(0.0f, 1.0f, 2.0f, 3.0f), c_real32_4(4.0f, 5.0f, 6.0f, 7.0f)),
		c_real32_4(1.0f, 2.0f, 3.0f, 4.0f));
	test(
		"extract_2",
		extract<2>(c_real32_4(0.0f, 1.0f, 2.0f, 3.0f), c_real32_4(4.0f, 5.0f, 6.0f, 7.0f)),
		c_real32_4(2.0f, 3.0f, 4.0f, 5.0f));
	test(
		"extract_3",
		extract<3>(c_real32_4(0.0f, 1.0f, 2.0f, 3.0f), c_real32_4(4.0f, 5.0f, 6.0f, 7.0f)),
		c_real32_4(3.0f, 4.0f, 5.0f, 6.0f));
	test(
		"extract_4",
		extract<4>(c_real32_4(0.0f, 1.0f, 2.0f, 3.0f), c_real32_4(4.0f, 5.0f, 6.0f, 7.0f)),
		c_real32_4(4.0f, 5.0f, 6.0f, 7.0f));

	// c_int32_4:

	{
		ALIGNAS_SIMD int32 k_set_elements[] = { 1, 2, 3, 4 };
		c_int32_4 set_test;
		set_test.load(k_set_elements);
		test("set", set_test, c_int32_4(1, 2, 3, 4));

		ALIGNAS_SIMD int32 k_set_constant_elements[] = { 1, 1, 1, 1 };
		c_int32_4 set_constant_test;
		set_constant_test.load(k_set_constant_elements);
		test("set_constant", set_constant_test, c_int32_4(1));
	}

	test("negate", -c_int32_4(1), c_int32_4(-1));
	test("not", ~c_int32_4(0), c_int32_4(-1));
	test("add", c_int32_4(1) + c_int32_4(2), c_int32_4(3));
	test("subtract", c_int32_4(1) - c_int32_4(2), c_int32_4(-1));
	test("and", c_int32_4(0x0f0f0f0f) & c_int32_4(0x0000ffff), c_int32_4(0x00000f0f));
	test("or", c_int32_4(0x0f0f0f0f) | c_int32_4(0x0000ffff), c_int32_4(0x0f0fffff));
	test("xor", c_int32_4(0x0f0f0f0f) ^ c_int32_4(0x0000ffff), c_int32_4(0x0f0ff0f0));
	test("shl_1", c_int32_4(1) << 4, c_int32_4(16));
	test("shl_2", c_int32_4(1) << c_int32_4(4), c_int32_4(16));
	test("shr_1", c_int32_4(16) >> 4, c_int32_4(1));
	test("shr_2", c_int32_4(16) >> c_int32_4(4), c_int32_4(1));
	test("shru_1", c_int32_4(-16) >> 4, c_int32_4(-1));
	test("shru_2", c_int32_4(-16) >> c_int32_4(4), c_int32_4(-1));

	test("eq_1", c_int32_4(1) == c_int32_4(1), c_int32_4(-1));
	test("eq_2", c_int32_4(0) == c_int32_4(1), c_int32_4(0));
	test("neq_1", c_int32_4(0) != c_int32_4(1), c_int32_4(-1));
	test("neq_2", c_int32_4(1) != c_int32_4(1), c_int32_4(0));
	test("gt_1", c_int32_4(1) > c_int32_4(0), c_int32_4(-1));
	test("gt_2", c_int32_4(0) > c_int32_4(1), c_int32_4(0));
	test("gt_3", c_int32_4(0) > c_int32_4(0), c_int32_4(0));
	test("lt_1", c_int32_4(0) < c_int32_4(1), c_int32_4(-1));
	test("lt_2", c_int32_4(1) < c_int32_4(0), c_int32_4(0));
	test("lt_3", c_int32_4(0) < c_int32_4(0), c_int32_4(0));
	test("ge_1", c_int32_4(1) >= c_int32_4(0), c_int32_4(-1));
	test("ge_2", c_int32_4(0) >= c_int32_4(1), c_int32_4(0));
	test("ge_3", c_int32_4(0) >= c_int32_4(0), c_int32_4(-1));
	test("le_1", c_int32_4(0) <= c_int32_4(1), c_int32_4(-1));
	test("le_2", c_int32_4(1) <= c_int32_4(0), c_int32_4(0));
	test("le_3", c_int32_4(0) <= c_int32_4(0), c_int32_4(-1));
	test("abs", abs(c_int32_4(-1)), c_int32_4(1));
	test("min", min(c_int32_4(1), c_int32_4(2)), c_int32_4(1));
	test("max", max(c_int32_4(1), c_int32_4(2)), c_int32_4(2));
	test("mask_from_msb", mask_from_msb(c_int32_4(0, -1, -1, 0)), 6);

	test(
		"extract_0",
		extract<0>(c_int32_4(0, 1, 2, 3), c_int32_4(4, 5, 6, 7)),
		c_int32_4(0, 1, 2, 3));
	test(
		"extract_1",
		extract<1>(c_int32_4(0, 1, 2, 3), c_int32_4(4, 5, 6, 7)),
		c_int32_4(1, 2, 3, 4));
	test(
		"extract_2",
		extract<2>(c_int32_4(0, 1, 2, 3), c_int32_4(4, 5, 6, 7)),
		c_int32_4(2, 3, 4, 5));
	test(
		"extract_3",
		extract<3>(c_int32_4(0, 1, 2, 3), c_int32_4(4, 5, 6, 7)),
		c_int32_4(3, 4, 5, 6));
	test(
		"extract_4",
		extract<4>(c_int32_4(0, 1, 2, 3), c_int32_4(4, 5, 6, 7)),
		c_int32_4(4, 5, 6, 7));
}

#endif // _DEBUG