#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

class c_parser {
public:
	static void initialize();
	static void deinitialize();

	static bool process(c_compiler_context &context, h_compiler_source_file source_file_handle);
};

