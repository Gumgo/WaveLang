#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

enum class e_token_type : uint16;

struct s_token {
	// The type of token
	e_token_type token_type;

	// Points into the source code
	c_compiler_string token_string;

	// Source code information
	s_compiler_source_location source_location;
};
