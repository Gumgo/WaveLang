#include "parser_generator/visitor_class_generator.h"

#include <unordered_set>

c_visitor_class_generator::c_visitor_class_generator(const s_grammar &grammar)
	: m_grammar(grammar) {
	m_class_name = "c_" + m_grammar.grammar_name + "_lr_parse_tree_visitor";
}

void c_visitor_class_generator::generate_class_declaration(std::ofstream &file) {
	file << "class " << m_class_name << " : public c_lr_parse_tree_visitor {\n";
	file << "public:\n";
	file << "\t" << m_class_name << "(const c_lr_parse_tree &parse_tree, c_wrapped_array<const "
		<< m_grammar.terminal_context_type << "> tokens);\n";
	file << "\tvoid visit() override;\n";
	file << "\n";
	file << "protected:\n";

	// Functions can be duplicated across multiple rules (all arguments are guaranteed to match). Keep track of which
	// ones we've generated so we don't write out duplicates.
	std::unordered_set<std::string> generated_functions;

	// Declarations for all of the functions associated with rules
	for (const s_grammar::s_rule &rule : m_grammar.rules) {
		if (!rule.function.empty() && !generated_functions.contains(rule.function)) {
			file << "\tvirtual bool enter_" << rule.function << "(";
			output_rule_function_declaration_arguments(rule, file);
			file << ") { return true; }\n";
			file << "\tvirtual void exit_" << rule.function << "(";
			output_rule_function_declaration_arguments(rule, file);
			file << ") {}\n\n";

			generated_functions.insert(rule.function);
		}
	}

	file << "\tbool enter_node(size_t node_index) override;\n";
	file << "\tvoid exit_node(size_t node_index) override;\n";

	file << "\n";
	file << "private:\n";

	// The variant type is used to store any possible context type
	file << "\tstruct s_no_context_value {};\n\n";
	file << "\tusing t_context_variant = std::variant<\n";
	file << "\t\ts_no_context_value";
	for (const std::string &context_type : get_unique_variant_types()) {
		file << ",\n\t\t" << context_type;
	}
	file << ">;\n\n";

	// This is used so we can generically construct types from their type name (e.g. construct_type<int32 *>())
	file << "\ttemplate<typename t_type>\n";
	file << "\tstatic t_type construct_context() {\n";
	file << "\t\treturn t_type();\n";
	file << "\t}\n\n";

	// List of tokens
	file << "\tc_wrapped_array<const " << m_grammar.terminal_context_type << "> m_tokens;\n";

	// This is a vector of contexts for each node
	file << "\tstd::vector<t_context_variant> m_node_contexts;\n";

	// This is a member as a memory optimization, so we don't have to constantly reallocate memory
	file << "\tstd::vector<size_t> m_child_node_indices_buffer;\n";

	file << "};\n";
}

void c_visitor_class_generator::generate_class_definition(std::ofstream &file) {
	file << m_class_name << "::" << m_class_name << "(const c_lr_parse_tree &parse_tree, c_wrapped_array<const "
		<< m_grammar.terminal_context_type << "> tokens)\n";
	file << "\t: c_lr_parse_tree_visitor(parse_tree)\n";
	file << "\t, m_tokens(tokens) {\n";
	file << "}\n\n";

	file << "void " << m_class_name << "::visit() {\n";
	file << "\tm_node_contexts.resize(m_parse_tree.get_node_count());\n";
	file << "\tc_lr_parse_tree_visitor::visit();\n\n";

	// Validate that all contexts have been deinstantiated
	file << "#if IS_TRUE(ASSERTS_ENABLED)\n";
	file << "\tfor (const t_context_variant &context_variant : m_node_contexts) {\n";
	file << "\t\twl_assert(context_variant.index() == 0);\n";
	file << "\t}\n";
	file << "#endif // IS_TRUE(ASSERTS_ENABLED)\n\n";

	file << "\tm_node_contexts.clear();\n";
	file << "}\n\n";

	output_enter_or_exit_node_function_definition(true, file);
	file << "\n";

	output_enter_or_exit_node_function_definition(false, file);
}

std::vector<std::string> c_visitor_class_generator::get_unique_variant_types() const {
	std::vector<std::string> unique_variant_types;

	// Build a list of unique types for the variant - this consists of all possible nonterminal contexts. The terminal
	// contexts are provided directly from the token stream.
	for (const s_grammar::s_nonterminal &nonterminal : m_grammar.nonterminals) {
		if (nonterminal.context_type.empty()) {
			continue;
		}

		bool found = false;
		for (const std::string &existing_type : unique_variant_types) {
			if (existing_type == nonterminal.context_type) {
				found = true;
				break;
			}
		}

		if (!found) {
			unique_variant_types.push_back(nonterminal.context_type);
		}
	}

	return unique_variant_types;
}

std::vector<c_visitor_class_generator::s_rule_function_argument> c_visitor_class_generator::get_rule_function_arguments(
	const s_grammar::s_rule &rule) const {
	std::vector<s_rule_function_argument> arguments;

	const s_grammar::s_nonterminal &nonterminal = m_grammar.nonterminals[rule.nonterminal_index];
	if (!nonterminal.context_type.empty()) {
		arguments.push_back({ e_argument_type::k_rule_nonterminal, nonterminal.context_type, "rule_head_context", 0 });
	}

	for (size_t index = 0; index < rule.components.size(); index++) {
		const s_grammar::s_rule_component &rule_component = rule.components[index];
		if (!rule_component.argument.empty()) {
			if (rule_component.is_terminal) {
				arguments.push_back(
					{
						e_argument_type::k_terminal,
						"const " + m_grammar.terminal_context_type,
						rule_component.argument,
						index
					});
			} else {
				const s_grammar::s_nonterminal &nonterminal_component =
					m_grammar.nonterminals[rule_component.terminal_or_nonterminal_index];
				arguments.push_back(
					{
						e_argument_type::k_nonterminal,
						nonterminal_component.context_type,
						rule_component.argument,
						index
					});
			}
		}
	}

	return arguments;
}

void c_visitor_class_generator::output_rule_function_declaration_arguments(
	const s_grammar::s_rule &rule,
	std::ofstream &file) const {
	std::vector<s_rule_function_argument> arguments = get_rule_function_arguments(rule);

	bool first = true;
	for (const s_rule_function_argument &argument : arguments) {
		if (!first) {
			file << ", ";
		} else {
			first = false;
		}

		file << argument.context_type;
		if (!argument.context_type.ends_with('*')) {
			file << " ";
		}
		file << "&" << argument.name;
	}
}

void c_visitor_class_generator::output_enter_or_exit_node_function_definition(bool enter, std::ofstream &file) const {
	file << (enter ? "bool " : "void ") << m_class_name << "::"
		<< (enter ? "enter" : "exit") << "_node(size_t node_index) {\n";
	file << "\tconst c_lr_parse_tree_node &node = m_parse_tree.get_node(node_index);\n";
	file << "\tif (node.get_symbol().is_terminal()) {\n";
	file << "\t\treturn" << (enter ? " false" : "") << ";\n"; // No point to visiting terminals
	file << "\t}\n\n";

	file << "\tbool is_root_node = node_index == m_parse_tree.get_root_node_index();\n";
	file << "\tget_child_node_indices(node_index, m_child_node_indices_buffer);\n\n";

	file << "\tswitch (node.get_production_index()) {\n";

	// Handle each possible production that has an associated function
	for (size_t rule_index = 0; rule_index < m_grammar.rules.size(); rule_index++) {
		const s_grammar::s_rule &rule = m_grammar.rules[rule_index];
		if (rule.function.empty()) {
			continue;
		}

		file << "\tcase " << rule_index << ":\n";

		if (enter) {
			// When calling enter, we must instantiate the contexts first, then pass them to the function
			output_instantiate_or_deinstantiate_contexts(true, rule, file);
			output_call_enter_or_exit_function(true, rule, file);
		} else {
			// When calling exit, we must pass contexts to the function before deinstantiating them
			output_call_enter_or_exit_function(false, rule, file);
			output_instantiate_or_deinstantiate_contexts(false, rule, file);
			file << "\t\tbreak;\n";
		}

		file << "\n";
	}

	file << "\tdefault:\n";
	file << "\t\t" << (enter ? "return true" : "break") << ";\n";
	file << "\t}\n";
	file << "}\n";
}

void c_visitor_class_generator::output_instantiate_or_deinstantiate_contexts(
	bool enter,
	const s_grammar::s_rule &rule,
	std::ofstream &file) const {
	std::vector<s_rule_function_argument> arguments = get_rule_function_arguments(rule);

	// Instantiate or deinstantiate each context
	for (const s_rule_function_argument &argument : arguments) {
		switch (argument.argument_type) {
		case e_argument_type::k_rule_nonterminal:
		{
			// We only need to [de]instantiate the rule's nonterminal context if this is the root node, otherwise
			// it will have already gotten [de]instantiated when we instantiate children
			const char *emplace_type = enter ? argument.context_type.c_str() : "s_no_context_value";
			file << "\t\tif (is_root_node) {\n";
			file << "\t\t\tm_node_contexts[node_index].emplace<" <<
				emplace_type << ">(construct_context<" << emplace_type << ">());\n";
			file << "\t\t}\n";
			break;
		}

		case e_argument_type::k_terminal:
			// Don't need to instantiate terminal arguments, they're taken directly from the token stream
			break;

		case e_argument_type::k_nonterminal:
		{
			const char *emplace_type = enter ? argument.context_type.c_str() : "s_no_context_value";
			file << "\t\tm_node_contexts[m_child_node_indices_buffer[" << argument.child_index << "]].emplace<"
				<< emplace_type << ">(construct_context<" << emplace_type << ">());\n";
			break;
		}

		default:
			wl_unreachable();
		}
	}
}

void c_visitor_class_generator::output_call_enter_or_exit_function(
	bool enter,
	const s_grammar::s_rule &rule,
	std::ofstream &file) const {
	std::vector<s_rule_function_argument> arguments = get_rule_function_arguments(rule);

	// Call the function
	file << "\t\t" << (enter ? "return enter_" : "exit_") << rule.function << "(";
	bool first = true;
	for (const s_rule_function_argument &argument : arguments) {
		if (!first) {
			file << ",";
		} else {
			first = false;
		}

		file << "\n\t\t\t";
		switch (argument.argument_type) {
		case e_argument_type::k_rule_nonterminal:
			file << "std::get<" << argument.context_type << ">(m_node_contexts[node_index])";
			break;

		case e_argument_type::k_terminal:
			file << "m_tokens[m_parse_tree.get_node(m_child_node_indices_buffer["
				<< argument.child_index << "]).get_token_index()]";
			break;

		case e_argument_type::k_nonterminal:
			file << "std::get<" << argument.context_type << ">(m_node_contexts[m_child_node_indices_buffer["
				<< argument.child_index << "]])";
			break;

		default:
			wl_unreachable();
		}
	}

	file << ");\n";
}
