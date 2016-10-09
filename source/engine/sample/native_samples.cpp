#include "engine/sample/native_samples.h"
#include "engine/sample/sample.h"

#define GENERATE_NATIVE_SAMPLES 0

#if IS_TRUE(GENERATE_NATIVE_SAMPLES)
#include <fstream>
#endif // IS_TRUE(GENERATE_NATIVE_SAMPLES)

#define NATIVE_SAMPLE_PREFIX "__native_"

// Native samples are generated at A440
static const uint32 k_native_sample_frequency = 440;

static const uint32 k_max_native_sample_mipmap_levels = 16;

#include "native_samples.inl"

typedef c_wrapped_array_const<real32> c_sample_data_array;
typedef c_wrapped_array_const<c_sample_data_array> c_mipmap_data_array;

struct s_native_sample_data {
	const char *name;
	bool is_mipmap;
	bool phase_shift_enabled;
	c_sample_data_array sample_data;
	c_mipmap_data_array mipmap_data;
};

static s_native_sample_data make_native_sample(
	const char *name,
	bool phase_shift_enabled,
	c_sample_data_array sample_data) {
	s_native_sample_data native_sample_data;
	native_sample_data.name = name;
	native_sample_data.is_mipmap = false;
	native_sample_data.phase_shift_enabled = phase_shift_enabled;
	native_sample_data.sample_data = sample_data;
	return native_sample_data;
}

static s_native_sample_data make_native_sample_mipmap(
	const char *name,
	bool phase_shift_enabled,
	c_mipmap_data_array mipmap_data) {
	s_native_sample_data native_sample_data;
	native_sample_data.name = name;
	native_sample_data.is_mipmap = true;
	native_sample_data.phase_shift_enabled = phase_shift_enabled;
	native_sample_data.mipmap_data = mipmap_data;
	return native_sample_data;
}

static const s_native_sample_data k_native_sample_data[] = {
	make_native_sample(NATIVE_SAMPLE_PREFIX "sin", false,
		c_sample_data_array::construct(k_native_sample_sin_data)),

	make_native_sample_mipmap(NATIVE_SAMPLE_PREFIX "sawtooth", true,
		c_mipmap_data_array::construct(k_native_sample_sawtooth_data)),

	make_native_sample_mipmap(NATIVE_SAMPLE_PREFIX "triangle", false,
		c_mipmap_data_array::construct(k_native_sample_triangle_data))
};

static_assert(NUMBEROF(k_native_sample_data) == k_native_sample_count, "Native sample initialization data mismatch");

const char *get_native_sample_prefix() {
	return NATIVE_SAMPLE_PREFIX;
}

const char *get_native_sample_name(uint32 native_sample) {
	wl_assert(VALID_INDEX(native_sample, k_native_sample_count));
	const s_native_sample_data &data = k_native_sample_data[native_sample];
	return data.name;
}

c_sample *build_native_sample(uint32 native_sample) {
	wl_assert(VALID_INDEX(native_sample, k_native_sample_count));

	const s_native_sample_data &data = k_native_sample_data[native_sample];
	c_sample *sample = new c_sample();

	if (!data.is_mipmap) {
		// This sample consists of a single array of sample data
		wl_assert(data.sample_data.get_count() > 0);

		uint32 sample_rate = cast_integer_verify<uint32>(data.sample_data.get_count()) * k_native_sample_frequency;
		sample->initialize(sample_rate, 1, cast_integer_verify<uint32>(data.sample_data.get_count()),
			k_sample_loop_mode_loop, 0, cast_integer_verify<uint32>(data.sample_data.get_count()),
			data.phase_shift_enabled, data.sample_data);
	} else {
		// This sample consists of a mipmap of sample data arrays. Construct sub-samples for each one, and place them
		// into a wrapped array to be passed to the owner mipmap sample.
		wl_assert(data.mipmap_data.get_count() > 0);
		wl_assert(data.mipmap_data.get_count() <= k_max_native_sample_mipmap_levels);

		s_static_array<c_sample *, k_max_native_sample_mipmap_levels> sub_samples;
		for (uint32 index = 0; index < data.mipmap_data.get_count(); index++) {
			c_sample *sub_sample = new c_sample();

			c_sample_data_array sub_sample_data = data.mipmap_data[index];
			uint32 sample_rate = cast_integer_verify<uint32>(sub_sample_data.get_count()) * k_native_sample_frequency;
			sub_sample->initialize_for_mipmap(sample_rate, 1, cast_integer_verify<uint32>(sub_sample_data.get_count()),
				k_sample_loop_mode_loop, 0, cast_integer_verify<uint32>(sub_sample_data.get_count()),
				data.phase_shift_enabled, sub_sample_data);

			sub_samples[index] = sub_sample;
		}

		c_wrapped_array_const<c_sample *> sub_sample_array(sub_samples.get_elements(), data.mipmap_data.get_count());
		sample->initialize(sub_sample_array);
	}

	return sample;
}

#if IS_TRUE(GENERATE_NATIVE_SAMPLES)
namespace native_sample_generator {
	static const uint32 k_native_sample_size = 256;
	static const uint32 k_native_sample_min_size = 4;
	static const real64 k_pi = 3.1415926535897932384626433832795;

	static void generate_sin(std::ofstream &out) {
		real32 unused;

		// No mipmap needed for sin
		out << "static const real32 k_native_sample_sin_data[] = {\n";

		for (uint32 index = 0; index < k_native_sample_size; index++) {
			real64 t = 2.0 * k_pi * static_cast<real64>(index) / static_cast<real64>(k_native_sample_size);
			real64 value = std::sin(t);
			real32 value_32 = static_cast<real32>(value);

			// Hack to add decimal point if it's missing:
			out << "\t" << value_32 << (std::modf(value_32, &unused) == 0.0f ? ".0" : "") << "f,\n";
		}

		out << "};\n\n";
	}

	static void generate_sawtooth(std::ofstream &out) {
		uint32 mip_levels = 0;
		uint32 mip_size = 256;
		real32 unused;

		while (mip_size >= k_native_sample_min_size) {
			out << "static const real32 k_native_sample_sawtooth_data_" << mip_levels << "[] = {\n";

			for (uint32 index = 0; index < mip_size; index++) {
				real64 value = 0.0;

				// Add sine waves up to the nyquist frequency
				for (uint32 k = 1; k <= (mip_size / 2); k++) {
					real64 t = 2.0 * k_pi * static_cast<real64>(k) *
						static_cast<real64>(index) / static_cast<real64>(mip_size);
					value += std::sin(t) / static_cast<real64>(k);
				}

				value *= (-2.0 / k_pi);

				real32 value_32 = static_cast<real32>(value);
				// Hack to add decimal point if it's missing:
				out << "\t" << value_32 << (std::modf(value_32, &unused) == 0.0f ? ".0" : "") << "f,\n";
			}

			out << "};\n\n";

			mip_levels++;
			mip_size /= 2;
		}

		out << "static const c_wrapped_array_const<real32> k_native_sample_sawtooth_data[] = {\n";

		for (uint32 mip_index = 0; mip_index < mip_levels; mip_index++) {
			out << "\tc_wrapped_array_const<real32>::construct(k_native_sample_sawtooth_data_" << mip_index << "),\n";
		}

		out << "};\n\n";
	}

	static void generate_triangle(std::ofstream &out) {
		uint32 mip_levels = 0;
		uint32 mip_size = 256;
		real32 unused;

		while (mip_size >= k_native_sample_min_size) {
			out << "static const real32 k_native_sample_triangle_data_" << mip_levels << "[] = {\n";

			for (uint32 index = 0; index < mip_size; index++) {
				real64 value = 0.0;

				// Add sine waves up to the nyquist frequency
				for (uint32 k = 1; k <= (mip_size / 2); k++) {
					if (k % 2 == 1) {
						real64 negate = ((((k - 1) / 2) % 2) == 0) ? 1.0 : -1.0;
						real64 k_64 = static_cast<real64>(k);
						real64 t = 2.0 * k_pi * k_64 * static_cast<real64>(index) / static_cast<real64>(mip_size);
						value += (negate * std::sin(t) / (k_64 * k_64));
					} else {
						// Even terms are zero
					}
				}

				value *= (8.0 / (k_pi * k_pi));

				real32 value_32 = static_cast<real32>(value);
				// Hack to add decimal point if it's missing:
				out << "\t" << value_32 << (std::modf(value_32, &unused) == 0.0f ? ".0" : "") << "f,\n";
			}

			out << "};\n\n";

			mip_levels++;
			mip_size /= 2;
		}

		out << "static const c_wrapped_array_const<real32> k_native_sample_triangle_data[] = {\n";

		for (uint32 mip_index = 0; mip_index < mip_levels; mip_index++) {
			out << "\tc_wrapped_array_const<real32>::construct(k_native_sample_triangle_data_" << mip_index << "),\n";
		}

		out << "};\n\n";
	}

	// Pulse wave is made by combining two sawtooth waves, square wave is just pulse wave with 0.5 phase
}

int main(int argc, char **argv) {
	std::ofstream out("native_samples.inl");
	native_sample_generator::generate_sin(out);
	native_sample_generator::generate_sawtooth(out);
	native_sample_generator::generate_triangle(out);

	return 0;
}
#endif // IS_TRUE(GENERATE_NATIVE_SAMPLES)
