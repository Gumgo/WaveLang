#pragma once

#include "common/common.h"

#include "compiler/source_file.h"

// Used to pass around source locations in the AST
template<typename t_value>
struct s_value_with_source_location {
	t_value value;
	s_compiler_source_location source_location;
};

struct s_upsample_factor {
	bool was_provided;
	uint32 upsample_factor;
};

struct s_data_type_with_details {
	c_ast_data_type data_type;
	bool was_upsample_factor_provided;
};
