#include "compiler/ast.h"
#include "compiler/ast_builder.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"

#include "execution_graph/native_module.h"
#include "execution_graph/native_module_registry.h"

#include <stdexcept>

static const e_ast_primitive_type k_native_module_primitive_type_to_ast_primitive_type_mapping[] = {
	e_ast_primitive_type::k_real,	// e_native_module_primitive_type::k_real
	e_ast_primitive_type::k_bool,	// e_native_module_primitive_type::k_bool
	e_ast_primitive_type::k_string	// e_native_module_primitive_type::k_string
};
static_assert(
	NUMBEROF(k_native_module_primitive_type_to_ast_primitive_type_mapping) ==
	enum_count<e_native_module_primitive_type>(),
	"Native module argument primitive type to ast primitive type mismatch");

static const e_ast_qualifier k_native_module_qualifier_to_ast_qualifier_mapping[] = {
	e_ast_qualifier::k_in,	// e_native_module_qualifier::k_in
	e_ast_qualifier::k_out,	// e_native_module_qualifier::k_out
	e_ast_qualifier::k_in	// e_native_module_qualifier::k_constant
};
static_assert(
	NUMBEROF(k_native_module_qualifier_to_ast_qualifier_mapping) == enum_count<e_native_module_qualifier>(),
	"Native module qualifier to ast qualifier mismatch");

static c_ast_data_type get_ast_data_type(c_native_module_data_type type) {
	return c_ast_data_type(
		k_native_module_primitive_type_to_ast_primitive_type_mapping[enum_index(type.get_primitive_type())],
		type.is_array());
}

static e_ast_qualifier get_ast_qualifier(e_native_module_qualifier qualifier) {
	wl_assert(valid_enum_index(qualifier));
	return k_native_module_qualifier_to_ast_qualifier_mapping[enum_index(qualifier)];
}

static bool node_is_type(const c_lr_parse_tree_node &node, e_token_type terminal_type) {
	return node.get_symbol().is_terminal() && node.get_symbol().get_index() == enum_index(terminal_type);
}

static bool node_is_type(const c_lr_parse_tree_node &node, e_parser_nonterminal nonterminal_type) {
	return !node.get_symbol().is_terminal() && node.get_symbol().get_index() == enum_index(nonterminal_type);
}

static c_ast_data_type get_data_type_from_node(const c_lr_parse_tree &parse_tree, const c_lr_parse_tree_node &node) {
	const c_lr_parse_tree_node *type_node = &node;

	if (node_is_type(*type_node, e_parser_nonterminal::k_type_or_void) ||
		node_is_type(*type_node, e_parser_nonterminal::k_type)) {
		type_node = &parse_tree.get_node(type_node->get_child_index());
	}

	bool is_array = false;
	if (node_is_type(*type_node, e_token_type::k_keyword_void)) {
		// Don't step down another level
	} else if (node_is_type(*type_node, e_parser_nonterminal::k_basic_type)) {
		is_array = false;
		type_node = &parse_tree.get_node(type_node->get_child_index());
	} else if (node_is_type(*type_node, e_parser_nonterminal::k_array_type)) {
		is_array = true;
		type_node = &parse_tree.get_node(type_node->get_child_index());
	} else {
		wl_unreachable();
	}

#if IS_TRUE(ASSERTS_ENABLED)
	if (is_array) {
		const c_lr_parse_tree_node &left_bracket_node = parse_tree.get_node(type_node->get_sibling_index());
		const c_lr_parse_tree_node &right_bracket_node = parse_tree.get_node(left_bracket_node.get_sibling_index());

		wl_assert(node_is_type(left_bracket_node, e_token_type::k_left_bracket));
		wl_assert(node_is_type(right_bracket_node, e_token_type::k_right_bracket));
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	c_ast_data_type result;

	if (node_is_type(*type_node, e_token_type::k_keyword_void)) {
		wl_assert(!is_array);
		result = c_ast_data_type(e_ast_primitive_type::k_void);
	} else if (node_is_type(*type_node, e_token_type::k_keyword_real)) {
		result = c_ast_data_type(e_ast_primitive_type::k_real, is_array);
	} else if (node_is_type(*type_node, e_token_type::k_keyword_bool)) {
		result = c_ast_data_type(e_ast_primitive_type::k_bool, is_array);
	} else if (node_is_type(*type_node, e_token_type::k_keyword_string)) {
		result = c_ast_data_type(e_ast_primitive_type::k_string, is_array);
	} else {
		wl_unreachable();
	}

	return result;
}

static e_ast_qualifier get_qualifier_from_node(const c_lr_parse_tree_node &node) {
	if (node_is_type(node, e_token_type::k_keyword_in)) {
		return e_ast_qualifier::k_in;
	} else if (node_is_type(node, e_token_type::k_keyword_out)) {
		return e_ast_qualifier::k_out;
	} else {
		return e_ast_qualifier::k_count;
		wl_unreachable();
	}
}

static size_t get_first_token_index(const c_lr_parse_tree &parse_tree, const c_lr_parse_tree_node &node) {
	const c_lr_parse_tree_node *node_ptr = &node;
	while (!node_ptr->get_symbol().is_terminal()) {
		node_ptr = &parse_tree.get_node(node_ptr->get_child_index());
	}

	return node_ptr->get_token_index();
}

static std::vector<size_t> build_left_recursive_list(
	const c_lr_parse_tree &parse_tree,
	size_t start_node_index,
	e_parser_nonterminal list_node_type);

static void build_native_module_declarations(c_ast_node_scope *global_scope);

static void add_parse_tree_to_global_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, c_ast_node_scope *global_scope);

static c_ast_node *build_global_scope_item(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &global_scope_item_node);

static c_ast_node_module_declaration *build_module_declaration(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &module_declaration_node);

static c_ast_node_scope *build_scope(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &scope_node,
	const std::vector<c_ast_node *> &initial_children);

static std::vector<c_ast_node *> build_scope_items(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &scope_item_node);

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node);

static c_ast_node_named_value_assignment *build_named_value_assignment(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_assignment_node);

static bool extract_named_value_assignment_lhs_expression(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_lhs_expression_node,
	std::string &out_named_value,
	c_ast_node_expression *&out_array_index_expression);

static c_ast_node_return_statement *build_return_statement(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &return_statement_node);

static std::vector<c_ast_node *> build_repeat_loop(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &return_statement_node);

static c_ast_node_expression *build_expression(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &expression_node);

static c_ast_node_module_call *build_binary_operator_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0);

static c_ast_node_module_call *build_unary_operator_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0);

static c_ast_node_module_call *build_dereference_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0);

static c_ast_node_module_call *build_module_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0);

static c_ast_node_constant *build_constant_array(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0);

c_ast_node *c_ast_builder::build_ast(const s_lexer_output &lexer_output, const s_parser_output &parser_output) {
	// Iterate the parse tree and build up the AST
	// Any valid parse tree should be able to produce an AST (though further analysis of the AST may indicate errors),
	// so this phase should only ever assert on unexpected input, since the parse tree is assumed to be valid.

	c_ast_node_scope *global_scope = new c_ast_node_scope();

	// The very first thing we do is add all native modules
	build_native_module_declarations(global_scope);

	for (size_t parse_tree_index = 0; parse_tree_index < parser_output.source_file_output.size(); parse_tree_index++) {
		add_parse_tree_to_global_scope(
			parser_output.source_file_output[parse_tree_index].parse_tree,
			lexer_output.source_file_output[parse_tree_index],
			global_scope);
	}

	return global_scope;
}

static std::vector<size_t> build_left_recursive_list(
	const c_lr_parse_tree &parse_tree,
	size_t start_node_index,
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
		const s_native_module_library &library = c_native_module_registry::get_native_module_library(
			native_module.uid.get_library_id());

		c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
		module_declaration->set_is_native(true);
		module_declaration->set_native_module_index(index);

		if (c_native_module_registry::get_native_module_operator(native_module.uid) == e_native_operator::k_invalid) {
			// Set the name to "library.module_name"
			std::string name = library.name.get_string();
			name.push_back('.');
			name += native_module.name.get_string();
			module_declaration->set_name(name);
		} else {
			// Set the module name directly with no library prefix because it is used to identify an operator
			module_declaration->set_name(native_module.name.get_string());
		}

		module_declaration->set_return_type(c_ast_data_type(e_ast_primitive_type::k_void));

		bool return_argument_found = false;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			s_native_module_argument argument = native_module.arguments[arg];
			if (arg == native_module.return_argument_index) {
				wl_assert(!return_argument_found);
				wl_assert(argument.type.get_qualifier() == e_native_module_qualifier::k_out);
				// The first out argument is routed through the return value, so don't add a declaration for it
				module_declaration->set_return_type(get_ast_data_type(argument.type.get_data_type()));
				return_argument_found = true;
			} else {
				// Add a declaration for this argument
				c_ast_node_named_value_declaration *argument_declaration = new c_ast_node_named_value_declaration();
				argument_declaration->set_qualifier(get_ast_qualifier(argument.type.get_qualifier()));
				argument_declaration->set_data_type(get_ast_data_type(argument.type.get_data_type()));
				module_declaration->add_argument(argument_declaration);
			}
		}

		global_scope->add_child(module_declaration);
	}
}

static void add_parse_tree_to_global_scope(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	c_ast_node_scope *global_scope) {
	const c_lr_parse_tree_node &root_node = parse_tree.get_node(parse_tree.get_root_node_index());
	wl_assert(node_is_type(root_node, e_parser_nonterminal::k_start));

	const c_lr_parse_tree_node &global_scope_node = parse_tree.get_node(root_node.get_child_index());
	wl_assert(node_is_type(global_scope_node, e_parser_nonterminal::k_global_scope));

	// Iterate through the list of global scope items
	std::vector<size_t> item_indices = build_left_recursive_list(
		parse_tree,
		global_scope_node.get_child_index(),
		e_parser_nonterminal::k_global_scope_item_list);

	for (size_t index = 0; index < item_indices.size(); index++) {
		const c_lr_parse_tree_node &global_scope_item_node = parse_tree.get_node(item_indices[index]);
		wl_assert(node_is_type(global_scope_item_node, e_parser_nonterminal::k_global_scope_item));
		c_ast_node *item = build_global_scope_item(parse_tree, tokens, global_scope_item_node);
		global_scope->add_child(item);
	}
}

static c_ast_node *build_global_scope_item(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &global_scope_item_node) {
	// Determine what type the scope item is
	wl_assert(global_scope_item_node.has_child());
	wl_assert(!global_scope_item_node.has_sibling());

	const c_lr_parse_tree_node &item_type_node = parse_tree.get_node(global_scope_item_node.get_child_index());

	wl_assert(!item_type_node.get_symbol().is_terminal());
	switch (static_cast<e_parser_nonterminal>(item_type_node.get_symbol().get_index())) {
	case e_parser_nonterminal::k_module_declaration:
		return build_module_declaration(parse_tree, tokens, item_type_node);

	default:
		// Invalid type
		wl_unreachable();
		return NULL;
	}
}

static c_ast_node_module_declaration *build_module_declaration(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &module_declaration_node) {
	wl_assert(node_is_type(module_declaration_node, e_parser_nonterminal::k_module_declaration));

	c_lr_parse_tree_iterator it(parse_tree, module_declaration_node.get_child_index());

	const c_lr_parse_tree_node &module_keyword_node = it.get_node();
	wl_assert(node_is_type(module_keyword_node, e_token_type::k_keyword_module));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_return_type_node = it.get_node();
	wl_assert(node_is_type(module_return_type_node, e_parser_nonterminal::k_type_or_void));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_name_node = it.get_node();
	wl_assert(node_is_type(module_name_node, e_token_type::k_identifier));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_left_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &module_declaration_arguments_node = it.get_node();
	wl_assert(node_is_type(module_declaration_arguments_node, e_parser_nonterminal::k_module_declaration_arguments));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_right_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_node = it.get_node();
	wl_assert(node_is_type(scope_node, e_parser_nonterminal::k_scope));
	wl_assert(!it.has_sibling());

	c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
	module_declaration->set_source_location(
		tokens.tokens[module_keyword_node.get_token_index()].source_location);

	c_ast_data_type return_type = get_data_type_from_node(parse_tree, module_return_type_node);

	module_declaration->set_return_type(return_type);
	module_declaration->set_name(tokens.tokens[module_name_node.get_token_index()].token_string.to_std_string());

	std::vector<c_ast_node *> initial_scope_children;

	// Read the arguments, if they exist
	if (module_declaration_arguments_node.has_child()) {
		const c_lr_parse_tree_node &module_declaration_argument_list_node =
			parse_tree.get_node(module_declaration_arguments_node.get_child_index());

		// The argument list node is either an argument declaration, or a list, comma, and declaration
		std::vector<size_t> argument_indices = build_left_recursive_list(
			parse_tree,
			module_declaration_argument_list_node.get_child_index(),
			e_parser_nonterminal::k_module_declaration_argument_list);

		// Skip the commas
		for (size_t index = 0; index < argument_indices.size(); index++) {
			if (index == 0) {
				wl_assert(node_is_type(
					parse_tree.get_node(argument_indices[index]),
					e_parser_nonterminal::k_module_declaration_argument));
			} else {
				wl_assert(node_is_type(
					parse_tree.get_node(argument_indices[index]),
					e_token_type::k_comma));
				argument_indices[index] = parse_tree.get_node(argument_indices[index]).get_sibling_index();
				wl_assert(node_is_type(
					parse_tree.get_node(argument_indices[index]),
					e_parser_nonterminal::k_module_declaration_argument));
			}
		}

		// Process arguments
		for (size_t argument_index = 0; argument_index < argument_indices.size(); argument_index++) {
			c_ast_node_named_value_declaration *argument_declaration = new c_ast_node_named_value_declaration();
			initial_scope_children.push_back(argument_declaration);

			c_lr_parse_tree_iterator arg_it(parse_tree, argument_indices[argument_index]);
			wl_assert(node_is_type(arg_it.get_node(), e_parser_nonterminal::k_module_declaration_argument));
			arg_it.follow_child();

			const c_lr_parse_tree_node &argument_qualifier_node = arg_it.get_node();
			wl_assert(node_is_type(
				argument_qualifier_node,
				e_parser_nonterminal::k_module_declaration_argument_qualifier));
			arg_it.follow_sibling();

			const c_lr_parse_tree_node &argument_type_node = arg_it.get_node();
			wl_assert(node_is_type(argument_type_node, e_parser_nonterminal::k_type));
			arg_it.follow_sibling();

			const c_lr_parse_tree_node &argument_name_node = arg_it.get_node();
			wl_assert(node_is_type(argument_name_node, e_token_type::k_identifier));
			wl_assert(!arg_it.has_sibling());

			wl_assert(argument_qualifier_node.has_child());
			const c_lr_parse_tree_node &qualifier_node = parse_tree.get_node(argument_qualifier_node.get_child_index());
			e_ast_qualifier qualifier = get_qualifier_from_node(qualifier_node);
			c_ast_data_type data_type = get_data_type_from_node(parse_tree, argument_type_node);

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

static c_ast_node_scope *build_scope(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &scope_node,
	const std::vector<c_ast_node *> &initial_children) {
	wl_assert(node_is_type(scope_node, e_parser_nonterminal::k_scope));

	c_lr_parse_tree_iterator it(parse_tree, scope_node.get_child_index());

	wl_assert(node_is_type(it.get_node(), e_token_type::k_left_brace));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_item_list_node = it.get_node();
	wl_assert(node_is_type(it.get_node(), e_parser_nonterminal::k_scope_item_list));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_right_brace));
	wl_assert(!it.has_sibling());

	// Add the initial items first
	c_ast_node_scope *scope = new c_ast_node_scope();
	for (size_t initial_index = 0; initial_index < initial_children.size(); initial_index++) {
		scope->add_child(initial_children[initial_index]);
	}

	// Iterate through the items in the module body and add each one to the module's scope
	std::vector<size_t> item_indices = build_left_recursive_list(
		parse_tree, scope_item_list_node.get_child_index(), e_parser_nonterminal::k_scope_item_list);

	for (size_t index = 0; index < item_indices.size(); index++) {
		const c_lr_parse_tree_node &scope_item_node = parse_tree.get_node(item_indices[index]);
		wl_assert(node_is_type(scope_item_node, e_parser_nonterminal::k_scope_item));
		std::vector<c_ast_node *> items = build_scope_items(parse_tree, tokens, scope_item_node);
		for (size_t item_index = 0; item_index < items.size(); item_index++) {
			scope->add_child(items[item_index]);
		}
	}

	return scope;
}

static std::vector<c_ast_node *> build_scope_items(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &scope_item_node) {
	// Determine what type the scope item is
	wl_assert(scope_item_node.has_child());
	wl_assert(!scope_item_node.has_sibling());

	std::vector<c_ast_node *> result;

	const c_lr_parse_tree_node &item_type_node = parse_tree.get_node(scope_item_node.get_child_index());

	wl_assert(!item_type_node.get_symbol().is_terminal());
	switch (static_cast<e_parser_nonterminal>(item_type_node.get_symbol().get_index())) {
	case e_parser_nonterminal::k_scope:
		result.push_back(build_scope(parse_tree, tokens, item_type_node, std::vector<c_ast_node *>()));
		break;

	case e_parser_nonterminal::k_named_value_declaration:
		result = build_named_value_declaration_and_optional_assignment(parse_tree, tokens, item_type_node);
		break;

	case e_parser_nonterminal::k_named_value_assignment:
		result.push_back(build_named_value_assignment(parse_tree, tokens, item_type_node));
		break;

	case e_parser_nonterminal::k_expression:
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

	case e_parser_nonterminal::k_module_return_statement:
		result.push_back(build_return_statement(parse_tree, tokens, item_type_node));
		break;

	case e_parser_nonterminal::k_repeat_loop:
		result = build_repeat_loop(parse_tree, tokens, item_type_node);
		break;

	default:
		// Invalid type
		wl_unreachable();
	}

	return result;
}

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node) {
	wl_assert(node_is_type(named_value_declaration_node, e_parser_nonterminal::k_named_value_declaration));

	c_lr_parse_tree_iterator it(parse_tree, named_value_declaration_node.get_child_index());

	// type, identifier, and assignment node

	const c_lr_parse_tree_node &named_value_type_node = it.get_node();
	wl_assert(node_is_type(named_value_type_node, e_parser_nonterminal::k_type));
	it.follow_sibling();

	const c_lr_parse_tree_node &name_node = it.get_node();
	wl_assert(node_is_type(name_node, e_token_type::k_identifier));

	c_ast_data_type data_type = get_data_type_from_node(parse_tree, named_value_type_node);

	std::vector<c_ast_node *> result;

	c_ast_node_named_value_declaration *declaration = new c_ast_node_named_value_declaration();
	declaration->set_source_location(
		tokens.tokens[get_first_token_index(parse_tree, named_value_type_node)].source_location);

	declaration->set_data_type(data_type);
	declaration->set_name(tokens.tokens[name_node.get_token_index()].token_string.to_std_string());
	result.push_back(declaration);

	// Read the optional assignment
	if (it.has_sibling()) {
		it.follow_sibling();
		const c_lr_parse_tree_node &expression_assignment_node = it.get_node();
		wl_assert(node_is_type(expression_assignment_node, e_parser_nonterminal::k_expression_assignment));
		wl_assert(!it.has_sibling());
		it.follow_child();

		wl_assert(node_is_type(it.get_node(), e_token_type::k_operator_assignment));
		it.follow_sibling();

		const c_lr_parse_tree_node &expression_node = it.get_node();
		wl_assert(node_is_type(expression_node, e_parser_nonterminal::k_expression));
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

static c_ast_node_named_value_assignment *build_named_value_assignment(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_assignment_node) {
	wl_assert(node_is_type(named_value_assignment_node, e_parser_nonterminal::k_named_value_assignment));

	c_lr_parse_tree_iterator it(parse_tree, named_value_assignment_node.get_child_index());

	const c_lr_parse_tree_node &named_value_expression_node = it.get_node();
	wl_assert(node_is_type(named_value_expression_node, e_parser_nonterminal::k_expression));
	it.follow_sibling();

	c_ast_node_named_value_assignment *result = new c_ast_node_named_value_assignment();
	result->set_source_location(
		tokens.tokens[get_first_token_index(parse_tree, named_value_expression_node)].source_location);

	wl_assert(node_is_type(it.get_node(), e_parser_nonterminal::k_expression_assignment));
	wl_assert(!it.has_sibling());
	it.follow_child();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_operator_assignment));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, e_parser_nonterminal::k_expression));
	wl_assert(!it.has_sibling());

	std::string named_value;
	c_ast_node_expression *array_index_expression;
	bool lhs_expression_is_valid = extract_named_value_assignment_lhs_expression(
		parse_tree, tokens, named_value_expression_node, named_value, array_index_expression);

	result->set_is_valid_named_value(lhs_expression_is_valid);
	if (lhs_expression_is_valid) {
		result->set_named_value(named_value);
		if (array_index_expression) {
			result->set_array_index_expression(array_index_expression);
		}
	}

	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);
	result->set_expression(expression);

	return result;
}

static bool extract_named_value_assignment_lhs_expression(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_lhs_expression_node,
	std::string &out_named_value,
	c_ast_node_expression *&out_array_index_expression) {
	// Manually parse the LHS expression, looking for a specific pattern. Gross!
	wl_assert(node_is_type(named_value_lhs_expression_node, e_parser_nonterminal::k_expression));

	c_lr_parse_tree_iterator it(parse_tree, named_value_lhs_expression_node.get_child_index());
	wl_assert(node_is_type(it.get_node(), e_parser_nonterminal::k_expr_1));

	// Iterate down to expr_8
	static const e_parser_nonterminal k_expression_chain[] = {
		e_parser_nonterminal::k_expr_2, e_parser_nonterminal::k_expr_3, e_parser_nonterminal::k_expr_4,
		e_parser_nonterminal::k_expr_5, e_parser_nonterminal::k_expr_6, e_parser_nonterminal::k_expr_7,
		e_parser_nonterminal::k_expr_8
	};

	for (size_t iter = 0; iter < NUMBEROF(k_expression_chain); iter++) {
		wl_assert(it.has_child());
		it.follow_child();

		if (!node_is_type(it.get_node(), k_expression_chain[iter])) {
			return false;
		}
	}

	wl_assert(node_is_type(it.get_node(), e_parser_nonterminal::k_expr_8));
	wl_assert(it.has_child());
	it.follow_child();

	const c_lr_parse_tree_node *array_index_expression_node = nullptr;

	// Check for array dereference
	if (node_is_type(it.get_node(), e_parser_nonterminal::k_expr_8)) {
		c_lr_parse_tree_iterator deref_it(parse_tree, it.get_node_index());
		it.follow_child();
		deref_it.follow_sibling();
		wl_assert(node_is_type(deref_it.get_node(), e_parser_nonterminal::k_array_dereference));
		deref_it.follow_child();

		wl_assert(node_is_type(deref_it.get_node(), e_token_type::k_left_bracket));
		deref_it.follow_sibling();

		array_index_expression_node = &deref_it.get_node();
		wl_assert(node_is_type(*array_index_expression_node, e_parser_nonterminal::k_expression));
		deref_it.follow_sibling();

		wl_assert(node_is_type(deref_it.get_node(), e_token_type::k_right_bracket));
		wl_assert(!deref_it.has_sibling());
	}

	// We should be at expr_9 now, unless there's a double array dereference, which is invalid
	if (!node_is_type(it.get_node(), e_parser_nonterminal::k_expr_9)) {
		return false;
	}

	// We expect an identifier
	wl_assert(it.has_child());
	it.follow_child();

	if (!node_is_type(it.get_node(), e_token_type::k_identifier)) {
		return false;
	}

	// Success
	if (array_index_expression_node) {
		out_array_index_expression = build_expression(parse_tree, tokens, *array_index_expression_node);
	} else {
		out_array_index_expression = nullptr;
	}

	out_named_value = tokens.tokens[it.get_node().get_token_index()].token_string.to_std_string();
	return true;
}

static c_ast_node_return_statement *build_return_statement(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &return_statement_node) {
	wl_assert(node_is_type(return_statement_node, e_parser_nonterminal::k_module_return_statement));

	c_lr_parse_tree_iterator it(parse_tree, return_statement_node.get_child_index());

	const c_lr_parse_tree_node &return_node = it.get_node();
	wl_assert(node_is_type(return_node, e_token_type::k_keyword_return));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, e_parser_nonterminal::k_expression));
	wl_assert(!it.has_sibling());

	c_ast_node_return_statement *result = new c_ast_node_return_statement();
	result->set_source_location(tokens.tokens[return_node.get_token_index()].source_location);

	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);
	result->set_expression(expression);

	return result;
}

static std::vector<c_ast_node *> build_repeat_loop(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &return_statement_node) {
	wl_assert(node_is_type(return_statement_node, e_parser_nonterminal::k_repeat_loop));

	c_lr_parse_tree_iterator it(parse_tree, return_statement_node.get_child_index());

	const c_lr_parse_tree_node &repeat_node = it.get_node();
	wl_assert(node_is_type(repeat_node, e_token_type::k_keyword_repeat));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_left_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &expression_node = it.get_node();
	wl_assert(node_is_type(expression_node, e_parser_nonterminal::k_expression));
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_right_parenthesis));
	it.follow_sibling();

	const c_lr_parse_tree_node &scope_node = it.get_node();
	wl_assert(node_is_type(scope_node, e_parser_nonterminal::k_scope));
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

static c_ast_node_expression *build_expression(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &expression_node) {
	// Result is always an expression
	c_ast_node_expression *result = new c_ast_node_expression();

	// We may need to move through several layers of child nodes before we find the actual expression type
	bool expression_type_found = false;
	const c_lr_parse_tree_node *current_node = &expression_node;
	while (!expression_type_found) {
		if (node_is_type(*current_node, e_parser_nonterminal::k_expression)) {
			// expression:

			// Always turns into expr_1
			current_node = &parse_tree.get_node(current_node->get_child_index());
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_1)) {
			// expr_1: ||

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_2
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_2)) {
			// expr_2: &&

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_3
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_3)) {
			// expr_3: == !=

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_4
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_4)) {
			// expr_4: > < >= <=

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_5
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_5)) {
			// expr_5: + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_6
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_6)) {
			// expr_6: * / %

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_7
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_7)) {
			// expr_7: unary + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_8
				current_node = &child_node;
			} else {
				result->set_expression_value(build_unary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_8)) {
			// expr_8: array dereference

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_9
				current_node = &child_node;
			} else {
				result->set_expression_value(build_dereference_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, e_parser_nonterminal::k_expr_9)) {
			// expr_9: parens, identifier, module_call, constant

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (node_is_type(child_node, e_token_type::k_left_parenthesis)) {
				// We found an expression in parenthesis whose ordering was already resolved by the parser, forward it
				// back to the top level (expression)
				current_node = &parse_tree.get_node(child_node.get_sibling_index());
				wl_assert(node_is_type(
					parse_tree.get_node(current_node->get_sibling_index()),
					e_token_type::k_right_parenthesis));
			} else if (node_is_type(child_node, e_token_type::k_identifier)) {
				// Named value
				c_ast_node_named_value *named_value = new c_ast_node_named_value();
				named_value->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				named_value->set_name(tokens.tokens[child_node.get_token_index()].token_string.to_std_string());
				result->set_expression_value(named_value);
				expression_type_found = true;
			} else if (node_is_type(child_node, e_parser_nonterminal::k_module_call)) {
				// Module call
				result->set_expression_value(build_module_call(
					parse_tree, tokens, parse_tree.get_node(child_node.get_child_index())));
				expression_type_found = true;
			} else if (node_is_type(child_node, e_token_type::k_constant_real)) {
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
			} else if (node_is_type(child_node, e_token_type::k_constant_bool)) {
				// This is a bool
				std::string string_value = tokens.tokens[child_node.get_token_index()].token_string.to_std_string();
				c_ast_node_constant *constant_node = new c_ast_node_constant();
				constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				wl_vassert(
					string_value == k_token_type_constant_bool_false_string ||
					string_value == k_token_type_constant_bool_true_string,
					"We should have already caught this in the lexer");
				bool bool_value = (string_value == k_token_type_constant_bool_true_string);
				constant_node->set_bool_value(bool_value);
				result->set_expression_value(constant_node);
				expression_type_found = true;
			} else if (node_is_type(child_node, e_token_type::k_constant_string)) {
				// This is a string
				c_compiler_string unescaped_string = tokens.tokens[child_node.get_token_index()].token_string;
				// We should always successfully escape the string because the lexer will have failed if any of the
				// escape sequences were invalid.
				std::string escaped_string = compiler_utility::escape_string(unescaped_string);

				c_ast_node_constant *constant_node = new c_ast_node_constant();
				constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
				constant_node->set_string_value(escaped_string);
				result->set_expression_value(constant_node);
				expression_type_found = true;
			} else if (node_is_type(child_node, e_parser_nonterminal::k_constant_array)) {
				// Constant array
				result->set_expression_value(build_constant_array(
					parse_tree, tokens, parse_tree.get_node(child_node.get_child_index())));
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

static c_ast_node_module_call *build_binary_operator_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0) {
	// There should be three nodes of the form "expr operator expr"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());
	const c_lr_parse_tree_node &node_2 = parse_tree.get_node(node_1.get_sibling_index());

	const char *operator_module_name = nullptr;

	// Choose the built-in name of the operator module
	wl_assert(node_1.get_symbol().is_terminal());
	switch (static_cast<e_token_type>(node_1.get_symbol().get_index())) {
	case e_token_type::k_operator_addition:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_addition);
		break;

	case e_token_type::k_operator_subtraction:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_subtraction);
		break;

	case e_token_type::k_operator_multiplication:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_multiplication);
		break;

	case e_token_type::k_operator_division:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_division);
		break;

	case e_token_type::k_operator_modulo:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_modulo);
		break;

	case e_token_type::k_operator_equal:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_equal);
		break;

	case e_token_type::k_operator_not_equal:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_not_equal);
		break;

	case e_token_type::k_operator_greater:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_greater);
		break;

	case e_token_type::k_operator_less:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_less);
		break;

	case e_token_type::k_operator_greater_equal:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_greater_equal);
		break;

	case e_token_type::k_operator_less_equal:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_less_equal);
		break;

	case e_token_type::k_operator_and:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_and);
		break;

	case e_token_type::k_operator_or:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_or);
		break;

	default:
		wl_unreachable();
	}

	c_ast_node_expression *arg_0 = build_expression(parse_tree, tokens, node_0);
	c_ast_node_expression *arg_1 = build_expression(parse_tree, tokens, node_2);

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_1.get_token_index()].source_location);
	module_call->set_is_invoked_using_operator(true);
	module_call->set_name(operator_module_name);
	module_call->add_argument(arg_0);
	module_call->add_argument(arg_1);

	return module_call;
}

static c_ast_node_module_call *build_unary_operator_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0) {
	// There should be two nodes of the form "operator expr"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());

	const char *operator_module_name = nullptr;

	// Choose the built-in name of the operator module
	wl_assert(node_0.get_symbol().is_terminal());
	switch (static_cast<e_token_type>(node_0.get_symbol().get_index())) {
	case e_token_type::k_operator_addition:
		// +x just returns x, and will be optimized away
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_noop);
		break;

	case e_token_type::k_operator_subtraction:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_negation);
		break;

	case e_token_type::k_operator_not:
		operator_module_name =
			c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_not);
		break;

	default:
		wl_unreachable();
	}

	c_ast_node_expression *arg_0 = build_expression(parse_tree, tokens, node_1);

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_0.get_token_index()].source_location);
	module_call->set_is_invoked_using_operator(true);
	module_call->set_name(operator_module_name);
	module_call->add_argument(arg_0);

	return module_call;
}

static c_ast_node_module_call *build_dereference_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0) {
	// There should be four nodes of the form "expr [ expr ]"
	c_lr_parse_tree_iterator it(parse_tree, node_0.get_sibling_index());
	wl_assert(node_is_type(it.get_node(), e_parser_nonterminal::k_array_dereference));
	wl_assert(!it.has_sibling());
	it.follow_child();

	const c_lr_parse_tree_node &left_bracket_node = it.get_node();
	wl_assert(node_is_type(left_bracket_node, e_token_type::k_left_bracket));
	it.follow_sibling();

	const c_lr_parse_tree_node &array_index_expression_node = it.get_node();
	it.follow_sibling();

	wl_assert(node_is_type(it.get_node(), e_token_type::k_right_bracket));
	wl_assert(!it.has_sibling());

	c_ast_node_expression *array_arg = build_expression(parse_tree, tokens, node_0);
	c_ast_node_expression *index_arg = build_expression(parse_tree, tokens, array_index_expression_node);

	const char *operator_module_name =
		c_native_module_registry::get_native_module_for_native_operator(e_native_operator::k_array_dereference);

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[left_bracket_node.get_token_index()].source_location);
	module_call->set_is_invoked_using_operator(true);
	module_call->set_name(operator_module_name);
	module_call->add_argument(array_arg);
	module_call->add_argument(index_arg);

	return module_call;
}

static c_ast_node_module_call *build_module_call(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0) {
	// We expect "identifier ( optional_args )"
	const c_lr_parse_tree_node &node_1 = parse_tree.get_node(node_0.get_sibling_index());
	const c_lr_parse_tree_node &node_2 = parse_tree.get_node(node_1.get_sibling_index());

	wl_assert(node_is_type(node_0, e_token_type::k_identifier));
	wl_assert(node_is_type(node_1, e_token_type::k_left_parenthesis));

	c_ast_node_module_call *module_call = new c_ast_node_module_call();
	module_call->set_source_location(tokens.tokens[node_0.get_token_index()].source_location);
	module_call->set_name(tokens.tokens[node_0.get_token_index()].token_string.to_std_string());

	if (node_is_type(node_2, e_token_type::k_right_parenthesis)) {
		// No arguments
	} else {
		wl_assert(node_is_type(node_2, e_parser_nonterminal::k_module_call_argument_list));
		wl_assert(node_is_type(
			parse_tree.get_node(node_2.get_sibling_index()),
			e_token_type::k_right_parenthesis));

		// The argument list node is either an expression, or a list, comma, and expression
		std::vector<size_t> argument_indices = build_left_recursive_list(
			parse_tree, node_2.get_child_index(), e_parser_nonterminal::k_module_call_argument_list);

		// Skip the commas
		for (size_t index = 0; index < argument_indices.size(); index++) {
			if (index == 0) {
				wl_assert(
					node_is_type(parse_tree.get_node(argument_indices[index]), e_parser_nonterminal::k_expression));
			} else {
				wl_assert(node_is_type(parse_tree.get_node(argument_indices[index]), e_token_type::k_comma));
				argument_indices[index] = parse_tree.get_node(argument_indices[index]).get_sibling_index();
				wl_assert(
					node_is_type(parse_tree.get_node(argument_indices[index]), e_parser_nonterminal::k_expression));
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

static c_ast_node_constant *build_constant_array(
	const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &node_0) {
	c_lr_parse_tree_iterator it(parse_tree, node_0.get_sibling_index());

	c_ast_node_constant *constant_array = new c_ast_node_constant();
	constant_array->set_source_location(tokens.tokens[get_first_token_index(parse_tree, node_0)].source_location);

	// We expect "array ( optional_expressions )"
	c_ast_data_type data_type = get_data_type_from_node(parse_tree, node_0);

	// Can't have arrays of arrays
	wl_assert(data_type.is_array());
	constant_array->set_array(data_type.get_element_type());

	wl_assert(node_is_type(it.get_node(), e_token_type::k_left_parenthesis));
	it.follow_sibling();

	if (node_is_type(it.get_node(), e_token_type::k_right_parenthesis)) {
		wl_assert(!it.has_sibling());
		// No values
	} else {
		const c_lr_parse_tree_node &constant_array_list_node = it.get_node();
		wl_assert(node_is_type(constant_array_list_node, e_parser_nonterminal::k_constant_array_list));
		it.follow_sibling();

		wl_assert(node_is_type(it.get_node(), e_token_type::k_right_parenthesis));
		wl_assert(!it.has_sibling());

		// The value list node is either an expression, or a list, comma, and expression
		std::vector<size_t> value_indices = build_left_recursive_list(
			parse_tree, constant_array_list_node.get_child_index(), e_parser_nonterminal::k_constant_array_list);

		// Skip the commas
		for (size_t index = 0; index < value_indices.size(); index++) {
			if (index == 0) {
				wl_assert(node_is_type(parse_tree.get_node(value_indices[index]), e_parser_nonterminal::k_expression));
			} else {
				wl_assert(node_is_type(parse_tree.get_node(value_indices[index]), e_token_type::k_comma));
				value_indices[index] = parse_tree.get_node(value_indices[index]).get_sibling_index();
				wl_assert(node_is_type(parse_tree.get_node(value_indices[index]), e_parser_nonterminal::k_expression));
			}
		}

		// Process arguments
		for (size_t value_index = 0; value_index < value_indices.size(); value_index++) {
			c_ast_node_expression *expression =
				build_expression(parse_tree, tokens, parse_tree.get_node(value_indices[value_index]));

			constant_array->add_array_value(expression);
		}
	}

	return constant_array;
}