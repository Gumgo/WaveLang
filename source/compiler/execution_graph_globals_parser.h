#ifndef WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__
#define WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__

#include "common/common.h"

#include "execution_graph/execution_graph.h"

#include <vector>

// Container holding values which will eventually resolve into one or more sets of execution graph globals. For example,
// specifying N different sample rates will cause the execution graph to be built N times using a different sample rate
// constant in each instance. At runtime, the best matching graph is then selected based on the stream parameters.
struct s_execution_graph_globals_context {
	bool max_voices_command_executed;
	uint32 max_voices;

	bool sample_rate_command_executed;
	std::vector<uint32> sample_rates;

	bool chunk_size_command_executed;
	uint32 chunk_size;

	void clear();

	// Fills in defaults for values which weren't specified
	void assign_defaults();

	// Builds all combinations of execution graph globals
	std::vector<s_execution_graph_globals> build_execution_graph_globals_set() const;
};

class c_execution_graph_globals_parser {
public:
	static void register_preprocessor_commands(s_execution_graph_globals_context *globals_context);
};

#endif // WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__
