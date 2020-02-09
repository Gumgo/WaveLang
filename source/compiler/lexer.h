#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include <vector>

enum class e_token_type : uint16 {
	// Invalid token to indicate errors
	k_invalid,

	// Keywords
	k_first_keyword,

	k_keyword_const = k_first_keyword,
	k_keyword_in,
	k_keyword_out,
	k_keyword_module,
	k_keyword_void,
	k_keyword_real,
	k_keyword_bool,
	k_keyword_string,
	k_keyword_return,
	k_keyword_repeat,

	k_last_keyword = k_keyword_repeat,

	// Identifiers start with [a-zA-Z_] followed by 0 or more [a-zA-Z0-9_]
	k_identifier,

	// Constants
	k_constant_real,
	k_constant_bool,
	k_constant_string,

	// Paired symbols
	k_first_single_character_operator,

	// Parentheses
	k_left_parenthesis = k_first_single_character_operator,
	k_right_parenthesis,

	// Braces
	k_left_brace,
	k_right_brace,

	// Brackets
	k_left_bracket,
	k_right_bracket,

	// Comma
	k_comma,
	k_semicolon,

	k_last_single_character_operator = k_semicolon,

	// Operators

	k_first_operator,

	// Operators
	k_operator_assignment = k_first_operator,
	k_operator_addition,
	k_operator_subtraction,
	k_operator_multiplication,
	k_operator_division,
	k_operator_modulo,
	k_operator_not,
	k_operator_equal,
	k_operator_not_equal,
	k_operator_greater,
	k_operator_less,
	k_operator_greater_equal,
	k_operator_less_equal,
	k_operator_and,
	k_operator_or,

	k_last_operator = k_operator_or,

	// Comment tokens - should never be a result of the lexer
	k_comment,

	k_count
};

extern const char *k_token_type_constant_bool_false_string;
extern const char *k_token_type_constant_bool_true_string;

struct s_token {
	// The type of token
	e_token_type token_type;

	// Points into the source code
	c_compiler_string token_string;

	// Source code information
	s_compiler_source_location source_location;
};

struct s_lexer_source_file_output {
	// List of tokens
	std::vector<s_token> tokens;
};

struct s_lexer_output {
	// List of tokens for each source file
	std::vector<s_lexer_source_file_output> source_file_output;
};

class c_lexer {
public:
	static void initialize_lexer();
	static void shutdown_lexer();

	static s_compiler_result process(
		const s_compiler_context &context,
		s_lexer_output &output,
		std::vector<s_compiler_result> &out_errors);

	// Token parsing is used in the preprocessor as well
	static s_token read_next_token(c_compiler_string str);
};

