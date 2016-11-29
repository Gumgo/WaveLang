#ifndef WAVELANG_EXECUTION_GRAPH_INSTRUMENT_STAGE_H__
#define WAVELANG_EXECUTION_GRAPH_INSTRUMENT_STAGE_H__

#include "common/common.h"

enum e_instrument_stage {
	k_instrument_stage_invalid = -1,

	k_instrument_stage_voice,
	k_instrument_stage_fx,

	k_instrument_stage_count
};

#endif // WAVELANG_EXECUTION_GRAPH_INSTRUMENT_STAGE_H__
