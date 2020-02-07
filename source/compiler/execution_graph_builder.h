#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include <vector>

class c_ast_node;
class c_instrument_variant;

class c_execution_graph_builder {
public:
	static s_compiler_result build_execution_graphs(
		const c_ast_node *ast,
		c_instrument_variant *out_instrument_variant,
		std::vector<s_compiler_result> &out_errors);
};

