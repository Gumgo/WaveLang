#ifndef WAVELANG_EXECUTION_GRAPH_INSTRUMENT_CONSTANTS_H__
#define WAVELANG_EXECUTION_GRAPH_INSTRUMENT_CONSTANTS_H__

#include "common/common.h"

// Bump this number when anything changes
static const uint32 k_instrument_format_version = 0;

enum e_instrument_result {
	k_instrument_result_success,
	k_instrument_result_failed_to_write,
	k_instrument_result_failed_to_read,
	k_instrument_result_invalid_header,
	k_instrument_result_version_mismatch,
	k_instrument_result_invalid_globals,
	k_instrument_result_invalid_graph,
	k_instrument_result_unregistered_native_module,

	k_instrument_result_count
};

#endif // WAVELANG_EXECUTION_GRAPH_INSTRUMENT_CONSTANTS_H__
