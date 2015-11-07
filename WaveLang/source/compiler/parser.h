#ifndef WAVELANG_PARSER_H__
#define WAVELANG_PARSER_H__

#include "common/common.h"
#include "compiler/compiler_utility.h"
#include "compiler/lr_parser.h"
#include <vector>

// Defines the specific parser rules for this language

enum e_parser_nonterminal {
	k_parser_nonterminal_start,

	k_parser_nonterminal_global_scope,
	k_parser_nonterminal_global_scope_item_list,
	k_parser_nonterminal_global_scope_item,

	k_parser_nonterminal_basic_type,
	k_parser_nonterminal_array_type,
	k_parser_nonterminal_type,
	k_parser_nonterminal_type_or_void,

	k_parser_nonterminal_module_declaration,
	k_parser_nonterminal_module_declaration_arguments,
	k_parser_nonterminal_module_declaration_argument_list,
	k_parser_nonterminal_module_declaration_argument,
	k_parser_nonterminal_module_declaration_argument_qualifier,

	k_parser_nonterminal_scope,
	k_parser_nonterminal_scope_item_list,
	k_parser_nonterminal_scope_item,

	k_parser_nonterminal_module_return_statement,
	k_parser_nonterminal_named_value_declaration,
	k_parser_nonterminal_named_value_assignment,
	k_parser_nonterminal_repeat_loop,
	k_parser_nonterminal_expression,
	k_parser_nonterminal_expression_assignment,
	k_parser_nonterminal_expr_1,
	k_parser_nonterminal_expr_2,
	k_parser_nonterminal_expr_3,
	k_parser_nonterminal_expr_4,
	k_parser_nonterminal_expr_5,
	k_parser_nonterminal_expr_6,
	k_parser_nonterminal_expr_7,
	k_parser_nonterminal_expr_8,
	k_parser_nonterminal_expr_9,
	k_parser_nonterminal_module_call,
	k_parser_nonterminal_module_call_argument_list,

	k_parser_nonterminal_constant_array, // Note: the array values themselves aren't constant, but the array itself is
	k_parser_nonterminal_constant_array_list,

	k_parser_nonterminal_array_dereference,

	k_parser_nonterminal_count
};

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

	static s_compiler_result process(const s_compiler_context &context, const s_lexer_output &input,
		s_parser_output &output, std::vector<s_compiler_result> &out_errors);
};

#endif // WAVELANG_PARSER_H__