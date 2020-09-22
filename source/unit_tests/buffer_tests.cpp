#include "common/common.h"

#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_iterator.h"

#include <gtest/gtest.h>

TEST(Buffer, Cico) {
	// Choose a weird size to test end iteration
	static constexpr size_t k_buffer_size = 259;

	static constexpr real32 k_real_constant = 3.0f;
	static constexpr bool k_bool_constant = true;

	ALIGNAS_SIMD real32 real_buffer_memory_in[align_size(k_buffer_size, k_simd_alignment)];
	zero_type(real_buffer_memory_in, array_count(real_buffer_memory_in));

	ALIGNAS_SIMD real32 real_buffer_memory_out[align_size(k_buffer_size, k_simd_alignment)];
	zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));

	ALIGNAS_SIMD int32 bool_buffer_memory_in[align_size(bool_buffer_int32_count(k_buffer_size), k_simd_alignment)];
	zero_type(bool_buffer_memory_in, array_count(bool_buffer_memory_in));

	ALIGNAS_SIMD int32 bool_buffer_memory_out[align_size(bool_buffer_int32_count(k_buffer_size), k_simd_alignment)];
	zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));

	c_buffer real_dynamic_in_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_real));
	real_dynamic_in_untyped.set_memory(real_buffer_memory_in);
	const c_real_buffer *real_dynamic_in = &real_dynamic_in_untyped.get_as<c_real_buffer>();

	c_buffer real_constant_in_untyped = c_real_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_real),
		k_real_constant);
	const c_real_buffer *real_constant_in = &real_constant_in_untyped.get_as<c_real_buffer>();

	c_buffer real_dynamic_out_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_real));
	real_dynamic_out_untyped.set_memory(real_buffer_memory_out);
	c_real_buffer *real_dynamic_out = &real_dynamic_out_untyped.get_as<c_real_buffer>();

	c_buffer bool_dynamic_in_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_bool));
	bool_dynamic_in_untyped.set_memory(bool_buffer_memory_in);
	const c_bool_buffer *bool_dynamic_in = &bool_dynamic_in_untyped.get_as<c_bool_buffer>();

	c_buffer bool_constant_in_untyped = c_bool_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_bool),
		k_bool_constant);
	const c_bool_buffer *bool_constant_in = &bool_constant_in_untyped.get_as<c_bool_buffer>();

	c_buffer bool_dynamic_out_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_bool));
	bool_dynamic_out_untyped.set_memory(bool_buffer_memory_out);
	c_bool_buffer *bool_dynamic_out = &bool_dynamic_out_untyped.get_as<c_bool_buffer>();

	// Basic tests:

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, real_dynamic_in, real_constant_in, real_dynamic_out,
			[&](size_t i, real32 a, real32 b, real32 &c) {
				c = a + b;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(real_dynamic_out->is_constant());
	}

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, true>(k_buffer_size, real_dynamic_in, real_constant_in, real_dynamic_out,
			[&](size_t i, real32 a, real32 b, real32 &c) {
				c = a + b;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(real_dynamic_out->is_constant());
	}

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, real_constant_in, real_dynamic_out,
			[&](size_t i, real32 a, real32 &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(real_dynamic_out->is_constant());
	}

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, true>(k_buffer_size, real_constant_in, real_dynamic_out,
			[&](size_t i, real32 a, real32 &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(real_dynamic_out->is_constant());
		EXPECT_EQ(real_dynamic_out->get_constant(), k_real_constant);
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, bool_dynamic_in, bool_constant_in, bool_dynamic_out,
			[&](size_t i, bool a, bool b, bool &c) {
				c = a + b;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(bool_dynamic_out->is_constant());
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, true>(k_buffer_size, bool_dynamic_in, bool_constant_in, bool_dynamic_out,
			[&](size_t i, bool a, bool b, bool &c) {
				c = a + b;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(bool_dynamic_out->is_constant());
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, bool_constant_in, bool_dynamic_out,
			[&](size_t i, bool a, bool &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, k_buffer_size);
		EXPECT_FALSE(bool_dynamic_out->is_constant());
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, true>(k_buffer_size, bool_constant_in, bool_dynamic_out,
			[&](size_t i, bool a, bool &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(bool_dynamic_out->is_constant());
		EXPECT_EQ(bool_dynamic_out->get_constant(), k_bool_constant);
	}

	// Test strides:

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<4, true>(k_buffer_size, real_constant_in, real_dynamic_out,
			[&](size_t i, const real32x4 &a, real32x4 &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(real_dynamic_out->is_constant());
		EXPECT_EQ(real_dynamic_out->get_constant(), k_real_constant);
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<4, true>(k_buffer_size, bool_constant_in, bool_dynamic_out,
			[&](size_t i, const int32x4 &a, int32x4 &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(bool_dynamic_out->is_constant());
		EXPECT_EQ(bool_dynamic_out->get_constant(), k_bool_constant);
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<128, true>(k_buffer_size, bool_constant_in, bool_dynamic_out,
			[&](size_t i, const int32x4 &a, int32x4 &b) {
				b = a;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(bool_dynamic_out->is_constant());
		EXPECT_EQ(bool_dynamic_out->get_constant(), k_bool_constant);
	}

	// Mixed types:

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		size_t iterations = 0;
		iterate_buffers<1, true>(
			k_buffer_size, real_constant_in, bool_constant_in, real_dynamic_out, bool_dynamic_out,
			[&](size_t i, real32 a, bool b, real32 &c, bool &d) {
				c = a;
				d = b;
				iterations++;
			});
		EXPECT_EQ(iterations, 1);
		EXPECT_TRUE(real_dynamic_out->is_constant());
		EXPECT_EQ(real_dynamic_out->get_constant(), k_real_constant);
		EXPECT_TRUE(bool_dynamic_out->is_constant());
		EXPECT_EQ(bool_dynamic_out->get_constant(), k_bool_constant);
	}
}

TEST(Buffer, Constant) {
	// Choose a weird size to test end iteration
	static constexpr size_t k_buffer_size = 259;

	static constexpr real32 k_real_constant = 3.0f;
	static constexpr bool k_bool_constant = true;

	ALIGNAS_SIMD real32 real_buffer_memory[align_size(k_buffer_size, k_simd_alignment)];
	zero_type(real_buffer_memory, array_count(real_buffer_memory));

	ALIGNAS_SIMD int32 bool_buffer_memory[align_size(bool_buffer_int32_count(k_buffer_size), k_simd_alignment)];
	zero_type(bool_buffer_memory, array_count(bool_buffer_memory));

	c_buffer real_compile_time_constant_buffer_untyped = c_real_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_real),
		k_real_constant);
	const c_real_buffer *real_compile_time_constant_buffer =
		&real_compile_time_constant_buffer_untyped.get_as<c_real_buffer>();

	c_buffer real_constant_buffer_untyped = c_real_buffer::construct(c_task_data_type(e_task_primitive_type::k_real));
	real_constant_buffer_untyped.set_memory(real_buffer_memory);
	real_constant_buffer_untyped.get_as<c_real_buffer>().assign_constant(k_real_constant);
	const c_real_buffer *real_constant_buffer = &real_constant_buffer_untyped.get_as<c_real_buffer>();

	c_buffer bool_compile_time_constant_buffer_untyped = c_bool_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_bool),
		k_bool_constant);
	const c_bool_buffer *bool_compile_time_constant_buffer =
		&bool_compile_time_constant_buffer_untyped.get_as<c_bool_buffer>();

	c_buffer bool_constant_buffer_untyped = c_bool_buffer::construct(c_task_data_type(e_task_primitive_type::k_bool));
	bool_constant_buffer_untyped.set_memory(bool_buffer_memory);
	bool_constant_buffer_untyped.get_as<c_bool_buffer>().assign_constant(k_bool_constant);
	const c_bool_buffer *bool_constant_buffer = &bool_constant_buffer_untyped.get_as<c_bool_buffer>();

	const c_real_buffer *real_buffers[] = {
		real_compile_time_constant_buffer,
		real_constant_buffer
	};

	for (const c_real_buffer *buffer : real_buffers) {
		EXPECT_EQ(buffer->get_constant(), k_real_constant);
		EXPECT_TRUE(all_true(real32x4(buffer->get_data()) == real32x4(k_real_constant)));

		iterate_buffers<1, false>(k_buffer_size, buffer,
			[](size_t i, real32 x) {
				EXPECT_EQ(x, k_real_constant);
			});
		iterate_buffers<4, false>(k_buffer_size, buffer,
			[](size_t i, const real32x4 &x) {
				EXPECT_TRUE(all_true(x == real32x4(k_real_constant)));
			});
	}

	const c_bool_buffer *bool_buffers[] = {
		bool_compile_time_constant_buffer,
		bool_constant_buffer
	};

	for (const c_bool_buffer *buffer : bool_buffers) {
		EXPECT_EQ(buffer->get_constant(), k_bool_constant);
		EXPECT_TRUE(all_true(int32x4(buffer->get_data()) == -static_cast<int32>(k_bool_constant)));

		iterate_buffers<1, false>(k_buffer_size, buffer,
			[](size_t i, bool x) {
				EXPECT_EQ(x, k_bool_constant);
			});
		iterate_buffers<4, false>(k_buffer_size, buffer,
			[](size_t i, const int32x4 &x) {
				EXPECT_TRUE(all_true(x == -int32x4(static_cast<int32>(k_bool_constant))));
			});
		iterate_buffers<128, false>(k_buffer_size, buffer,
			[](size_t i, const int32x4 &x) {
				EXPECT_TRUE(all_true(x == -int32x4(static_cast<int32>(k_bool_constant))));
			});
	}
}

TEST(Buffer, TerminateEarly) {
	static constexpr size_t k_buffer_size = 259;

	c_buffer buffer = c_real_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_real),
		1.0f);
	const c_real_buffer *real_buffer = &buffer.get_as<c_real_buffer>();

	{
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, real_buffer,
			[&](size_t i, real32 a) {
				iterations++;
				return true;
			});
		EXPECT_EQ(iterations, k_buffer_size);
	}

	{
		size_t iterations = 0;
		iterate_buffers<1, false>(k_buffer_size, real_buffer,
			[&](size_t i, real32 a) {
				iterations++;
				return i < 5;
			});
		EXPECT_EQ(iterations, 6);
	}

	{
		size_t iterations = 0;
		iterate_buffers<4, false>(k_buffer_size, real_buffer,
			[&](size_t i, const real32x4 &a) {
				iterations++;
				return i < 10;
			});
		EXPECT_EQ(iterations, 4);
	}
}

TEST(Buffer, LastIterationFunction) {
	c_buffer buffer = c_real_buffer::construct_compile_time_constant(
		c_task_data_type(e_task_primitive_type::k_real),
		1.0f);
	const c_real_buffer *real_buffer = &buffer.get_as<c_real_buffer>();

	{
		size_t iterations = 0;
		bool last_iteration_called = false;
		iterate_buffers<4, false>(16, real_buffer,
			[&](size_t i, const real32x4 &a) {
				iterations++;
			},
			[&](size_t i, const real32x4 &a) {
				last_iteration_called = true;
			});
		EXPECT_EQ(iterations, 4);
		EXPECT_FALSE(last_iteration_called);
	}

	{
		size_t iterations = 0;
		bool last_iteration_called = false;
		iterate_buffers<4, false>(18, real_buffer,
			[&](size_t i, const real32x4 &a) {
				iterations++;
			},
			[&](size_t i, const real32x4 &a) {
				EXPECT_EQ(i, 16);
				last_iteration_called = true;
			});
		EXPECT_EQ(iterations, 4);
		EXPECT_TRUE(last_iteration_called);
	}
}

TEST(Buffer, IterationOrder) {
	static constexpr size_t k_buffer_size = 259;

	ALIGNAS_SIMD real32 real_buffer_memory_in[align_size(k_buffer_size, k_simd_alignment)];
	zero_type(real_buffer_memory_in, array_count(real_buffer_memory_in));

	ALIGNAS_SIMD real32 real_buffer_memory_out[align_size(k_buffer_size, k_simd_alignment)];
	zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));

	ALIGNAS_SIMD int32 bool_buffer_memory_in[align_size(bool_buffer_int32_count(k_buffer_size), k_simd_alignment)];
	zero_type(bool_buffer_memory_in, array_count(bool_buffer_memory_in));

	ALIGNAS_SIMD int32 bool_buffer_memory_out[align_size(bool_buffer_int32_count(k_buffer_size), k_simd_alignment)];
	zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));

	c_buffer real_in_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_real));
	real_in_untyped.set_memory(real_buffer_memory_in);
	const c_real_buffer *real_in = &real_in_untyped.get_as<c_real_buffer>();

	c_buffer real_out_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_real));
	real_out_untyped.set_memory(real_buffer_memory_out);
	c_real_buffer *real_out = &real_out_untyped.get_as<c_real_buffer>();

	c_buffer bool_in_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_bool));
	bool_in_untyped.set_memory(bool_buffer_memory_in);
	const c_bool_buffer *bool_in = &bool_in_untyped.get_as<c_bool_buffer>();

	c_buffer bool_out_untyped = c_buffer::construct(c_task_data_type(e_task_primitive_type::k_bool));
	bool_out_untyped.set_memory(bool_buffer_memory_out);
	c_bool_buffer *bool_out = &bool_out_untyped.get_as<c_bool_buffer>();

	// Real input buffer is 0, 1, 2, ...
	for (size_t index = 0; index < k_buffer_size; index++) {
		real_buffer_memory_in[index] = static_cast<real32>(index);
	}

	// Bool input buffer is a mod pattern
	for (size_t index = 0; index < k_buffer_size; index++) {
		int32 value =
			static_cast<int32>(index % 3 == 0)
			^ static_cast<int32>(index % 7 == 0)
			^ static_cast<int32>(index % 19 == 0)
			^ static_cast<int32>(index % 31 == 0);
		bool_buffer_memory_in[index / 32] |= value << (index % 32);
	}

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		iterate_buffers<1, false>(k_buffer_size, real_in, real_out,
			[](size_t i, real32 a, real32 &b) {
				b = a;
			});
		for (size_t index = 0; index < array_count(real_buffer_memory_in); index++) {
			EXPECT_EQ(real_buffer_memory_in[index], real_buffer_memory_out[index]);
		}
	}

	{
		zero_type(real_buffer_memory_out, array_count(real_buffer_memory_out));
		iterate_buffers<4, false>(k_buffer_size, real_in, real_out,
			[](size_t i, const real32x4 &a, real32x4 &b) {
				b = a;
			});
		for (size_t index = 0; index < array_count(real_buffer_memory_in); index++) {
			EXPECT_EQ(real_buffer_memory_in[index], real_buffer_memory_out[index]);
		}
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		iterate_buffers<1, false>(k_buffer_size, bool_in, bool_out,
			[](size_t i, bool a, bool &b) {
				b = a;
			});
		for (size_t index = 0; index < array_count(bool_buffer_memory_in); index++) {
			EXPECT_EQ(bool_buffer_memory_in[index], bool_buffer_memory_out[index]);
		}
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		iterate_buffers<4, false>(k_buffer_size, bool_in, bool_out,
			[](size_t i, const int32x4 &a, int32x4 &b) {
				b = a;
			});
		for (size_t index = 0; index < array_count(bool_buffer_memory_in); index++) {
			EXPECT_EQ(bool_buffer_memory_in[index], bool_buffer_memory_out[index]);
		}
	}

	{
		zero_type(bool_buffer_memory_out, array_count(bool_buffer_memory_out));
		iterate_buffers<128, false>(k_buffer_size, bool_in, bool_out,
			[](size_t i, const int32x4 &a, int32x4 &b) {
				b = a;
			});
		for (size_t index = 0; index < array_count(bool_buffer_memory_in); index++) {
			EXPECT_EQ(bool_buffer_memory_in[index], bool_buffer_memory_out[index]);
		}
	}
}
