#pragma once

#include "common/common.h"

// Bump this number when anything changes
static constexpr uint32 k_instrument_format_version = 0;

enum class e_instrument_result {
	k_success,
	k_failed_to_write,
	k_failed_to_read,
	k_invalid_header,
	k_version_mismatch,
	k_invalid_globals,
	k_invalid_graph,
	k_unregistered_native_module,
	k_validation_failure,

	k_count
};

