#pragma once

#include "common/common.h"

enum class e_grammar_token_type {
	k_invalid,
	k_eof,
	k_identifier,
	k_string,
	k_period,
	k_colon,
	k_semicolon,
	k_left_brace,
	k_right_brace,
	k_left_parenthesis,
	k_right_parenthesis,
	k_arrow,

	k_count
};

struct s_grammar_token {
	e_grammar_token_type token_type;
	size_t offset;
	size_t length;
	size_t line;
	size_t character;
};

class c_grammar_lexer {
public:
	c_grammar_lexer(const char *source);

	s_grammar_token get_next_token();

private:
	void increment(size_t amount);

	const char *m_source;
	size_t m_offset;
	size_t m_line;
	size_t m_character;
};
