#include "compiler/ast_builder.h"
#include "compiler/ast.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "execution_graph/native_modules.h"
#include <stdexcept>

static const e_ast_data_type k_native_module_argument_type_to_ast_data_type_mapping[] = {
	k_ast_data_type_real,	// k_native_module_argument_type_real
	k_ast_data_type_bool,	// k_native_module_argument_type_bool
	k_ast_data_type_string	// k_native_module_argument_type_string
};
static_assert(NUMBEROF(k_native_module_argument_type_to_ast_data_type_mapping) == k_native_module_argument_type_count,
	"Native module argument type to ast data type mismatch");

static const e_ast_qualifier k_native_module_argument_qualifier_to_ast_qualifier_mapping[] = {
	k_ast_qualifier_in,		// k_native_module_argument_qualifier_in
	k_ast_qualifier_out,	// k_native_module_argument_qualifier_out
	k_ast_qualifier_in		// k_native_module_argument_qualifier_constant
};
static_assert(NUMBEROF(k_native_module_argument_qualifier_to_ast_qualifier_mapping) ==
	k_native_module_argument_qualifier_count, "Native module argument qualifier to ast qualifier mismatch");

static e_ast_data_type get_ast_data_type(e_native_module_argument_type type) {
	wl_assert(VALID_INDEX(type, k_native_module_argument_type_count));
	return k_native_module_argument_type_to_ast_data_type_mapping[type];
}

static e_ast_qualifier get_ast_qualifier(e_native_module_argument_qualifier qualifier) {
	wl_assert(VALID_INDEX(qualifier, k_native_module_argument_qualifier_count));
	return k_native_module_argument_qualifier_to_ast_qualifier_mapping[qualifier];
}

static bool node_is_type(const c_lr_parse_tree_node &node, e_token_type terminal_type) {
	return node.get_symbol().is_terminal() && node.get_symbol().get_index() == terminal_type;
}

static bool node_is_type(const c_lr_parse_tree_node &node, e_parser_nonterminal nonterminal_type) {
	return !node.get_symbol().is_terminal() && node.get_symbol().get_index() == nonterminal_type;
}

static e_ast_data_type get_data_type_from_node(const c_lr_parse_tree_node &node) {
	if (node_is_type(node, k_token_type_keyword_void)) {
		return k_ast_data_type_void;
	} else if (node_is_type(node, k_token_type_keyword_module)) {
		return k_ast_data_type_module;
	} else if (node_is_type(node, k_token_type_keyword_real)) {
		return k_ast_data_type_real;
	} else if (node_is_type(node, k_token_type_keyword_bool)) {
		return k_ast_data_type_bool;
	} else if (node_is_type(node, k_token_type_keyword_string)) {
		return k_ast_data_type_string;
	} else {
		wl_unreachable();
		return k_ast_data_type_count;
	}
}

static e_ast_qualifier get_qualifier_from_node(const c_lr_parse_tree_node &node) {
	if (node_is_type(node, k_token_type_keyword_in)) {
		return k_ast_qualifier_in;
	} else if (node_is_type(node, k_token_type_keyword_out)) {
		return k_ast_qualifier_out;
	} else {
		return k_ast_qualifier_count;
		wl_unreachable();
	}
}

static std::vector<size_t> build_left_recursive_list(const c_lr_parse_tree &parse_tree, size_t start_node_index,
	e_parser_nonterminal list_node_type);

static void build_native_module_declarations(c_ast_node_scope *global_scope);

static void add_parse_tree_to_global_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, c_ast_node_scope *global_scope);

static c_ast_node *build_global_scope_item(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &global_scope_item_node);

static c_ast_node_module_declaration *build_module_declaration(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &module_declaration_node);

static c_ast_node_scope *build_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_node,
	const std::vector<c_ast_node *> &initial_children);

static std::vector<c_ast_node *> build_scope_items(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_item_node);

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node);

static c_ast_node_named_value_assignment *build_named_value_assignment(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &named_value_assignment_node);

static c_ast_node_return_statement *build_return_statement(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &return_statement_node);

static std::vector<c_ast_node *> build_repeat_loop(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &return_statement_node);

static c_ast_node_expression *build_expression(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &expression_node);

static c_ast_node_module_call *build_binary_operator_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0);

static c_ast_node_module_call *build_unary_operator_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0);

static c_ast_node_module_call *build_module_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0);

c_ast_node *c_ast_builder::build_ast(const s_lexer_output &lexer_output, const s_parser_output &parser_output) {
	// Iterate the parse tree and build up the AST
	// Any valid parse tree should be able to produce an AST (though further analysis of the AST may indicate errors),
	// so this phase should only ever assert on unexpected input, since the parse tree is assumed to be valid.

	c_ast_node_scope *global_scope = new c_ast_node_scope();

	// The very first thing we do is add all native modules
	build_native_module_declarations(global_scope);

	for (size_t parse_tree_index = 0; parse_tree_index < parser_output.source_file_output.size(); parse_tree_index++) {
		add_parse_tree_to_global_scope(parser_output.source_file_output[parse_tree_index].parse_tree,
			lexer_output.source_file_output[parse_tree_index], global_scope);
	}

	return global_scope;
}

static std::vector<size_t> build_left_recursive_list(const c_lr_parse_tree &parse_tree, size_t start_node_index,
	e_parser_nonterminal list_node_type) {
	// The left-recursive item list [a,b,c] would appear in one of the two forms in the parse tree:
	//       N   |     N
	//      / \  |    / \
	//     N   c |   N   c
	//    / \    |  / \
	//   N   b   | N   b
	//  / \      | |
	// N   a     | a
	// where N is an item list node.

	std::vector<size_t> list_node_indices;
	c_lr_parse_tree_iterator it(parse_tree, start_node_index);
	while (it.is_valid()) {
		const c_lr_parse_tree_node &list_node = it.get_node();
		if (node_is_type(list_node, list_node_type)) {
			if (list_node.has_sibling()) {
				list_node_indices.push_back(list_node.get_sibling_index());
			}

			// The first child is another list node, or invalid
			it.follow_child();
		} else {
			// This list is in the second form
			list_node_indices.push_back(it.get_node_index());
			break;
		}
	}

	// Due to the left-recursive rule, we end up visiting the actual items themselves in reverse, so accumulate and flip
	// the order
	size_t half = list_node_indices.size() / 2;
	for (size_t index = 0; index < half; index++) {
		std::swap(list_node_indices[index], list_node_indices[list_node_indices.size() - index - 1]);
	}

	return list_node_indices;
}

static void build_native_module_declarations(c_ast_node_scope *global_scope) {
	for (uint32 index = 0; index < c_native_module_registry::get_native_module_count(); index++) {
		const s_native_module &native_module = c_native_module_registry::get_native_module(index);

		c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
		module_declaration->set_is_native(true);
		module_declaration->set_native_module_index(index);
		module_declaration->set_name(native_module.name);
		module_declaration->set_return_type(k_ast_data_type_void);

		bool first_out_argument_found = false;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			s_native_module_argument argument = native_module.arguments[arg];
			if (native_module.first_output_is_return &&
				argument.qualifier == k_native_module_argument_qualifier_out &&
				!first_out_argument_found) {
				// The first out argument is routed through the return value, so don't add a declaration for it
				module_declaration->set_return_type(get_ast_data_type(argument.type));
				first_out_argument_found = true;
			} else {
				// Add a declaration for this argument
				c_ast_node_named_value_declaration *argument_declaration = new c_ast_node_named_value_declaration();
				argument_declaration->set_qualifier(get_ast_qualifier(argument.qualifier));
				argument_declaration->set_data_type(get_ast_data_type(argument.type));
				module_declaration->add_argument(argument_declaration);
			}
		}

		global_scope->add_child(module_declaration);
	}
}

static void add_parse_tree_to_global_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, c_ast_node_scope *global_scope) {
	const c_lr_parse_tree_node &root_node = parse_tree.get_node(parse_tree.get_root_node_index());
	wl_assert(node_is_type(root_node, k_parser_nonterminal_start));

	const c_lr_parse_tree_node &global_scope_node = parse_tree.get_node(root_node.get_child_index());
	wl_assert(node_is_type(global_scope_node, k_parser_nonterminal_global_scope));

	// Iterate through the list of global scope items
	std::vector<size_t> item_indices = build_left_recursive_list(parse_tree, global_scope_node.get_child_index(),
		k_parser_nonterminal_global_scope_item_list);

	for (size_t index = 0; index < item_indices.size(); index++) {
		const c_lr_parse_tree_node &global_scope_item_node = parse_tree.get_node(item_indices[index]);
		wl_assert(node_is_type(global_scope_item_node, k_parser_nonterminal_global_scope_item));
		c_ast_node *item = build_global_scope_item(parse_tree, tokens, global_scope_item_node);
		global_scope->add_child(item);
	}
}

static c_ast_node *build_global_scope_item(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &global_scope_item_node) {
	// Determine what type the scope item is
	wl_assert(global_scope_item_node.has_child());
	wl_assert(!global_scope_item_node.has_sibling());

	const c_lr_parse_tree_node &item_type_node = parse_tree.get_node(global_scope_item_node.get_child_index());

	wl_assert(!item_type_node.get_symbol().is_terminal());
	switch (item_type_node.get_symbol().get_index()) {
	case k_parser_nonterminal_module_declaration:
		return build_module_declaration(parse_tree, tokens, item_type_node);

	default:
		// Invalid type
		wl_unreachable();
		return NULL;
	}
}

static c_ast_node_module_declaration *build_module_declaration(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &module_declaration_node) {
	wl_assert(node_is_type(module_declaration_node, k_parser_nonterminal_module_declaration));

	c_lr_parse_tree_iterator it(parse_tree, module_declaration_node.get_child_index());

	const c_lr_parse_tree_node &module_keyword_node = it.get_node();
	wl_assert(node_is_type(module_keyword_node, k_token_type_keyword_module));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_return_type_node = it.get_node();
	wl_assert(node_is_type(module_return_type_node, k_parser_nonterminal_type_or_void));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_name_node = it.get_node();
	wl_assert(node_is_type(module_name_node, k_token_type_identifier));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_token_type_left_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_declaration_arguments_node = it.get_node();
	wl_assert(node_is_type(module_declaration_arguments_node, k_parser_nonterminal_module_declaration_arguments));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_token_type_right_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_node = it.get_node();
	wl_assert(node_is_type(scope_node, k_parser_nonterminal_scope));
	wl_assert(!it.has_sibling());

	c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
	module_declaration->set_source_location(
		tokens.tokens[module_keyword_node.get_token_index()].source_location);

	const c_lr_parse_tree_node &return_type_node = parse_tree.get_node(module_return_type_node.get_child_index());
	e_ast_data_type return_type = get_data_type_from_node(return_type_node);

	module_declaration->set_return_type(return_type);
	module_declaration->set_name(tokens.tokens[module_name_node.get_token_index()].token_string.to_std_string());

	std::vector<c_ast_node *> initial_scope_children;

	// Read the arguments, if they exist
	if (module_declaration_arguments_node.has_child()) {
		const c_lr_parse_tree_node &module_declaration_argument_list_node =
			parse_tree.get_node(module_declaration_arguments_node.get_child_index());

		// The argument list node is either an argument declaration, or a list, comma, and declaration
		std::vector<size_t> argument_indices = build_left_recursive_list(
			parse_tree, module_declaration_argument_list_node.get_child_index(),
			k_parser_nonterminal_module_declaration_argument_list);

		// Skip the commas
		for (size_t index = 0; index < argument_indices.size(); index++) {
			if (index == 0) {
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]),
					k_parser_nonterminal_module_declaration_argument));
			} else {
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]),
					k_token_type_comma));
				argument_indices[index] = parse_tree.get_node(argument_indices[index]).get_sibling_index();
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]),
					k_parser_nonterminal_module_declaration_argument));
			}
		}

		// Process arguments
		for (size_t argument_index = 0; argument_index < argument_indices.size(); argument_index++) {
			c_ast_node_named_value_declaration *argument_declaration = new c_ast_node_named_value_declaration();
			initial_scope_children.push_back(argument_declaration);

			c_lr_parse_tree_iterator arg_it(parse_tree, argument_indices[argument_index]);
			wl_assert(node_is_type(arg_it.get_node(), k_parser_nonterminal_module_declaration_argument));
			arg_it.follow_child();

			const c_lr_parse_tree_node &argument_qualifier_node = arg_it.get_node();
			wl_assert(node_is_type(argument_qualifier_node,
				k_parser_nonterminal_module_declaration_argument_qualifier));
			arg_it.follow_sibling();

			const c_lr_parse_tree_node &argument_type_node = arg_it.get_node();
			wl_assert(node_is_type(argument_type_node, k_parser_nonterminal_type));
			arg_it.follow_sibling();

			const c_lr_parse_tree_node &argument_name_node = arg_it.get_node();
			wl_assert(node_is_type(argument_name_node, k_token_type_identifier));
			wl_assert(!arg_it.has_sibling());

			wl_assert(argument_qualifier_node.has_child());
			const c_lr_parse_tree_node &qualifier_node = parse_tree.get_node(argument_qualifier_node.get_child_index());
			e_ast_qualifier qualifier = get_qualifier_from_node(qualifier_node);

			wl_assert(argument_type_node.has_child());
			const c_lr_parse_tree_node &type_node = parse_tree.get_node(argument_type_node.get_child_index());
			e_ast_data_type data_type = get_data_type_from_node(type_node);

			argument_declaration->set_source_location(
				tokens.tokens[qualifier_node.get_token_index()].source_location);

			argument_declaration->set_qualifier(qualifier);
			argument_declaration->set_data_type(data_type);

			argument_declaration->set_name(
				tokens.tokens[argument_name_node.get_token_index()].token_string.to_std_string());

			module_declaration->add_argument(argument_declaration);
		}
	}

	// Create the scope with the arguments as the initial children. This is because the arguments are considered part
	// of the module's scope.
	c_ast_node_scope *module_body_scope = build_scope(parse_tree, tokens, scope_node, initial_scope_children);
	module_declaration->set_scope(module_body_scope);

	return module_declaration;
}

static c_ast_node_scope *build_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_node,
	const std::vector<c_ast_node *> &initial_children) {
	wl_assert(node_is_type(scope_node, k_parser_nonterminal_scope));

	c_lr_parse_tree_iterator it(parse_tree, scope_node.get_child_index());

	wl_assert(node_is_type(it.get_node(), k_token_type_left_brace));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_item_list_node = it.get_node();
	wl_assert(node_is_type(it.get_node(), k_parser_nonterminal_scope_item_list));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_token_type_right_brace));
	wl_assert(!it.has_sibling());

	// Add the initial items first
	c_ast_node_scope *scope = new c_ast_node_scope();
	for (size_t initial_index = 0; initial_index < initial_children.size(); initial_index++) {
		scope->add_child(initial_children[initial_index]);
	}

	// Iterate through the items in the module body and add each one to the module's scope
	std::vector<size_t> item_indices = build_left_recursive_list(parse_tree, scope_item_list_node.get_child_index(),
		k_parser_nonterminal_scope_item_list);

	for (size_t index = 0; index < item_indices.size(); index++) {
		const c_lr_parse_tree_node &scope_item_node = parse_tree.get_node(item_indices[index]);
		wl_assert(node_is_type(scope_item_node, k_parser_nonterminal_scope_item));
		std::vector<c_ast_node *> items = build_scope_items(parse_tree, tokens, scope_item_node);
		for (size_t item_index = 0; item_index < items.size(); item_index++) {
			scope->add_child(items[item_index]);
		}
	}

	return scope;
}

static std::vector<c_ast_node *> build_scope_items(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_item_node) {
	// Determine what type the scope item is
	wl_assert(scope_item_node.has_child());
	wl_assert(!scope_item_node.has_sibling());

	std::vector<c_ast_node *> result;

	const c_lr_parse_tree_node &item_type_node = parse_tree.get_node(scope_item_node.get_child_index());

	wl_assert(!item_type_node.get_symbol().is_terminal());
	switch (item_type_node.get_symbol().get_index()) {
	case k_parser_nonterminal_scope:
		result.push_back(build_scope(parse_tree, tokens, item_type_node, std::vector<c_ast_node *>()));
		break;

	case k_parser_nonterminal_named_value_declaration:
		result = build_named_value_declaration_and_optional_assignment(parse_tree, tokens, item_type_node);
		break;

	case k_parser_nonterminal_named_value_assignment:
		result.push_back(build_named_value_assignment(parse_tree, tokens, item_type_node));
		break;

	case k_parser_nonterminal_expression:
	{
		// In this case, we wrap the expression in a named value assignment, but don't fill in the named value being
		// assigned to. This is because we assume that the assignments are from output parameters in a module call.
		// Technically this means we can allow named value assignment nodes with no actual assignment, but it's
		// pointless to do this, and will get optimized away later.
		c_ast_node_named_value_assignment *named_value_assignment = new c_ast_node_named_value_assignment();
		c_ast_node_expression *expression = build_expression(parse_tree, tokens, item_type_node);
		named_value_assignment->set_expression(expression);
		named_value_assignment->set_source_location(expression->get_source_location());
		result.push_back(named_value_assignment);
		break;
	}

	case k_parser_nonterminal_module_return_statement:
		result.push_back(build_return_statement(parse_tree, tokens, item_type_node));
		break;

	case k_parser_nonterminal_repeat_loop:
		result = build_repeat_loop(parse_tree, tokens, item_type_node);
		break;

	default:
		// Invalid type
		wl_unreachable();
	}

	return result;
}

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node) {
	wl_assert(node_is_type(named_value_declaration_node, k_parser_nonterminal_named_value_declaration));

	c_lr_parse_tree_iterator it(parse_tree, named_value_declaration_node.get_child_index());

	// "val", identifier, and assignment node

	const c_lr_parse_tree_node &named_value_type_node = it.get_node();
	wl_assert(node_is_type(named_value_type_node, k_parser_nonterminal_type));
	it.follow_sibling();

	const c_lr_parse_tree_node &name_node = it.get_node();
	wl_assert(node_is_type(name_node, k_token_type_identifier));

	const c_lr_parse_tree_node &type_node = parse_tree.get_node(named_value_type_node.get_child_index());
	e_ast_data_type data_type = get_data_type_from_node(type_node);

	std::vector<c_ast_node *> result;

	c_ast_node_named_value_declaration *declaration = new c_ast_node_named_value_declaration();
	declaration->set_source_location(tokens.tokens[type_node.get_token_index()].source_location);

	declaration->set_data_type(data_type);
	declaration->set_name(tokens.tokens[name_node.get_token_index()].token_string.to_std_string());
	result.push_back(declaration);

	// Read the optional assignment
	if (it.has_sibling()) {
		it.follow_sibling();
		const c_lr_parse_tree_node &expression_assignment_node = it.get_node();
		wl_assert(node_is_type(expression_assignment_node, k_parser_nonterminal_expression_assignment));
		wl_assert(!it.has_sibling());
		it.follow_child();

		wl_assert(node_is_type(it.get_node(), k_token_type_operator_assignment));
		it.follow_sibling();

		const c_lr_parse_tree_node &expression_node = it.get_node();
		wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));
		wl_assert(!it.has_sibling());

		c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);

		c_ast_node_named_value_assignment *assignment = new c_ast_node_named_value_assignment();
		assignment->set_source_location(tokens.tokens[name_node.get_token_index()].source_location);
		assignment->set_named_value(declaration->get_name());
		assignment->set_expression(expression);
		result.push_back(assignment);
	}

	return result;
}

static c_ast_node_named_value_assignment *build_named_value_assignment(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &named_value_assignment_node) {
	wl_assert(node_is_type(named_value_assignment_node, k_parser_nonterminal_named_value_assignment));

	c_lr_parse_tree_iterator it(parse_tree, named_value_assignment_node.get_child_index());

	const c_lr_parse_tree_node &name_node = it.get_node();
	wl_assert(node_is_type(name_node, k_token_type_identifier));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_parser_nonterminal_expression_assignment));
	wl_assert(!it.has_sibling());
	it.follow_child();

	wl_assert(node_is_type(it.get_node(), k_token_type_operator_assignment));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));
	wl_assert(!it.has_sibling());

	c_ast_node_named_value_assignment *result = new c_ast_node_named_value_assignment();
	result->set_source_location(tokens.tokens[name_node.get_token_index()].source_location);

	result->set_named_value(tokens.tokens[name_node.get_token_index()].token_string.to_std_string());
	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);
	result->set_expression(expression);

	return result;

}

static c_ast_node_return_statement *build_return_statement(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &return_statement_node) {
	wl_assert(node_is_type(return_statement_node, k_parser_nonterminal_module_return_statement));

	c_lr_parse_tree_iterator it(parse_tree, return_statement_node.get_child_index());

	const c_lr_parse_tree_node &return_node = it.get_node();
	wl_assert(node_is_type(return_node, k_token_type_keyword_return));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));
	wl_assert(!it.has_sibling());

	c_ast_node_return_statement *result = new c_ast_node_return_statement();
	result->set_source_location(tokens.tokens[return_node.get_token_index()].source_location);

	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);
	result->set_expression(expression);

	return result;
}

static std::vector<c_ast_node *> build_repeat_loop(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &return_statement_node) {
	wl_assert(node_is_type(return_statement_node, k_parser_nonterminal_repeat_loop));

	c_lr_parse_tree_iterator it(parse_tree, return_statement_node.get_child_index());

	const c_lr_parse_tree_node &repeat_node = it.get_node();
	wl_assert(node_is_type(repeat_node, k_token_type_keyword_repeat));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_token_type_left_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), k_token_type_right_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_node = it.get_node();
	wl_assert(node_is_type(scope_node, k_parser_nonterminal_scope));
	wl_assert(!it.has_sibling());

	std::vector<c_ast_node *> result;

	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);

	// Create an "anonymous" assignment for the expression
	c_ast_node_named_value_assignment *named_value_assignment = new c_ast_node_named_value_assignment();
	named_value_assignment->set_expression(expression);
	named_value_assignment->set_source_location(expression->get_source_location());
	result.push_back(named_value_assignment);

	c_ast_node_repeat_loop *repeat_loop = new c_ast_node_repeat_loop();
	repeat_loop->set_source_location(tokens.tokens[repeat_node.get_token_index()].source_location);
	result.push_back(repeat_loop);

	// Point the repeat loop at the expression we just stored in the named value assignment
	repeat_loop->set_expression(named_value_assignment);

	// Create the scope
	c_ast_node_scope *repeat_loop_scope = build_scope(parse_tree, tokens, scope_node, std::vector<c_ast_node *>());
	repeat_loop->set_scope(repeat_loop_scope);

	return result;
}

static c_ast_node_expression *build_expression(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &expression_node) {
	// Result is always an expression
	c_ast_node_expression *result = new c_ast_node_expression();

	// We may need to move through several layers of child nodes before we find the actual expression type
	bool expression_type_found = false;
	const c_lr_parse_tree_node *current_node = &expression_node;
	while (!expression_type_found) {
		if (node_is_type(*current_node, k_parser_nonterminal_expression)) {
			// expression:

			// Always turns into expr_1
			current_node = &parse_tree.get_node(current_node->get_child_index());
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_1)) {
			// expr_1: ||

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_2
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_2)) {
			// expr_2: &&

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_3
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_3)) {
			// expr_3: == !=

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_4
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_4)) {
			// expr_4: > < >= <=

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_5
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_5)) {
			// expr_5: + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_6
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_6)) {
			// expr_6: * / %

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_7
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_7)) {
			// expr_7: unary + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_8
				current_node = &child_node;
			} else {
				result->set_expression_value(build_unary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_8)) {
			// expr_8: parens, identifier, module_call, constant

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (node_is_type(child_node, k_token_type_left_parenthesis)) {
				// We found an expression in parenthesis whose ordering was already resolved by the parser, forward it
				// back to the top level (expression)
				current_node = &parse_tree.get_node(child_node.get_sibling_index());
				wl_assert(node_is_type(parse_tree.get_node(current_node->get_sibling_index()),
					k_token_type_right_parenthesis));
			} else if (node_is_type(child_node, k_token_type_identifier)) {
				// Named value
				c_ast_node_named_value *named_value = new c_ast_node_named_value();
				named_value->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				named_value->set_name(tokens.tokens[child_node.get_token_index()].token_string.to_std_string());
				result->set_expression_value(named_value);
				expression_type_found = true;
			} else if (node_is_type(child_node, k_parser_nonterminal_module_call)) {
				// Module call
				result->set_expression_value(build_module_call(parse_tree, tokens,
					parse_tree.get_node(child_node.get_child_index())));
				expression_type_found = true;
			} else if (node_is_type(child_node, k_token_type_constant_real)) {
				try {
					// This is a real
					std::string string_value = tokens.tokens[child_node.get_token_index()].token_string.to_std_string();
					c_ast_node_constant *constant_node = new c_ast_node_constant();
					constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
					constant_node->set_real_value(std::stof(string_value));
					result->set_expression_value(constant_node);
					expression_type_found = true;
				} catch (const std::invalid_argument &) {
					wl_vhalt("We should have already caught this in the lexer");
				} catch (const std::out_of_range &) {
					wl_vhalt("We should have already caught this in the lexer");
				}
			} else if (node_is_type(child_node, k_token_type_constant_bool)) {
				// This is a bool
				std::string string_value = tokens.tokens[child_node.get_token_index()].token_string.to_std_string();
				c_ast_node_constant *constant_node = new c_ast_node_constant();
				constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				wl_vassert(string_value == k_token_type_constant_bool_false_string ||
					string_value == k_token_type_constant_bool_true_string,
					"We should have already caught this in the lexer");
				bool bool_value = (string_value == k_token_type_constant_bool_true_string);
				constant_node->set_bool_value(bool_value);
				result->set_expression_value(constant_node);
				expression_type_found = true;
			} else if (node_is_type(child_node, k_token_type_constant_string)) {
				// This is a string
				c_compiler_string unescaped_string = tokens.tokens[child_node.get_token_index()].token_string;
				std::string escaped_string;

				// Go through each character and resolve escape sequences
				for (size_t index = 0; index < unescaped_string.get_length(); index++) {
					char ch = unescaped_string[index];
					if (ch == '\\') {
						index++;
						char escape_result;
						size_t advance = compiler_utility::resolve_escape_sequence(
							unescaped_string.advance(index), &escape_result);
						wl_vassert(advance > 0, "We should have already caught this in the lexer");

						index += advance;
						escaped_string.push_back(escape_result);
					} else {
						escaped_string.push_back(ch);
					}
				}

				c_ast_node_constant *constant_node = new c_ast_node_constant();
				constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				constant_node->set_string_value(escaped_string);
				result->set_expression_value(constant_node);
				expression_type_found = true;
			} else {
				wl_unreachable();
			}
		} else {
			// Unknown expression type
			wl_unreachable();
		}
	}

	// Propagate the source location down to the expression wrapper
	result->set_source_location(result->get_expression_value()->get_source_location());
	return result;
}

static c_ast_node_module_call *build_binary_operator_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0) {
	// There should be three nodes of the form "expr operator expr"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());
	const c_lr_parse_tree_node &node_2 = parse_tree.get_node(node_1.get_sibling_index());

	const char *operator_module_name = nullptr;

	// Choose the built-in name of the operator module
	wl_assert(node_1.get_symbol().is_terminal());
	switch (node_1.get_symbol().get_index()) {
	case k_token_type_operator_addition:
		operator_module_name = c_native_module_registry::k_operator_addition_operator_name;
		break;

	case k_token_type_operator_subtraction:
		operator_module_name = c_native_module_registry::k_operator_subtraction_operator_name;
		break;

	case k_token_type_operator_multiplication:
		operator_module_name = c_native_module_registry::k_operator_multiplication_operator_name;
		break;

	case k_token_type_operator_division:
		operator_module_name = c_native_module_registry::k_operator_division_name;
		break;

	case k_token_type_operator_modulo:
		operator_module_name = c_native_module_registry::k_operator_modulo_name;
		break;

	case k_token_type_operator_concatenation:
		operator_module_name = c_native_module_registry::k_operator_concatenation_name;
		break;

	case k_token_type_operator_equal:
		operator_module_name = c_native_module_registry::k_operator_equal_name;
		break;

	case k_token_type_operator_not_equal:
		operator_module_name = c_native_module_registry::k_operator_not_equal_name;
		break;

	case k_token_type_operator_greater:
		operator_module_name = c_native_module_registry::k_operator_greater_name;
		break;

	case k_token_type_operator_less:
		operator_module_name = c_native_module_registry::k_operator_less_name;
		break;

	case k_token_type_operator_greater_equal:
		operator_module_name = c_native_module_registry::k_operator_greater_equal_name;
		break;

	case k_token_type_operator_less_equal:
		operator_module_name = c_native_module_registry::k_operator_less_equal_name;
		break;

	case k_token_type_operator_and:
		operator_module_name = c_native_module_registry::k_operator_and_name;
		break;

	case k_token_type_operator_or:
		operator_module_name = c_native_module_registry::k_operator_or_name;
		break;

	default:
		wl_unreachable();
	}

	c_ast_node_expression *arg_0 = build_expression(parse_tree, tokens, node_0);
	c_ast_node_expression *arg_1 = build_expression(parse_tree, tokens, node_2);

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_1.get_token_index()].source_location);
	module_call->set_name(operator_module_name);
	module_call->add_argument(arg_0);
	module_call->add_argument(arg_1);

	return module_call;
}

static c_ast_node_module_call *build_unary_operator_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0) {
	// There should be two nodes of the form "operator expr"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());

	const char *operator_module_name = nullptr;

	// Choose the built-in name of the operator module
	wl_assert(node_0.get_symbol().is_terminal());
	switch (node_0.get_symbol().get_index()) {
	case k_token_type_operator_addition:
		// +x just returns x, and will be optimized away
		operator_module_name = c_native_module_registry::k_operator_noop_name;
		break;

	case k_token_type_operator_subtraction:
		operator_module_name = c_native_module_registry::k_operator_negation_name;
		break;

	case k_token_type_operator_not:
		operator_module_name = c_native_module_registry::k_operator_not_name;
		break;

	default:
		wl_unreachable();
	}

	c_ast_node_expression *arg_0 = build_expression(parse_tree, tokens, node_1);

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_0.get_token_index()].source_location);
	module_call->set_name(operator_module_name);
	module_call->add_argument(arg_0);

	return module_call;
}

static c_ast_node_module_call *build_module_call(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &node_0) {
	// We expect "identifier ( optional_args )"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());
	const c_lr_parse_tree_node &node_2 = parse_tree.get_node(node_1.get_sibling_index());

	wl_assert(node_is_type(node_0, k_token_type_identifier));
	wl_assert(node_is_type(node_1, k_token_type_left_parenthesis));

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_0.get_token_index()].source_location);
	module_call->set_name(tokens.tokens[node_0.get_token_index()].token_string.to_std_string());

	if (node_is_type(node_2, k_token_type_right_parenthesis)) {
		// No arguments
	} else {
		wl_assert(node_is_type(node_2, k_parser_nonterminal_module_call_argument_list));
		wl_assert(node_is_type(parse_tree.get_node(node_2.get_sibling_index()),
			k_token_type_right_parenthesis));

		// The argument list node is either an expression, or a list, comma, and expression
		std::vector<size_t> argument_indices = build_left_recursive_list(parse_tree, node_2.get_child_index(),
			k_parser_nonterminal_module_call_argument_list);

		// Skip the commas
		for (size_t index = 0; index < argument_indices.size(); index++) {
			if (index == 0) {
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]), k_parser_nonterminal_expression));
			} else {
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]), k_token_type_comma));
				argument_indices[index] = parse_tree.get_node(argument_indices[index]).get_sibling_index();
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]), k_parser_nonterminal_expression));
			}
		}

		// Process arguments
		for (size_t argument_index = 0; argument_index < argument_indices.size(); argument_index++) {
			c_ast_node_expression *expression =
				build_expression(parse_tree, tokens, parse_tree.get_node(argument_indices[argument_index]));

			module_call->add_argument(expression);
		}
	}

	return module_call;
}
