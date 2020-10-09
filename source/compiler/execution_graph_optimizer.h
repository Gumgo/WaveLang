#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include "execution_graph/instrument_globals.h"
#include "execution_graph/native_module.h"

#include <map>
#include <set>
#include <stack>
#include <vector>

class c_execution_graph;

// Used to evaluate constants in a partially-built graph. Necessary for building repeat loops and indexing arrays.
class c_execution_graph_constant_evaluator {
public:
	struct s_result {
		c_native_module_data_type type;

		union {
			real32 real_value;
			bool bool_value;
		};
		c_native_module_string string_value;
		c_native_module_array array_value;
	};

	c_execution_graph_constant_evaluator();
	void initialize(
		c_execution_graph *execution_graph,
		const s_instrument_globals *instrument_globals,
		std::vector<s_compiler_result> *errors);

	// Each time a constant is evaluated, the evaluation progress is retained. This way, we can avoid re-evaluating the
	// same thing multiple times. This works as long as nodes are only added and never removed or changed during
	// construction. (We perform these actions at optimization time.)
	bool evaluate_constant(c_node_reference node_reference);
	s_result get_result() const;

	void remove_cached_result(c_node_reference node_reference);

private:
	void try_evaluate_node(c_node_reference node_reference);
	bool build_module_call_arguments(
		const s_native_module &native_module,
		c_node_reference native_module_node_reference,
		std::vector<s_native_module_compile_time_argument> &arg_list_out);
	void store_native_module_call_results(
		const s_native_module &native_module,
		c_node_reference native_module_node_reference,
		const std::vector<s_native_module_compile_time_argument> &arg_list);

	// The graph
	c_execution_graph *m_execution_graph;

	// Globals
	const s_instrument_globals *m_instrument_globals;

	std::vector<s_compiler_result> *m_errors;

	// Maps node indices to their evaluated results. Either native module output nodes or constant nodes.
	std::map<c_node_reference, s_result> m_results;

	// Stack of nodes to evaluate. Either native module output nodes or constant nodes.
	std::stack<c_node_reference> m_pending_nodes;

	// The final result
	bool m_invalid_constant;
	s_result m_result;
};

// Used to trim unused nodes from a partially-built graph to avoid having the graph grow to be unnecessarily big.
class c_execution_graph_trimmer {
public:
	using f_on_node_removed = void (*)(void *context, c_node_reference node_reference);

	c_execution_graph_trimmer();
	void initialize(
		c_execution_graph *execution_graph,
		f_on_node_removed on_node_removed,
		void *on_node_removed_context);

	void try_trim_node(c_node_reference node_reference);

private:
	void add_pending_node(c_node_reference node_reference);

	// The graph
	c_execution_graph *m_execution_graph;

	f_on_node_removed m_on_node_removed;
	void *m_on_node_removed_context;

	// List of nodes to check, cached here to avoid unnecessary allocations
	std::stack<c_node_reference> m_pending_nodes;
	std::set<c_node_reference> m_visited_nodes;
};

class c_execution_graph_optimizer {
public:
	static s_compiler_result optimize_graph(
		c_execution_graph *execution_graph,
		const s_instrument_globals *instrument_globals,
		std::vector<s_compiler_result> &errors_out);
};

