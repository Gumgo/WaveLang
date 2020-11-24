#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

#include "execution_graph/instrument_globals.h"

#include <vector>

// Container holding values which will eventually resolve into one or more sets of instrument globals. For example,
// specifying N different sample rates will cause the instrument to be built N times using a different sample rate
// constant in each instance. At runtime, the best instrument variant is then selected based on the stream parameters.
struct s_instrument_globals_context {
	bool max_voices_command_executed;
	uint32 max_voices;

	bool sample_rate_command_executed;
	std::vector<uint32> sample_rates;

	bool chunk_size_command_executed;
	uint32 chunk_size;

	bool activate_fx_immediately_command_executed;
	bool activate_fx_immediately;

	void clear();

	// Fills in defaults for values which weren't specified
	void assign_defaults();

	// Builds all combinations of instrument globals
	std::vector<s_instrument_globals> build_instrument_globals_set() const;
};

class c_instrument_globals_parser {
public:
	static void initialize();
	static void deinitialize();

	static void parse_instrument_globals(
		c_compiler_context &context,
		h_compiler_source_file source_file_handle,
		bool is_top_level_source_file,
		s_instrument_globals_context &instrument_globals);
};

