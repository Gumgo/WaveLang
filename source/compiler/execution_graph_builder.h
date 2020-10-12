#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include <vector>

class c_ast_node;
class c_instrument_variant;

class c_execution_graph_builder {
public:
	static s_compiler_result build_execution_graphs(
		c_wrapped_array<void *> native_module_library_contexts,
		const c_ast_node *ast,
		c_instrument_variant *instrument_variant_out,
		std::vector<s_compiler_result> &errors_out);
};

