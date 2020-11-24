#pragma once

#include "common/common.h"

#include "compiler/source_file.h"

#include <string_view>

enum class e_token_type : uint16;

struct s_token {
	// The type of token
	e_token_type token_type;

	// Points into the source code
	std::string_view token_string;

	// Source code information
	s_compiler_source_location source_location;

	// Value, if the token type supports it
	union {
		real32 real_value;
		bool bool_value;
	} value;
};

// This function assumes (and asserts) that the token string contains no invalid escape sequences
std::string get_unescaped_token_string(const s_token &token);

std::string escape_token_string_for_output(const std::string_view &token_string);
