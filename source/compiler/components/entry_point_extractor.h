#pragma once

#include "common/common.h"

#include "compiler/ast/nodes.h"
#include "compiler/compiler_context.h"

class c_entry_point_extractor {
public:
	static void extract_entry_points(
		c_compiler_context &context,
		c_AST_node_module_declaration *&voice_entry_point_out,
		c_AST_node_module_declaration *&fx_entry_point_out);
};
