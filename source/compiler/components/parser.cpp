#include "compiler/components/lexer.h"
#include "compiler/components/parser.h"

#include "generated/wavelang_grammar.h"

#define OUTPUT_PARSE_TREE_ENABLED 1 // $TODO $COMPILER disable this when compiler is done

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
#include "common/utility/graphviz_generator.h"

#include <fstream>
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

static bool g_parser_initialized = false;
static c_lr_parser g_lr_parser;

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
static constexpr const char *k_parse_tree_output_extension = ".gv";
static void print_parse_tree(c_graphviz_generator &graph, const s_compiler_source_file &source_file);
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

void c_parser::initialize() {
	wl_assert(!g_parser_initialized);
	initialize_wavelang_parser(g_lr_parser);
	g_parser_initialized = true;
}

void c_parser::deinitialize() {
	wl_assert(g_parser_initialized);

	// Clear memory
	g_lr_parser = c_lr_parser();
	g_parser_initialized = false;
}

bool c_parser::process(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	wl_assert(g_parser_initialized);

	s_compiler_source_file &source_file = context.get_source_file(source_file_handle);

	struct s_parser_context {
		c_wrapped_array<const s_token> tokens;
		size_t next_token_index;

		static bool get_next_token(void *context, uint16 &token_out) {
			s_parser_context *this_ptr = static_cast<s_parser_context *>(context);
			if (this_ptr->next_token_index < this_ptr->tokens.get_count()) {
				token_out = static_cast<uint16>(this_ptr->tokens[this_ptr->next_token_index++].token_type);
				return true;
			} else {
				return false;
			}
		}
	};

	s_parser_context parser_context;
	parser_context.tokens = c_wrapped_array<const s_token>(source_file.tokens);
	parser_context.next_token_index = 0;

	std::vector<size_t> error_tokens;
	source_file.parse_tree = g_lr_parser.parse_token_stream(
		s_parser_context::get_next_token,
		&parser_context,
		error_tokens);

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
	if (error_tokens.empty()) {
		c_graphviz_generator parse_tree_graph;
		parse_tree_graph.set_graph_name("parse_tree");
		print_parse_tree(parse_tree_graph, source_file);
		parse_tree_graph.output_to_file(
			("parse_tree_" + std::to_string(source_file_handle.get_data()) + k_parse_tree_output_extension).c_str());
	}
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

	for (size_t token_index : error_tokens) {
		const s_token &token = source_file.tokens[token_index];
		context.error(
			e_compiler_error::k_unexpected_token,
			token.source_location,
			"Unexpected token '%s'",
			escape_token_string_for_output(token.token_string).c_str());
	}

	return error_tokens.empty();
}

#if IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)

#define PRINT_TERMINAL_STRINGS 1

static void print_parse_tree(c_graphviz_generator &graph, const s_compiler_source_file &source_file) {
	struct s_node_context {
		size_t index;
		size_t parent_index;
	};

	std::vector<s_node_context> node_stack;
	node_stack.push_back({ source_file.parse_tree.get_root_node_index(), c_lr_parse_tree::k_invalid_index });

	while (node_stack.empty()) {
		s_node_context node_context = node_stack.back();
		node_stack.pop_back();

		const c_lr_parse_tree_node &node = source_file.parse_tree.get_node(node_context.index);

		c_graphviz_node graph_node;

		std::string node_name = "node_" + std::to_string(node_context.index);
		graph_node.set_name(node_name.c_str());
		graph_node.set_shape("box");

		if (node.get_symbol().is_terminal()) {
			graph_node.set_style("rounded");
#if IS_TRUE(PRINT_TERMINAL_STRINGS)
			std::string label(source_file.tokens[node.get_token_index()].token_string);
			graph_node.set_label(c_graphviz_generator::escape_string(label.c_str()).c_str());
#else IS_TRUE(PRINT_TERMINAL_STRINGS)
			graph_node.set_label(
				get_wavelang_terminal_reflection_string(
					static_cast<e_token_type>(node.get_symbol().get_index())));
#endif // IS_TRUE(PRINT_TERMINAL_STRINGS)
		} else {
			graph_node.set_label(
				get_wavelang_nonterminal_reflection_string(
					static_cast<e_parser_nonterminal>(node.get_symbol().get_index())));
		}

		graph.add_node(graph_node);

		if (node_context.parent_index != c_lr_parse_tree::k_invalid_index) {
			graph.add_edge(
				("node_" + std::to_string(node_context.parent_index)).c_str(),
				node_name.c_str());
		}

		if (node.has_sibling()) {
			node_stack.push_back({ node.get_sibling_index(), node_context.index });
		}

		if (node.has_child()) {
			node_stack.push_back({ node.get_child_index(), node_context.index });
		}
	}
}
#endif // IS_TRUE(OUTPUT_PARSE_TREE_ENABLED)
