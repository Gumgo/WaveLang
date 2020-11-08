#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"
#include "compiler/token.h"

#include "generated/wavelang_grammar.h"

#include <vector>

extern const char *k_token_type_constant_bool_false_string;
extern const char *k_token_type_constant_bool_true_string;

// $TODO these should be in the .cpp file - we only need them here because of the preprocessor, which will go away soon
static constexpr e_token_type k_first_keyword = e_token_type::k_keyword_const;
static constexpr e_token_type k_last_keyword = e_token_type::k_keyword_repeat;

static constexpr e_token_type k_first_single_character_operator = e_token_type::k_left_parenthesis;
static constexpr e_token_type k_last_single_character_operator = e_token_type::k_semicolon;

static constexpr e_token_type k_first_operator = e_token_type::k_operator_assignment;
static constexpr e_token_type k_last_operator = e_token_type::k_operator_or;

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
		std::vector<s_compiler_result> &errors_out);

	// Token parsing is used in the preprocessor as well
	static s_token read_next_token(c_compiler_string str);
};

