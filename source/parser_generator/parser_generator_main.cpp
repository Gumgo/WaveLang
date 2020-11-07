#include "common/common.h"
#include "common/math/floating_point.h"

#include "parser_generator/grammar_lexer.h"
#include "parser_generator/grammar_parser.h"
#include "parser_generator/lr_parser_generator.h"

#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
	initialize_floating_point_behavior();

	if (argc != 4) {
		std::cerr << "usage: " << argv[0] << " <grammar_input_file> <parser_output_file_h> <parser_output_file_inl>\n";
		return 1;
	}

	const char *grammar_filename = argv[1];
	const char *parser_output_filename_h = argv[2];
	const char *parser_output_filename_inl = argv[3];

	std::vector<char> source;
	{
		std::ifstream grammar_file(grammar_filename, std::ios::binary | std::ios::ate);

		if (!grammar_file.is_open()) {
			std::cerr << "Failed to open grammar file '" << grammar_filename << "'\n";
			return -1;
		}

		std::streampos full_file_size = grammar_file.tellg();
		grammar_file.seekg(0);

		// cast_integer_verify doesn't work with std::streampos
		if (full_file_size > static_cast<std::streampos>(std::numeric_limits<int32>::max())) {
			std::cerr << "Grammar file '" << grammar_filename << "' is too big\n";
			return -1;
		}

		size_t file_size = static_cast<size_t>(full_file_size);
		wl_assert(static_cast<std::streampos>(file_size) == full_file_size);

		source.resize(file_size + 1);
		grammar_file.read(&source.front(), file_size);
		source.back() = '\0';

		if (grammar_file.fail()) {
			std::cerr << "Failed to read source file '" << grammar_filename << "'\n";
			return -1;
		}
	}

	std::vector<s_grammar_token> tokens;
	c_grammar_lexer lexer(&source.front());
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
	c_grammar_parser parser(
		&source.front(),
		c_wrapped_array<const s_grammar_token>(&tokens.front(), tokens.size()),
		&grammar);
	if (!parser.parse()) {
		const s_grammar_token &error_token = parser.get_error_token();
		std::cerr << "Parser error: " << parser.get_error_message()
			<< " (line " << error_token.line << ", character " << error_token.character << ")\n";
		return -1;
	}

	c_lr_parser_generator lr_parser_generator;
	e_lr_conflict conflict = lr_parser_generator.generate_lr_parser(
		grammar,
		parser_output_filename_h,
		parser_output_filename_inl);
	if (conflict == e_lr_conflict::k_shift_reduce) {
		std::cerr << "Shift-reduce conflict encountered";
		return -1;
	} else if (conflict == e_lr_conflict::k_reduce_reduce) {
		std::cerr << "Reduce-reduce conflict encountered";
		return -1;
	}

	std::cout << "Wrote '" << parser_output_filename_h << "' and '" << parser_output_filename_inl << "'";
	return 0;
}
