#pragma once

#include "common/common.h"

// The entry point to the wavelang compiler

class c_instrument;

class c_compiler {
public:
	static c_instrument *compile(
		c_wrapped_array<void *> native_module_library_contexts,
		const char *source_filename);
};
