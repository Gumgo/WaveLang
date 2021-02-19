#include "common/common.h"
#include "common/math/floating_point.h"
#include "common/utility/file_utility.h"
#include "common/utility/memory_leak_detection.h"

#include "parser_generator/grammar_lexer.h"
#include "parser_generator/grammar_parser.h"
#include "parser_generator/lr_parser_generator.h"

#include <iostream>

int main(int argc, char **argv) {
	initialize_memory_leak_detection();
	initialize_floating_point_behavior();

	if (argc != 4) {
		std::cerr << "usage: " << argv[0] << " <grammar_input_file> <parser_output_file_h> <parser_output_file_inl>\n";
		return 1;
	}

	const char *grammar_filename = argv[1];
	const char *parser_output_filename_h = argv[2];
	const char *parser_output_filename_inl = argv[3];

	std::vector<char> source;
	e_read_full_file_result read_result = read_full_file(grammar_filename, source);
	switch (read_result) {
	case e_read_full_file_result::k_success:
		break;

	case e_read_full_file_result::k_failed_to_open:
		std::cerr << "Failed to open grammar file '" << grammar_filename << "'\n";
		return -1;

	case e_read_full_file_result::k_failed_to_read:
		std::cerr << "Failed to read grammar file '" << grammar_filename << "'\n";
		return -1;

	case e_read_full_file_result::k_file_too_big:
		std::cerr << "Grammar file '" << grammar_filename << "' is too big\n";
		return -1;

	default:
		wl_unreachable();
	}

	// Add null terminator so that we can detect EOF easily
	source.push_back('\0');

	std::vector<s_grammar_token> tokens;
	c_grammar_lexer lexer(source.data());
	while (true) {
		s_grammar_token next_token = lexer.get_next_token();
		if (next_token.token_type == e_grammar_token_type::k_invalid) {
			std::cerr << "Lexer error (line " << next_token.line << ", character " << next_token.character << ")\n";
			return -1;
		}

		tokens.push_back(next_token);
		if (next_token.token_type == e_grammar_token_type::k_eof) {
			break;
		}
	}

	s_grammar grammar;
	c_grammar_parser parser(source.data(), c_wrapped_array<const s_grammar_token>(tokens), &grammar);
	if (!parser.parse()) {
		const s_grammar_token &error_token = parser.get_error_token();
		std::cerr << "Parser error: " << parser.get_error_message()
			<< " (line " << error_token.line << ", character " << error_token.character << ")\n";
		return -1;
	}

	c_lr_parser_generator lr_parser_generator;
	s_lr_conflict conflict = lr_parser_generator.generate_lr_parser(
		grammar,
		parser_output_filename_h,
		parser_output_filename_inl);
	if (conflict.conflict == e_lr_conflict::k_shift_reduce) {
		const std::string function = grammar.rules[conflict.production_index].function;
		std::cerr << "Shift-reduce conflict encountered between terminal '"
			<< grammar.terminals[conflict.terminal_index].value
			<< "' and production for nonterminal '"
			<< grammar.rules[conflict.production_index].nonterminal << "'"
			<< (function.empty() ? "" : " (" + function + ")")
			<< "\n";
		return -1;
	} else if (conflict.conflict == e_lr_conflict::k_reduce_reduce) {
		const std::string function_a = grammar.rules[conflict.production_index_a].function;
		const std::string function_b = grammar.rules[conflict.production_index_b].function;
		std::cerr << "Reduce-reduce conflict encountered between productions for nonterminals '"
			<< grammar.rules[conflict.production_index_a].nonterminal << "'"
			<< (function_a.empty() ? "" : " (" + function_a + ")")
			<< " and '"
			<< grammar.rules[conflict.production_index_b].nonterminal << "'"
			<< (function_b.empty() ? "" : " (" + function_b + ")")
			<< "\n";
		return -1;
	}

	std::cout << "Wrote '" << parser_output_filename_h << "' and '" << parser_output_filename_inl << "'\n";
	return 0;
}
