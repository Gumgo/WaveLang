#include "compiler/ast_builder.h"
#include "compiler/ast.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "execution_graph/native_modules.h"
#include <stdexcept>

static bool node_is_type(const c_lr_parse_tree_node &node, e_token_type terminal_type) {
	return node.get_symbol().is_terminal() && node.get_symbol().get_index() == terminal_type;
}

static bool node_is_type(const c_lr_parse_tree_node &node, e_parser_nonterminal nonterminal_type) {
	return !node.get_symbol().is_terminal() && node.get_symbol().get_index() == nonterminal_type;
}

static void build_native_module_declarations(c_ast_node_scope *global_scope);

static void add_parse_tree_to_global_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, c_ast_node_scope *global_scope);

static std::vector<c_ast_node *> build_scope_items(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_item_node,
	bool module_declarations_allowed,
	bool named_value_declarations_allowed,
	bool named_value_assignments_allowed,
	bool expressions_allowed,
	bool return_statements_allowed);

static c_ast_node_module_declaration *build_module_declaration(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &module_declaration_node);

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node);

static c_ast_node_named_value_assignment *build_named_value_assignment(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &named_value_assignment_node);

static c_ast_node_return_statement *build_return_statement(const c_lr_parse_tree &parse_tree,
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

static void build_native_module_declarations(c_ast_node_scope *global_scope) {
	for (uint32 index = 0; index < c_native_module_registry::get_native_module_count(); index++) {
		const s_native_module &native_module = c_native_module_registry::get_native_module(index);

		c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
		module_declaration->set_is_native(true);
		module_declaration->set_name(native_module.name);
		module_declaration->set_has_return_value(native_module.first_output_is_return);

		bool first_out_argument_found = false;
		for (size_t arg = 0; arg < native_module.argument_count; arg++) {
			e_native_module_argument_type arg_type = native_module.argument_types[arg];
			if (native_module.first_output_is_return &&
				arg_type == k_native_module_argument_type_out &&
				!first_out_argument_found) {
				// The first out argument is routed through the return value, so don't add a declaration for it
				first_out_argument_found = true;
			} else {
				// Add a declaration for this argument
				c_ast_node_named_value_declaration *argument = new c_ast_node_named_value_declaration();
				wl_assert(arg_type == k_native_module_argument_type_in ||
					arg_type == k_native_module_argument_type_out ||
					arg_type == k_native_module_argument_type_constant);
				argument->set_qualifier((arg_type == k_native_module_argument_type_out) ?
					c_ast_node_named_value_declaration::k_qualifier_out :
					c_ast_node_named_value_declaration::k_qualifier_in);

				module_declaration->add_argument(argument);
			}
		}

		global_scope->add_child(module_declaration);
	}
}

static void add_parse_tree_to_global_scope(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, c_ast_node_scope *global_scope) {
	const c_lr_parse_tree_node &root_node = parse_tree.get_node(parse_tree.get_root_node_index());
	wl_assert(node_is_type(root_node, k_parser_nonterminal_start));

	// Iterate through the list of global scope items
	// Due to the left-recursive rule, we end up visiting the actual items themselves in reverse, so accumulate and flip
	// the order
	std::vector<size_t> item_indices;
	size_t global_scope_index = root_node.get_child_index();
	while (global_scope_index != c_lr_parse_tree::k_invalid_index) {
		const c_lr_parse_tree_node &global_scope_node = parse_tree.get_node(global_scope_index);
		wl_assert(node_is_type(global_scope_node, k_parser_nonterminal_global_scope));

		if (global_scope_node.has_sibling()) {
			item_indices.push_back(global_scope_node.get_sibling_index());
		}

		// The first child is another global scope node, or invalid
		global_scope_index = global_scope_node.get_child_index();
	}

	// Visit in reverse
	for (size_t index = 0; index < item_indices.size(); index++) {
		size_t reverse_index = item_indices.size() - index - 1;

		const c_lr_parse_tree_node &global_scope_item_node = parse_tree.get_node(item_indices[reverse_index]);
		wl_assert(node_is_type(global_scope_item_node, k_parser_nonterminal_global_scope_item));
		std::vector<c_ast_node *> items = build_scope_items(parse_tree, tokens, global_scope_item_node,
			true, false, false, false, false);
		for (size_t item_index = 0; item_index < items.size(); item_index++) {
			global_scope->add_child(items[item_index]);
		}
	}
}

static std::vector<c_ast_node *> build_scope_items(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &scope_item_node,
	bool module_declarations_allowed,
	bool named_value_declarations_allowed,
	bool named_value_assignments_allowed,
	bool expressions_allowed,
	bool return_statements_allowed) {
	// Determine what type the scope item is
	wl_assert(scope_item_node.has_child());
	wl_assert(!scope_item_node.has_sibling());

	std::vector<c_ast_node *> result;

	const c_lr_parse_tree_node &item_type_node = parse_tree.get_node(scope_item_node.get_child_index());

	wl_assert(!item_type_node.get_symbol().is_terminal());
	switch (item_type_node.get_symbol().get_index()) {
	case k_parser_nonterminal_module_declaration:
		wl_assert(module_declarations_allowed);
		result.push_back(build_module_declaration(parse_tree, tokens, item_type_node));
		break;

	case k_parser_nonterminal_named_value_declaration:
		wl_assert(named_value_declarations_allowed);
		result = build_named_value_declaration_and_optional_assignment(parse_tree, tokens, item_type_node);
		break;

	case k_parser_nonterminal_named_value_assignment:
		wl_assert(named_value_assignments_allowed);
		result.push_back(build_named_value_assignment(parse_tree, tokens, item_type_node));
		break;

	case k_parser_nonterminal_expression:
		wl_assert(expressions_allowed);
		{
			// In this case, we wrap the expression in a named value assignment, but don't fill in the named value being
			// assigned to. This is because we assume that the assignments are from output parameters in a module call.
			// Technically this means we can allow named value assignment nodes with no actual assignment, but it's
			// pointless to do this, and will get optimized away later.
			c_ast_node_named_value_assignment *named_value_assignment = new c_ast_node_named_value_assignment();
			c_ast_node_expression *expression = build_expression(parse_tree, tokens, item_type_node);
			named_value_assignment->set_expression(expression);
			result.push_back(named_value_assignment);
		}
		break;

	case k_parser_nonterminal_module_return_statement:
		wl_assert(return_statements_allowed);
		result.push_back(build_return_statement(parse_tree, tokens, item_type_node));
		break;

	default:
		// Invalid type
		wl_unreachable();
	}

	return result;
}

static c_ast_node_module_declaration *build_module_declaration(const c_lr_parse_tree &parse_tree,
	const s_lexer_source_file_output &tokens, const c_lr_parse_tree_node &module_declaration_node) {
	wl_assert(node_is_type(module_declaration_node, k_parser_nonterminal_module_declaration));

	const c_lr_parse_tree_node &module_keyword_node =
		parse_tree.get_node(module_declaration_node.get_child_index());
	const c_lr_parse_tree_node &module_declaration_return_node =
		parse_tree.get_node(module_keyword_node.get_sibling_index());
	const c_lr_parse_tree_node &module_name_node =
		parse_tree.get_node(module_declaration_return_node.get_sibling_index());
	const c_lr_parse_tree_node &left_paren_node =
		parse_tree.get_node(module_name_node.get_sibling_index());
	const c_lr_parse_tree_node &module_declaration_arguments_node =
		parse_tree.get_node(left_paren_node.get_sibling_index());
	const c_lr_parse_tree_node &right_paren_node =
		parse_tree.get_node(module_declaration_arguments_node.get_sibling_index());
	const c_lr_parse_tree_node &left_brace_node =
		parse_tree.get_node(right_paren_node.get_sibling_index());
	const c_lr_parse_tree_node &module_body_item_list_node =
		parse_tree.get_node(left_brace_node.get_sibling_index());
	const c_lr_parse_tree_node &right_brace_node =
		parse_tree.get_node(module_body_item_list_node.get_sibling_index());

	// Assert all the correct types
	wl_assert(node_is_type(module_keyword_node, k_token_type_keyword_module));
	wl_assert(node_is_type(module_declaration_return_node, k_parser_nonterminal_module_declaration_return));
	wl_assert(node_is_type(module_name_node, k_token_type_identifier));
	wl_assert(node_is_type(left_paren_node, k_token_type_left_parenthesis));
	wl_assert(node_is_type(module_declaration_arguments_node, k_parser_nonterminal_module_declaration_arguments));
	wl_assert(node_is_type(right_paren_node, k_token_type_right_parenthesis));
	wl_assert(node_is_type(left_brace_node, k_token_type_left_brace));
	wl_assert(node_is_type(module_body_item_list_node, k_parser_nonterminal_module_body_item_list));
	wl_assert(node_is_type(right_brace_node, k_token_type_right_brace));

	c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
	module_declaration->set_source_location(
		tokens.tokens[module_keyword_node.get_token_index()].source_location);

	// Set return value true or false
	bool has_return = module_declaration_return_node.has_child();
	if (has_return) {
		// There should either be an "out" keyword, or nothing
		wl_assert(node_is_type(parse_tree.get_node(module_declaration_return_node.get_child_index()),
			k_token_type_keyword_out));
	}

	module_declaration->set_has_return_value(has_return);
	module_declaration->set_name(tokens.tokens[module_name_node.get_token_index()].token_string.to_std_string());

	// Create the module body scope before we read the arguments. This is because the arguments get included as body
	// scope items.
	c_ast_node_scope *module_body_scope = new c_ast_node_scope();
	module_declaration->set_scope(module_body_scope);

	// Read the arguments, if they exist
	if (module_declaration_arguments_node.has_child()) {
		const c_lr_parse_tree_node &module_declaration_argument_list_node =
			parse_tree.get_node(module_declaration_arguments_node.get_child_index());

		// The argument list node is either an argument declaration, or a list, comma, and declaration
		// Accumulate arguments and read in reverse due to left-recursivity
		std::vector<size_t> argument_indices;

		size_t child_index = module_declaration_argument_list_node.get_child_index();
		while (child_index != c_lr_parse_tree::k_invalid_index) {
			const c_lr_parse_tree_node &child_node = parse_tree.get_node(child_index);

			if (!child_node.has_sibling()) {
				wl_assert(node_is_type(child_node, k_parser_nonterminal_module_declaration_argument));
				argument_indices.push_back(child_index);
				child_index = c_lr_parse_tree::k_invalid_index;
			} else {
				// Skip the comma and read the next node for the argument
				const c_lr_parse_tree_node &comma_node = parse_tree.get_node(child_node.get_sibling_index());
				wl_assert(node_is_type(comma_node, k_token_type_comma));
				size_t argument_index = comma_node.get_sibling_index();
				wl_assert(node_is_type(parse_tree.get_node(argument_index),
					k_parser_nonterminal_module_declaration_argument));
				argument_indices.push_back(argument_index);

				wl_assert(child_node.has_child());
				child_index = child_node.get_child_index();
			}
		}

		// Process arguments (in reverse)
		for (size_t argument_index = 0; argument_index < argument_indices.size(); argument_index++) {
			size_t reverse_argument_index = argument_indices.size() - argument_index - 1;

			c_ast_node_named_value_declaration *argument_declaration = new c_ast_node_named_value_declaration();

			const c_lr_parse_tree_node &module_declaration_argument_node =
				parse_tree.get_node(argument_indices[reverse_argument_index]);
			wl_assert(node_is_type(module_declaration_argument_node, k_parser_nonterminal_module_declaration_argument));
			wl_assert(module_declaration_argument_node.has_child());

			const c_lr_parse_tree_node &module_declaration_argument_qualifier_node =
				parse_tree.get_node(module_declaration_argument_node.get_child_index());
			const c_lr_parse_tree_node &keyword_val_node =
				parse_tree.get_node(module_declaration_argument_qualifier_node.get_sibling_index());
			const c_lr_parse_tree_node &argument_name_node =
				parse_tree.get_node(keyword_val_node.get_sibling_index());

			// Make sure nodes are the right type
			wl_assert(node_is_type(module_declaration_argument_qualifier_node,
				k_parser_nonterminal_module_declaration_argument_qualifier));
			wl_assert(node_is_type(keyword_val_node,
				k_token_type_keyword_val));
			wl_assert(node_is_type(argument_name_node,
				k_token_type_identifier));

			wl_assert(module_declaration_argument_qualifier_node.has_child());
			const c_lr_parse_tree_node &qualifier_node =
				parse_tree.get_node(module_declaration_argument_qualifier_node.get_child_index());

			argument_declaration->set_source_location(
				tokens.tokens[qualifier_node.get_token_index()].source_location);

			if (node_is_type(qualifier_node, k_token_type_keyword_in)) {
				argument_declaration->set_qualifier(c_ast_node_named_value_declaration::k_qualifier_in);
			} else if (node_is_type(qualifier_node, k_token_type_keyword_out)) {
				argument_declaration->set_qualifier(c_ast_node_named_value_declaration::k_qualifier_out);
			} else {
				wl_unreachable();
			}

			argument_declaration->set_name(
				tokens.tokens[argument_name_node.get_token_index()].token_string.to_std_string());

			module_declaration->add_argument(argument_declaration);
			module_body_scope->add_child(argument_declaration);
		}
	}

	// Iterate through the items in the module body and add each one to the module's scope

	// Due to the left-recursive rule, we end up visiting the actual items themselves in reverse, so accumulate and flip
	// the order
	std::vector<size_t> item_indices;
	size_t module_body_item_list_index = module_body_item_list_node.get_child_index();
	while (module_body_item_list_index != c_lr_parse_tree::k_invalid_index) {
		const c_lr_parse_tree_node &item_list_node = parse_tree.get_node(module_body_item_list_index);
		wl_assert(node_is_type(item_list_node, k_parser_nonterminal_module_body_item_list));

		if (item_list_node.has_sibling()) {
			item_indices.push_back(item_list_node .get_sibling_index());
		}

		// The first child is another item list, or invalid
		module_body_item_list_index = item_list_node.get_child_index();
	}

	// Visit in reverse
	for (size_t index = 0; index < item_indices.size(); index++) {
		size_t reverse_index = item_indices.size() - index - 1;

		const c_lr_parse_tree_node &module_body_item_node = parse_tree.get_node(item_indices[reverse_index]);
		wl_assert(node_is_type(module_body_item_node, k_parser_nonterminal_module_body_item));
		std::vector<c_ast_node *> items = build_scope_items(parse_tree, tokens, module_body_item_node,
			false, true, true, true, true);
		for (size_t item_index = 0; item_index < items.size(); item_index++) {
			module_body_scope->add_child(items[item_index]);
		}
	}

	return module_declaration;
}

static std::vector<c_ast_node *> build_named_value_declaration_and_optional_assignment(
	const c_lr_parse_tree &parse_tree, const s_lexer_source_file_output &tokens,
	const c_lr_parse_tree_node &named_value_declaration_node) {
	wl_assert(node_is_type(named_value_declaration_node, k_parser_nonterminal_named_value_declaration));

	// "val", identifier, and assignment node
	const c_lr_parse_tree_node &val_node =
		parse_tree.get_node(named_value_declaration_node.get_child_index());
	const c_lr_parse_tree_node &name_node =
		parse_tree.get_node(val_node.get_sibling_index());

	wl_assert(node_is_type(val_node, k_token_type_keyword_val));
	wl_assert(node_is_type(name_node, k_token_type_identifier));

	std::vector<c_ast_node *> result;

	c_ast_node_named_value_declaration *declaration = new c_ast_node_named_value_declaration();
	declaration->set_source_location(tokens.tokens[val_node.get_token_index()].source_location);

	declaration->set_name(tokens.tokens[name_node.get_token_index()].token_string.to_std_string());
	result.push_back(declaration);

	// Read the optional assignment
	if (name_node.has_sibling()) {
		const c_lr_parse_tree_node &expression_assignment_node =
			parse_tree.get_node(name_node.get_sibling_index());
		wl_assert(node_is_type(expression_assignment_node, k_parser_nonterminal_expression_assignment));

		const c_lr_parse_tree_node &assignment_operator_node =
			parse_tree.get_node(expression_assignment_node.get_child_index());
		const c_lr_parse_tree_node &expression_node =
			parse_tree.get_node(assignment_operator_node.get_sibling_index());

		wl_assert(node_is_type(assignment_operator_node, k_token_type_operator_assignment));
		wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));

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

	const c_lr_parse_tree_node &name_node =
		parse_tree.get_node(named_value_assignment_node.get_child_index());
	const c_lr_parse_tree_node &expression_assignment_node =
		parse_tree.get_node(name_node.get_sibling_index());
	const c_lr_parse_tree_node &assignment_node =
		parse_tree.get_node(expression_assignment_node.get_child_index());
	const c_lr_parse_tree_node &expression_node =
		parse_tree.get_node(assignment_node.get_sibling_index());

	wl_assert(node_is_type(name_node, k_token_type_identifier));
	wl_assert(node_is_type(expression_assignment_node, k_parser_nonterminal_expression_assignment));
	wl_assert(node_is_type(assignment_node, k_token_type_operator_assignment));
	wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));

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

	const c_lr_parse_tree_node &return_node =
		parse_tree.get_node(return_statement_node.get_child_index());
	const c_lr_parse_tree_node &expression_node =
		parse_tree.get_node(return_node.get_sibling_index());

	wl_assert(node_is_type(return_node, k_token_type_keyword_return));
	wl_assert(node_is_type(expression_node, k_parser_nonterminal_expression));

	c_ast_node_return_statement *result = new c_ast_node_return_statement();
	result->set_source_location(tokens.tokens[return_node.get_token_index()].source_location);

	c_ast_node_expression *expression = build_expression(parse_tree, tokens, expression_node);
	result->set_expression(expression);

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
			// expr_1: + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_2
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_2)) {
			// expr_2: * / %

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_3
				current_node = &child_node;
			} else {
				result->set_expression_value(build_binary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_3)) {
			// expr_3: unary + -

			const c_lr_parse_tree_node &child_node = parse_tree.get_node(current_node->get_child_index());
			if (!child_node.has_sibling()) {
				// Move to expr_4
				current_node = &child_node;
			} else {
				result->set_expression_value(build_unary_operator_call(parse_tree, tokens, child_node));
				expression_type_found = true;
			}
		} else if (node_is_type(*current_node, k_parser_nonterminal_expr_4)) {
			// expr_4: parens, identifier, module_call, constant

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
			} else if (node_is_type(child_node, k_token_type_constant)) {
				try {
					// This is a constant
					std::string string_value = tokens.tokens[child_node.get_token_index()].token_string.to_std_string();
					c_ast_node_constant *constant_node = new c_ast_node_constant();
					constant_node->set_source_location(tokens.tokens[child_node.get_token_index()].source_location);
					constant_node->set_value(std::stof(string_value));
					result->set_expression_value(constant_node);
					expression_type_found = true;
				} catch (const std::invalid_argument &) {
					wl_vhalt("We should have already caught this in the lexer");
				} catch (const std::out_of_range &) {
					wl_vhalt("We should have already caught this in the lexer");
				}
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
		operator_module_name = c_native_module_registry::k_operator_noop_name; // +x just returns x, and will be optimized away
		break;

	case k_token_type_operator_subtraction:
		operator_module_name = c_native_module_registry::k_operator_negation_name;
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

		// The argument list node is either an argument declaration, or a list, comma, and declaration
		// Accumulate arguments and read in reverse due to left-recursivity
		std::vector<size_t> argument_indices;

		size_t child_index = node_2.get_child_index();
		while (child_index != c_lr_parse_tree::k_invalid_index) {
			const c_lr_parse_tree_node &child_node = parse_tree.get_node(child_index);

			if (!child_node.has_sibling()) {
				wl_assert(node_is_type(child_node, k_parser_nonterminal_expression));
				argument_indices.push_back(child_index);
				child_index = c_lr_parse_tree::k_invalid_index;
			} else {
				// Skip the comma and read the next node for the argument
				const c_lr_parse_tree_node &comma_node = parse_tree.get_node(child_node.get_sibling_index());
				wl_assert(node_is_type(comma_node, k_token_type_comma));
				size_t argument_index = comma_node.get_sibling_index();
				wl_assert(node_is_type(parse_tree.get_node(argument_index),
					k_parser_nonterminal_expression));
				argument_indices.push_back(argument_index);

				wl_assert(child_node.has_child());
				child_index = child_node.get_child_index();
			}
		}

		// Process arguments (in reverse)
		for (size_t argument_index = 0; argument_index < argument_indices.size(); argument_index++) {
			size_t reverse_argument_index = argument_indices.size() - argument_index - 1;

			c_ast_node_expression *expression = build_expression(parse_tree, tokens,
				parse_tree.get_node(argument_indices[reverse_argument_index]));

			module_call->add_argument(expression);
		}
	}

	return module_call;
}
