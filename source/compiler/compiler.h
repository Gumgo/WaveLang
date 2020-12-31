#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

// The entry point to the wavelang compiler

class c_instrument;

class c_compiler {
public:
	static c_instrument *compile(
		c_compiler_context &context,
		const char *source_filename);
};
