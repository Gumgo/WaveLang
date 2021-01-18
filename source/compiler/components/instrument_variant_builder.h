#pragma once

#include "common/common.h"

#include "compiler/ast/nodes.h"
#include "compiler/compiler_context.h"

#include "instrument/instrument.h"
#include "instrument/instrument_globals.h"

class c_instrument_variant_builder {
public:
	static c_instrument_variant *build_instrument_variant(
		c_compiler_context &context,
		const s_instrument_globals &instrument_globals,
		c_ast_node_module_declaration *voice_entry_point,
		c_ast_node_module_declaration *fx_entry_point);
};
