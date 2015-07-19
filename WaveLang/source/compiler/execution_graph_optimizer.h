#ifndef WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__
#define WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__

#include "common/common.h"
#include "execution_graph/execution_graph.h"
#include "compiler/compiler_utility.h"
#include <vector>

class c_execution_graph_optimizer {
public:
	static s_compiler_result optimize_graph(c_execution_graph *execution_graph,
		std::vector<s_compiler_result> &out_errors);
};

#endif // WAVELANG_EXECUTION_GRAPH_OPTIMIZER_H__