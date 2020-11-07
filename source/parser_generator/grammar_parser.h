#pragma once

#include "common/common.h"

#include "parser_generator/grammar.h"
#include "parser_generator/grammar_lexer.h"

class c_grammar_parser {
public:
	c_grammar_parser(const char *source, c_wrapped_array<const s_grammar_token> tokens, s_grammar *grammar_out);

	bool parse();

	const char *get_error_message() const {
		return m_error_message.c_str();
	}

	const s_grammar_token &get_error_token() const {
		return m_tokens[m_token_index];
	}

private:
	const s_grammar_token &next_token() const {
		return m_tokens[m_token_index];
	}

	std::string next_token_string() const {
		return std::string(m_source + next_token().offset, next_token().length);
	}

	bool try_read_simple_command(
		const char *command_name,
		e_grammar_token_type value_type,
		std::string &value,
		bool &processed_command_out);
	bool try_read_terminals_command(bool &processed_command_out);
	bool try_read_nonterminals_command(bool &processed_command_out);

	bool read_terminal();
	bool read_nonterminal();
	bool read_rule(const std::string &nonterminal);

	bool finalize_and_validate();

	const char *m_source;
	c_wrapped_array<const s_grammar_token> m_tokens;
	size_t m_token_index;

	s_grammar *m_grammar;

	bool m_terminals_read;
	bool m_nonterminals_read;

	std::string m_error_message;
};
