#ifndef WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__
#define WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__

#include "common/common.h"

struct s_execution_graph_globals;

// Context for whet
struct s_execution_graph_globals_context {
	bool max_voices_command_executed;
	bool sample_rate_command_executed;
	bool chunk_size_command_executed;

	s_execution_graph_globals *globals;

	void clear();
};

class c_execution_graph_globals_parser {
public:
	static void register_preprocessor_commands(s_execution_graph_globals_context *globals_context);
};

#endif // WAVELANG_COMPILER_EXECUTION_GRAPH_GLOBALS_PARSER_H__
