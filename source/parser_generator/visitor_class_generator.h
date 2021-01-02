#pragma once

#include "common/common.h"

#include "parser_generator/grammar.h"

#include <fstream>
#include <unordered_map>
#include <vector>

class c_visitor_class_generator {
public:
	c_visitor_class_generator(const s_grammar &grammar);

	void generate_class_declaration(std::ofstream &file);
	void generate_class_definition(std::ofstream &file);

private:
	enum class e_argument_type {
		k_rule_nonterminal,
		k_nonterminal,
		k_terminal,

		k_count
	};

	struct s_rule_function_argument {
		e_argument_type argument_type;
		std::string context_type;
		std::string name;
		size_t child_index;
		bool is_consumed_by_function;
	};

	std::vector<std::string> get_unique_variant_types() const;
	std::vector<s_rule_function_argument> get_rule_function_arguments(const s_grammar::s_rule &rule) const;
	void output_rule_function_declaration_arguments(const s_grammar::s_rule &rule, std::ofstream &file) const;
	void output_enter_or_exit_node_function_definition(bool enter, std::ofstream &file) const;
	void output_instantiate_or_deinstantiate_contexts(
		bool enter,
		const s_grammar::s_rule &rule,
		std::ofstream &file) const;
	void output_call_enter_or_exit_function(
		bool enter,
		const s_grammar::s_rule &rule,
		std::ofstream &file) const;

	const s_grammar &m_grammar;
	std::string m_class_name;
};
