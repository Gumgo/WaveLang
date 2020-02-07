#ifndef WAVELANG_COMPILER_COMPILER_H__
#define WAVELANG_COMPILER_COMPILER_H__

#include "common/common.h"

#include "compiler/compiler_utility.h"

// The entry point to the wavelang compiler

class c_instrument;

class c_compiler {
public:
	static s_compiler_result compile(
		const char *root_path,
		const char *source_filename,
		c_instrument *out_instrument);
};

#endif // WAVELANG_COMPILER_COMPILER_H__
