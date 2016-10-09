#ifndef WAVELANG_COMPILER_COMPILER_H__
#define WAVELANG_COMPILER_COMPILER_H__

#include "common/common.h"

#include "compiler/compiler_utility.h"

// The entry point to the wavelang compiler

class c_execution_graph;

class c_compiler {
public:
	static s_compiler_result compile(const char *root_path, const char *source_filename,
		c_execution_graph *out_execution_graph);
};

#endif // WAVELANG_COMPILER_COMPILER_H__
