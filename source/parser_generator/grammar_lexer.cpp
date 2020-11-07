#include "parser_generator/grammar_lexer.h"

static bool do_next_characters_match(const char *buffer, const char *str);
static bool is_whitespace(char c);
static bool is_identifier_start_character(char c);
static bool is_identifier_character(char c);

c_grammar_lexer::c_grammar_lexer(const char *source)
	: m_source(source)
	, m_offset(0)
	, m_line(1)
	, m_character(1) {}

s_grammar_token c_grammar_lexer::get_next_token() {
	s_grammar_token result;
	zero_type(&result);

	// Skip whitespace and comments
	bool try_skipping = true;
	while (try_skipping) {
		// If we detect whitespace or comments, we should try skipping again
		try_skipping = false;

		while (is_whitespace(m_source[m_offset])) {
			try_skipping = true;
			increment(1);
		}

		if (do_next_characters_match(m_source + m_offset, "//")) {
			try_skipping = true;
			increment(2);
			while (m_source[m_offset] != '\0' && m_source[m_offset] != '\n') {
				increment(1);
			}
		}
	}

	result.line = m_line;
	result.character = m_character;

	if (m_source[m_offset] == '\0') {
		result.token_type = e_grammar_token_type::k_eof;
		result.offset = m_offset;
		result.length = 0;
		return result;
	} else if (is_identifier_start_character(m_source[m_offset])) {
		result.token_type = e_grammar_token_type::k_identifier;
		result.offset = m_offset;
		while (is_identifier_character(m_source[m_offset])) {
			increment(1);
		}

		result.length = m_offset - result.offset;
		return result;
	} else if (m_source[m_offset] == '"') {
		// Skip the start quote
		increment(1);

		result.token_type = e_grammar_token_type::k_string;
		result.offset = m_offset;
		while (m_source[m_offset] != '"') {
			if (m_source[m_offset] == '\0' || m_source[m_offset] == '\r' || m_source[m_offset] == '\n') {
				// No end quote found
				result.token_type = e_grammar_token_type::k_invalid;
				return result;
			}
			increment(1);
		}

		result.length = m_offset - result.offset;
		m_offset++; // Skip the end quote
		return result;
	} else {
		// Longer ones come first because they take precedence over shorter tokens (e.g. == would match over =)
		struct s_token_match_string {
			e_grammar_token_type token_type;
			const char *token_string;
		};

		static const s_token_match_string k_token_match_strings[] = {
			{ e_grammar_token_type::k_arrow, "->" },
			{ e_grammar_token_type::k_period, "." },
			{ e_grammar_token_type::k_colon, ":" },
			{ e_grammar_token_type::k_semicolon, ";" },
			{ e_grammar_token_type::k_left_brace, "{" },
			{ e_grammar_token_type::k_right_brace, "}" },
			{ e_grammar_token_type::k_left_parenthesis, "(" },
			{ e_grammar_token_type::k_right_parenthesis, ")" }
		};

		for (const s_token_match_string &token_match_string : k_token_match_strings) {
			if (do_next_characters_match(m_source + m_offset, token_match_string.token_string)) {
				result.token_type = token_match_string.token_type;
				result.offset = m_offset;
				result.length = strlen(token_match_string.token_string);
				increment(result.length);
				return result;
			}
		}

		// We found no match
		result.token_type = e_grammar_token_type::k_invalid;
		result.offset = m_offset;
		return result;
	}
}

void c_grammar_lexer::increment(size_t amount) {
	for (size_t i = 0; i < amount; i++) {
		if (m_source[m_offset] == '\n') {
			m_line++;
			m_character = 1;
		} else {
			m_character++;
		}

		m_offset++;
	}
}

static bool do_next_characters_match(const char *buffer, const char *str) {
	while (true) {
		char c = *str;
		if (c == '\0') {
			return true;
		} else if (c != *buffer) {
			return false;
		}

		buffer++;
		str++;
	}
}

static bool is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_identifier_start_character(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_');
}

static bool is_identifier_character(char c) {
	return is_identifier_start_character(c) || (c >= '0' && c <= '9');
}
