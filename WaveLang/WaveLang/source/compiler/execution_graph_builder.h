#ifndef WAVELANG_EXECUTION_GRAPH_BUILDER_H__
#define WAVELANG_EXECUTION_GRAPH_BUILDER_H__

#include "common\common.h"

class c_ast_node;
class c_execution_graph;

class c_execution_graph_builder {
public:
	static void build_execution_graph(const c_ast_node *ast, c_execution_graph *out_execution_graph);
};

#endif // WAVELANG_EXECUTION_GRAPH_BUILDER_H__