#ifndef WAVELANG_COMPILER_EXECUTION_GRAPH_OPTIMIZER_H__
#define WAVELANG_COMPILER_EXECUTION_GRAPH_OPTIMIZER_H__

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include "execution_graph/execution_graph.h"

#include <map>
#include <stack>
#include <vector>

// Used to evaluate constants in a partially-built graph. Necessary for building repeat loops.
class c_execution_graph_constant_evaluator {
public:
	struct s_result {
		c_native_module_data_type type;

		union {
			real32 real_value;
			bool bool_value;
		};
		std::string string_value;
		std::vector<uint32> array_value;
	};

	c_execution_graph_constant_evaluator();
	void initialize(const c_execution_graph *execution_graph);

	// Each time a constant is evaluated, the evaluation progress is retained. This way, we can avoid re-evaluating the
	// same thing multiple times. This works as long as nodes are only added and never removed or changed during
	// construction. (We perform these actions at optimization time.)
	bool evaluate_constant(uint32 node_index);
	s_result get_result() const;

private:
	// The graph
	const c_execution_graph *m_execution_graph;

	// Maps node indices to their evaluated results. Either native module output nodes or constant nodes.
	std::map<uint32, s_result> m_results;

	// Stack of nodes to evaluate. Either native module output nodes or constant nodes.
	std::stack<uint32> m_pending_nodes;

	// The final result
	bool m_invalid_constant;
	s_result m_result;

	void try_evaluate_node(uint32 node_index);
	bool build_module_call_arguments(const s_native_module &native_module, uint32 native_module_node_index,
		std::vector<s_native_module_compile_time_argument> &out_arg_list);
	void store_native_module_call_results(const s_native_module &native_module, uint32 native_module_node_index,
		const std::vector<s_native_module_compile_time_argument> &arg_list);
};

class c_execution_graph_optimizer {
public:
	static s_compiler_result optimize_graph(c_execution_graph *execution_graph,
		std::vector<s_compiler_result> &out_errors);
};

#endif // WAVELANG_COMPILER_EXECUTION_GRAPH_OPTIMIZER_H__
