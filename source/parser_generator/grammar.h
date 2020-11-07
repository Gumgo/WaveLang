#pragma once

#include "common/common.h"

#include <vector>

struct s_grammar {
	enum class e_associativity {
		k_none,
		k_left,
		k_right,
		k_nonassoc,

		k_count
	};

	struct s_terminal {
		std::string value;
		e_associativity associativity;
	};

	struct s_nonterminal {
		std::string value;
		std::string context_type;
	};

	struct s_rule_component {
		std::string terminal_or_nonterminal;
		bool is_terminal;
		size_t terminal_or_nonterminal_index;
		std::string argument;
	};

	struct s_rule {
		std::string nonterminal;
		size_t nonterminal_index;
		std::string function;
		std::vector<s_rule_component> components;
	};

	std::string grammar_name;

	std::string terminal_type_name;
	std::string terminal_context_type;
	std::string terminal_value_prefix;
	std::vector<s_terminal> terminals;

	std::string nonterminal_type_name;
	std::string nonterminal_value_prefix;
	std::vector<s_nonterminal> nonterminals;

	std::vector<s_rule> rules;
};
