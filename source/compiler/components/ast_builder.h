#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

class c_AST_builder {
public:
	static void build_ast_declarations(c_compiler_context &context, h_compiler_source_file source_file_handle);
	static void build_ast_definitions(c_compiler_context &context, h_compiler_source_file source_file_handle);
};
