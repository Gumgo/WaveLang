#pragma once

#include "common/common.h"

#include "compiler/compiler_context.h"

#include "execution_graph/instrument.h"

class c_instrument_variant_optimizer {
public:
	static bool optimize_instrument_variant(c_compiler_context &context, c_instrument_variant &instrument_variant);
};
