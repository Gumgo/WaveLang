#pragma once

#include "common/common.h"

#include "compiler/compiler_utility.h"
#include "compiler/lr_parser.h"

#include <vector>

// Defines the specific parser rules for this language

enum class e_parser_nonterminal : uint16 {
	k_start,

	k_global_scope,
	k_global_scope_item_list,
	k_global_scope_item,

	k_basic_type,
	k_array_type,
	k_type,
	k_type_or_void,

	k_module_declaration,
	k_module_declaration_arguments,
	k_module_declaration_argument_list,
	k_module_declaration_argument,
	k_module_declaration_argument_qualifier,

	k_scope,
	k_scope_item_list,
	k_scope_item,

	k_module_return_statement,
	k_named_value_declaration,
	k_named_value_assignment,
	k_repeat_loop,
	k_expression,
	k_expression_assignment,
	k_expr_1,
	k_expr_2,
	k_expr_3,
	k_expr_4,
	k_expr_5,
	k_expr_6,
	k_expr_7,
	k_expr_8,
	k_expr_9,
	k_module_call,
	k_module_call_argument_list,

	k_constant_array, // Note: the array values themselves aren't constant, but the array itself is
	k_constant_array_list,

	k_array_dereference,

	k_count
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

	static s_compiler_result process(
		const s_compiler_context &context,
		const s_lexer_output &input,
		s_parser_output &output,
		std::vector<s_compiler_result> &out_errors);
};

