#include "compiler/parser.h"
#include "compiler/lexer.h"

#define OUTPUT_PARSE_TREE_ENABLED 1

#if PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)
#include <fstream>
#endif // PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)

static s_lr_production make_production(
	c_lr_symbol lhs,
	c_lr_symbol rhs_0 = c_lr_symbol(),
	c_lr_symbol rhs_1 = c_lr_symbol(),
	c_lr_symbol rhs_2 = c_lr_symbol(),
	c_lr_symbol rhs_3 = c_lr_symbol(),
	c_lr_symbol rhs_4 = c_lr_symbol(),
	c_lr_symbol rhs_5 = c_lr_symbol(),
	c_lr_symbol rhs_6 = c_lr_symbol(),
	c_lr_symbol rhs_7 = c_lr_symbol(),
	c_lr_symbol rhs_8 = c_lr_symbol(),
	c_lr_symbol rhs_9 = c_lr_symbol()) {
	s_lr_production production = {
		lhs,
		{
			rhs_0, rhs_1, rhs_2, rhs_3, rhs_4, rhs_5, rhs_6, rhs_7, rhs_8, rhs_9
		}
	};
	return production;
}

// Shorthand
#define PROD(x, ...) make_production(x, __VA_ARGS__), // Hack to allow the macro to accept commas
#define TS(x) c_lr_symbol(true, static_cast<uint16>(k_token_type_ ## x))
#define NS(x) c_lr_symbol(false, static_cast<uint16>(k_parser_nonterminal_ ## x))

// Production table
static const s_lr_production k_production_table[] = {
	// txt file, because it's annoying to try to edit this as an inl file
	#include "parser_rules.txt"
};

#if !PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
#include "lr_parse_tables.inl"
#endif PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)

static bool g_parser_initialized = false;
static c_lr_parser g_lr_parser;

static bool process_source_file(
	const s_compiler_source_file &source_file,
	const s_lexer_source_file_output &source_file_input,
	int32 source_file_index,
	s_parser_source_file_output &source_file_output,
	std::vector<s_compiler_result> &out_errors);

#if PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)
static void print_parse_tree(std::ofstream &out, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree &tree, size_t index);
#endif // PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)

void c_parser::initialize_parser() {
	wl_assert(!g_parser_initialized);

	// Build the production set from the provided rules
	c_lr_production_set production_set;
	production_set.initialize(k_token_type_count, k_parser_nonterminal_count);
	for (size_t index = 0; index < NUMBEROF(k_production_table); index++) {
		production_set.add_production(k_production_table[index]);
	}

#if PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_lr_parser.initialize(production_set);
#else // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_lr_parser.initialize(production_set,
		c_wrapped_array_const<c_lr_action>::construct(k_lr_action_table),
		c_wrapped_array_const<uint32>::construct(k_lr_goto_table));
#endif // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_parser_initialized = true;
}

void c_parser::shutdown_parser() {
	wl_assert(g_parser_initialized);

	// Clear memory
	g_lr_parser = c_lr_parser();
	g_parser_initialized = false;
}

s_compiler_result c_parser::process(const s_compiler_context &context, const s_lexer_output &input,
	s_parser_output &output, std::vector<s_compiler_result> &out_errors) {
	wl_assert(g_parser_initialized);
	s_compiler_result result;
	result.clear();

	bool failed = false;

#if PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)
	{
		// Clear the tree output
		std::ofstream out("parse_tree.txt");
	}
#endif // PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)

	// Iterate over each source file and attempt to process it
	for (size_t source_file_index = 0; source_file_index < context.source_files.size(); source_file_index++) {
		const s_compiler_source_file &source_file = context.source_files[source_file_index];
		output.source_file_output.push_back(s_parser_source_file_output());
		failed |= !process_source_file(source_file, input.source_file_output[source_file_index],
			static_cast<int32>(source_file_index), output.source_file_output.back(), out_errors);
	}

	if (failed) {
		// Don't associate the error with a particular file, those errors are collected through out_errors
		result.result = k_compiler_result_parser_error;
		result.message = "Parser encountered error(s)";
	}

	return result;
}

static bool process_source_file(
	const s_compiler_source_file &source_file,
	const s_lexer_source_file_output &source_file_input,
	int32 source_file_index,
	s_parser_source_file_output &source_file_output,
	std::vector<s_compiler_result> &out_errors) {
	// Initially assume success
	bool result = true;

	struct s_parser_context {
		const s_lexer_source_file_output *token_stream;
		size_t next_token_index;

		static bool get_next_token(void *context, uint16 &out_token) {
			s_parser_context *typed_context = static_cast<s_parser_context *>(context);
			if (typed_context->next_token_index < typed_context->token_stream->tokens.size()) {
				out_token = static_cast<uint16>(
					typed_context->token_stream->tokens[typed_context->next_token_index].token_type);
				typed_context->next_token_index++;
				return true;
			} else {
				return false;
			}
		}
	};

	// Set up the context for our get_next_token function
	s_parser_context context;
	context.token_stream = &source_file_input;
	context.next_token_index = 0;

	std::vector<size_t> error_tokens;
	source_file_output.parse_tree = g_lr_parser.parse_token_stream(s_parser_context::get_next_token, &context,
		error_tokens);
#if PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)
	if (error_tokens.empty()) {
		std::ofstream out("parse_tree.txt", std::ios::app);
		print_parse_tree(out, source_file_input,
			source_file_output.parse_tree, source_file_output.parse_tree.get_root_node_index());
		out << "\n";
	}
#endif // PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)

	for (size_t error_index = 0; error_index < error_tokens.size(); error_index++) {
		const s_token &token = source_file_input.tokens[error_tokens[error_index]];
		s_compiler_result error;
		error.result = k_compiler_result_unexpected_token;
		error.source_location = token.source_location;
		error.message = "Unexpected token '" + token.token_string.to_std_string() + "'";
		out_errors.push_back(error);
	}

	if (!error_tokens.empty()) {
		result = false;
	}

	return result;
}

#if PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)
static const char *k_parse_tree_output_terminal_strings[] = {
	"invalid",
	"keyword-const",
	"keyword-in",
	"keyword-out",
	"keyword-module",
	"keyword-void",
	"keyword-real",
	"keyword-bool",
	"keyword-string",
	"keyword-return",
	"identifier",
	"constant-real",
	"constant-bool",
	"constant-string",
	"left-parenthesis",
	"right-parenthesis",
	"left-brace",
	"right-brace",
	"comma",
	"semicolon",
	"operator-assignment",
	"operator-addition",
	"operator-subtraction",
	"operator-multiplication",
	"operator-division",
	"operator-modulo",
	"operator-concatenation",
	"comment"
};
static_assert(NUMBEROF(k_parse_tree_output_terminal_strings) == k_token_type_count, "Incorrect array size");

static const char *k_parse_tree_output_nonterminal_strings[] = {
	"start",
	"global-scope",
	"global-scope-item-list",
	"global-scope-item",
	"type",
	"type-or-void",
	"module-declaration",
	"module-declaration-arguments",
	"module-declaration-argument-list",
	"module-declaration-argument",
	"module-declaration-argument-qualifier",
	"scope",
	"scope-item-list",
	"scope-item",
	"module-return-statement",
	"named-value-declaration",
	"named-value-assignment",
	"expression",
	"expression-assignment",
	"expr-1",
	"expr-2",
	"expr-3",
	"expr-4",
	"module-call",
	"module-call-argument-list"
};
static_assert(NUMBEROF(k_parse_tree_output_nonterminal_strings) == k_parser_nonterminal_count, "Incorrect array size");

#define PRINT_TERMINAL_STRINGS 1

static void print_parse_tree(std::ofstream &out, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree &tree, size_t index) {
	// For use with http://ironcreek.net/phpsyntaxtree/

	const c_lr_parse_tree_node &node = tree.get_node(index);

	out << "[";
	if (node.get_symbol().is_terminal()) {
#if PREDEFINED(PRINT_TERMINAL_STRINGS)
		out << tokens.tokens[node.get_token_index()].token_string.to_std_string();
#else PREDEFINED(PRINT_TERMINAL_STRINGS)
		out << k_parse_tree_output_terminal_strings[node.get_symbol().get_index()];
#endif // PREDEFINED(PRINT_TERMINAL_STRINGS)
	} else {
		out << k_parse_tree_output_nonterminal_strings[node.get_symbol().get_index()];
	}

	if (node.get_child_index() != c_lr_parse_tree::k_invalid_index) {
		out << " ";
		print_parse_tree(out, tokens, tree, node.get_child_index());
	}

	out << "]";

	if (node.get_sibling_index() != c_lr_parse_tree::k_invalid_index) {
		print_parse_tree(out, tokens, tree, node.get_sibling_index());
	}
}
#endif // PREDEFINED(OUTPUT_PARSE_TREE_ENABLED)