#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"

#include <vector>

enum e_token_type {
	// Invalid token to indicate errors
	k_token_type_invalid,

	// Keywords
	k_token_type_first_keyword,

	k_token_type_keyword_const = k_token_type_first_keyword,
	k_token_type_keyword_in,
	k_token_type_keyword_out,
	k_token_type_keyword_module,
	k_token_type_keyword_void,
	k_token_type_keyword_real,
	k_token_type_keyword_bool,
	k_token_type_keyword_string,
	k_token_type_keyword_return,
	k_token_type_keyword_repeat,

	k_token_type_last_keyword = k_token_type_keyword_repeat,

	// Identifiers start with [a-zA-Z_] followed by 0 or more [a-zA-Z0-9_]
	k_token_type_identifier,

	// Constants
	k_token_type_constant_real,
	k_token_type_constant_bool,
	k_token_type_constant_string,

	// Paired symbols
	k_token_type_first_single_character_operator,

	// Parentheses
	k_token_type_left_parenthesis = k_token_type_first_single_character_operator,
	k_token_type_right_parenthesis,

	// Braces
	k_token_type_left_brace,
	k_token_type_right_brace,

	// Brackets
	k_token_type_left_bracket,
	k_token_type_right_bracket,

	// Comma
	k_token_type_comma,
	k_token_type_semicolon,

	k_token_type_last_single_character_operator = k_token_type_semicolon,

	// Operators

	k_token_type_first_operator,

	// Operators
	k_token_type_operator_assignment = k_token_type_first_operator,
	k_token_type_operator_addition,
	k_token_type_operator_subtraction,
	k_token_type_operator_multiplication,
	k_token_type_operator_division,
	k_token_type_operator_modulo,
	k_token_type_operator_not,
	k_token_type_operator_equal,
	k_token_type_operator_not_equal,
	k_token_type_operator_greater,
	k_token_type_operator_less,
	k_token_type_operator_greater_equal,
	k_token_type_operator_less_equal,
	k_token_type_operator_and,
	k_token_type_operator_or,

	k_token_type_last_operator = k_token_type_operator_or,

	// Comment tokens - should never be a result of the lexer
	k_token_type_comment,

	k_token_type_count
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

