#ifndef WAVELANG_EXECUTION_GRAPH_BUILDER_H__
#define WAVELANG_EXECUTION_GRAPH_BUILDER_H__

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include <vector>

class c_ast_node;
class c_execution_graph;

class c_execution_graph_builder {
public:
	static s_compiler_result build_execution_graph(const c_ast_node *ast, c_execution_graph *out_execution_graph,
		std::vector<s_compiler_result> &out_errors);
};

#endif // WAVELANG_EXECUTION_GRAPH_BUILDER_H__