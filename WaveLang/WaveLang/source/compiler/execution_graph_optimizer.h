#ifndef WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__
#define WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__

#include "common\common.h"
#include "execution_graph\execution_graph.h"

class c_execution_graph_optimizer {
public:
	static void optimize_graph(c_execution_graph *execution_graph);
};

#endif // WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__