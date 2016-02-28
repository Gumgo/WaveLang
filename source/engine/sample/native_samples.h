#ifndef WAVELANG_NATIVE_SAMPLES_DEFINED__
#define WAVELANG_NATIVE_SAMPLES_DEFINED__

#include "common/common.h"

class c_sample;

enum e_native_sample {
	k_native_sample_sin,
	k_native_sample_sawtooth,
	k_native_sample_triangle,

	k_native_sample_count
};

const char *get_native_sample_prefix();
const char *get_native_sample_name(uint32 native_sample);
c_sample *build_native_sample(uint32 native_sample);

#endif // WAVELANG_NATIVE_SAMPLES_DEFINED__