#include "compiler/lexer.h"
#include "compiler/parser.h"

#define OUTPUT_PARSE_TREE_ENABLED 0

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
#include "common/utility/graphviz_generator.h"

#include <fstream>
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

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
	s_lr_production production;
	production.lhs = lhs;
	production.rhs[0] = rhs_0;
	production.rhs[1] = rhs_1;
	production.rhs[2] = rhs_2;
	production.rhs[3] = rhs_3;
	production.rhs[4] = rhs_4;
	production.rhs[5] = rhs_5;
	production.rhs[6] = rhs_6;
	production.rhs[7] = rhs_7;
	production.rhs[8] = rhs_8;
	production.rhs[9] = rhs_9;
	return production;
}

// Shorthand
#define PROD(x, ...) make_production(x, ##__VA_ARGS__), // Hack to allow the macro to accept commas
#define TS(x) c_lr_symbol(true, static_cast<uint16>(e_token_type::k_ ## x))
#define NS(x) c_lr_symbol(false, static_cast<uint16>(e_parser_nonterminal::k_ ## x))

// Production table
static const s_lr_production k_production_table[] = {
	// txt file, because it's annoying to try to edit this as an inl file
	#include "parser_rules.txt"
};

#if !IS_TRUE(LR_PARSE_TABLE_GENERATION_ENABLED)
#include "lr_parse_tables.inl"
#endif IS_TRUE(LR_PARSE_TABLE_GENERATION_ENABLED)

static bool g_parser_initialized = false;
static c_lr_parser g_lr_parser;

static bool process_source_file(
	const s_compiler_source_file &source_file,
	const s_lexer_source_file_output &source_file_input,
	int32 source_file_index,
	s_parser_source_file_output &source_file_output,
	std::vector<s_compiler_result> &errors_out);

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
static constexpr char k_parse_tree_output_filename[] = "parse_tree.gv";
static void print_parse_tree(
	c_graphviz_generator &graph,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree &tree,
	size_t index,
	size_t parent_index = static_cast<size_t>(-1));
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

void c_parser::initialize_parser() {
	wl_assert(!g_parser_initialized);

	// Build the production set from the provided rules
	c_lr_production_set production_set;
	production_set.initialize(enum_count<e_token_type>(), enum_count<e_parser_nonterminal>());
	for (size_t index = 0; index < array_count(k_production_table); index++) {
		production_set.add_production(k_production_table[index]);
	}

#if IS_TRUE(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_lr_parser.initialize(production_set);
#else // IS_TRUE(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_lr_parser.initialize(
		production_set,
		c_wrapped_array<const c_lr_action>::construct(k_lr_action_table),
		c_wrapped_array<const uint32>::construct(k_lr_goto_table));
#endif // IS_TRUE(LR_PARSE_TABLE_GENERATION_ENABLED)
	g_parser_initialized = true;
}

void c_parser::shutdown_parser() {
	wl_assert(g_parser_initialized);

	// Clear memory
	g_lr_parser = c_lr_parser();
	g_parser_initialized = false;
}

s_compiler_result c_parser::process(
	const s_compiler_context &context,
	const s_lexer_output &input,
	s_parser_output &output,
	std::vector<s_compiler_result> &errors_out) {
	wl_assert(g_parser_initialized);
	s_compiler_result result;
	result.clear();

	bool failed = false;

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
	{
		// Clear the tree output
		std::ofstream out(k_parse_tree_output_filename);
	}
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

	// Iterate over each source file and attempt to process it
	for (size_t source_file_index = 0; source_file_index < context.source_files.size(); source_file_index++) {
		const s_compiler_source_file &source_file = context.source_files[source_file_index];
		output.source_file_output.push_back(s_parser_source_file_output());
		failed |= !process_source_file(
			source_file,
			input.source_file_output[source_file_index],
			cast_integer_verify<int32>(source_file_index),
			output.source_file_output.back(), errors_out);
	}

	if (failed) {
		// Don't associate the error with a particular file, those errors are collected through errors_out
		result.result = e_compiler_result::k_parser_error;
		result.message = "Parser encountered error(s)";
	}

	return result;
}

static bool process_source_file(
	const s_compiler_source_file &source_file,
	const s_lexer_source_file_output &source_file_input,
	int32 source_file_index,
	s_parser_source_file_output &source_file_output,
	std::vector<s_compiler_result> &errors_out) {
	// Initially assume success
	bool result = true;

	struct s_parser_context {
		const s_lexer_source_file_output *token_stream;
		size_t next_token_index;

		static bool get_next_token(void *context, uint16 &token_out) {
			s_parser_context *typed_context = static_cast<s_parser_context *>(context);
			if (typed_context->next_token_index < typed_context->token_stream->tokens.size()) {
				token_out = static_cast<uint16>(
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
	source_file_output.parse_tree = g_lr_parser.parse_token_stream(
		s_parser_context::get_next_token,
		&context,
		error_tokens);
#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
	if (error_tokens.empty()) {
		c_graphviz_generator parse_tree_graph;
		parse_tree_graph.set_graph_name("parse_tree");
		print_parse_tree(
			parse_tree_graph,
			source_file_input,
			source_file_output.parse_tree,
			source_file_output.parse_tree.get_root_node_index());
		parse_tree_graph.output_to_file(k_parse_tree_output_filename);
	}
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

	for (size_t error_index = 0; error_index < error_tokens.size(); error_index++) {
		const s_token &token = source_file_input.tokens[error_tokens[error_index]];
		s_compiler_result error;
		error.result = e_compiler_result::k_unexpected_token;
		error.source_location = token.source_location;
		error.message = "Unexpected token '" + token.token_string.to_std_string() + "'";
		errors_out.push_back(error);
	}

	if (!error_tokens.empty()) {
		result = false;
	}

	return result;
}

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
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
	"keyword-repeat",
	"identifier",
	"constant-real",
	"constant-bool",
	"constant-string",
	"left-parenthesis",
	"right-parenthesis",
	"left-brace",
	"right-brace",
	"left-bracket",
	"right-bracket",
	"comma",
	"semicolon",
	"operator-assignment",
	"operator-addition",
	"operator-subtraction",
	"operator-multiplication",
	"operator-division",
	"operator-modulo",
	"operator-not",
	"operator-equal",
	"operator-not-equal",
	"operator-greater",
	"operator-less",
	"operator-greater-equal",
	"operator-less-equal",
	"operator-and",
	"operator-or",
	"comment"
};
static_assert(array_count(k_parse_tree_output_terminal_strings) == enum_count<e_token_type>(), "Incorrect array size");

static const char *k_parse_tree_output_nonterminal_strings[] = {
	"start",
	"global-scope",
	"global-scope-item-list",
	"global-scope-item",
	"basic-type",
	"array-type",
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
	"repeat-loop",
	"expression",
	"expression-assignment",
	"expr-1",
	"expr-2",
	"expr-3",
	"expr-4",
	"expr-5",
	"expr-6",
	"expr-7",
	"expr-8",
	"expr-9",
	"module-call",
	"module-call-argument-list",
	"constant-array",
	"constant-array-list",
	"array-dereference"
};
static_assert(array_count(k_parse_tree_output_nonterminal_strings) == enum_count<e_parser_nonterminal>(),
	"Incorrect array size");

#define PRINT_TERMINAL_STRINGS 1

static void print_parse_tree(
	c_graphviz_generator &graph,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree &tree,
	size_t index,
	size_t parent_index) {
	const c_lr_parse_tree_node &node = tree.get_node(index);

	c_graphviz_node graph_node;

	std::string node_name = "node_" + std::to_string(index);
	graph_node.set_name(node_name.c_str());
	graph_node.set_shape("box");

	if (node.get_symbol().is_terminal()) {
		graph_node.set_style("rounded");
#if IS_TRUE(PRINT_TERMINAL_STRINGS)
		graph_node.set_label(c_graphviz_generator::escape_string(
			tokens.tokens[node.get_token_index()].token_string.to_std_string().c_str()).c_str());
#else IS_TRUE(PRINT_TERMINAL_STRINGS)
		graph_node.set_label(k_parse_tree_output_terminal_strings[node.get_symbol().get_index()]);
#endif // IS_TRUE(PRINT_TERMINAL_STRINGS)
	} else {
		graph_node.set_label(k_parse_tree_output_nonterminal_strings[node.get_symbol().get_index()]);
	}

	graph.add_node(graph_node);

	if (parent_index != static_cast<size_t>(-1)) {
		graph.add_edge(
			("node_" + std::to_string(parent_index)).c_str(),
			node_name.c_str());
	}

	if (node.get_child_index() != c_lr_parse_tree::k_invalid_index) {
		print_parse_tree(graph, tokens, tree, node.get_child_index(), index);
	}

	if (node.get_sibling_index() != c_lr_parse_tree::k_invalid_index) {
		print_parse_tree(graph, tokens, tree, node.get_sibling_index(), parent_index);
	}
}
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
