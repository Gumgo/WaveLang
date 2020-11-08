#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"
#include "compiler/lr_parser.h"

#include "generated/wavelang_grammar.h"

#include <vector>

// Defines the specific parser rules for this language

struct s_lexer_output;

struct s_parser_source_file_output {
	c_lr_parse_tree parse_tree;
};

struct s_parser_output {
	// List of parse trees for each source file
	std::vector<s_parser_source_file_output> source_file_output;
};

class c_parser {
public:
	static void initialize_parser();
	static void shutdown_parser();

	static s_compiler_result process(
		const s_compiler_context &context,
		const s_lexer_output &input,
		s_parser_output &output,
		std::vector<s_compiler_result> &errors_out);
};

