#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

class c_importer {
public:
	static void initialize();
	static void deinitialize();

	static void resolve_imports(c_compiler_context &context, h_compiler_source_file source_file_handle);
	static void add_imports_to_global_scope(c_compiler_context &context, h_compiler_source_file source_file_handle);
};
