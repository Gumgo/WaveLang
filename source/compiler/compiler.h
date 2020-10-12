#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

// The entry point to the wavelang compiler

class c_instrument;

class c_compiler {
public:
	static s_compiler_result compile(
		c_wrapped_array<void *> native_module_library_contexts,
		const char *root_path,
		const char *source_filename,
		c_instrument *instrument_out);
};

