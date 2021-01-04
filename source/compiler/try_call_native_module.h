#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"
#include "compiler/graph_trimmer.h"

#include <vector>

bool try_call_native_module(
	c_compiler_context &context,
	c_graph_trimmer &graph_trimmer,
	const s_instrument_globals &instrument_globals,
	c_node_reference native_module_call_node_reference,
	const s_compiler_source_location &call_source_location,
	bool &did_call_out,
	std::vector<c_node_reference> *output_node_references_out = nullptr);
