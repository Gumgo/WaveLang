#include "parser_generator/grammar_parser.h"

#include <unordered_map>

// Convenience macro that returns false if the next token isn't what we expect
#define EXPECT_TOKEN(expected_token_type)								\
MACRO_BLOCK(															\
	if (m_tokens[m_token_index].token_type != (expected_token_type)) {	\
		m_error_message = "Unexpected token";							\
		return false;													\
	}																	\
)

static constexpr const char *k_end_of_input_terminal_name = "end_of_input";
static constexpr const char *k_start_nonterminal_name = "start";

c_grammar_parser::c_grammar_parser(
	const char *source,
	c_wrapped_array<const s_grammar_token> tokens,
	s_grammar *grammar_out)
	: m_source(source)
	, m_tokens(tokens)
	, m_token_index(0)
	, m_grammar(grammar_out)
	, m_terminals_read(false)
	, m_nonterminals_read(false) {}

bool c_grammar_parser::parse() {
	if (m_tokens.get_count() == 0) {
		m_error_message = "No tokens provided";
		return false;
	}

	while (true) {
		if (next_token().token_type == e_grammar_token_type::k_eof) {
			return finalize_and_validate();
		}

		EXPECT_TOKEN(e_grammar_token_type::k_identifier);
		std::string command = next_token_string();

		bool processed_command = false;

		if (!try_read_simple_command(
			"grammar_name",
			e_grammar_token_type::k_identifier,
			m_grammar->grammar_name,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		std::string include;
		if (!try_read_simple_command(
			"include",
			e_grammar_token_type::k_string,
			include,
			processed_command)) {
			return false;
		} else if (processed_command) {
			m_grammar->includes.push_back(include);
		}

		if (!try_read_simple_command(
			"terminal_type_name",
			e_grammar_token_type::k_string,
			m_grammar->terminal_type_name,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_simple_command(
			"terminal_context_type",
			e_grammar_token_type::k_string,
			m_grammar->terminal_context_type,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_simple_command(
			"terminal_value_prefix",
			e_grammar_token_type::k_identifier,
			m_grammar->terminal_value_prefix,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_simple_command(
			"nonterminal_type_name",
			e_grammar_token_type::k_string,
			m_grammar->nonterminal_type_name,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_simple_command(
			"nonterminal_value_prefix",
			e_grammar_token_type::k_identifier,
			m_grammar->nonterminal_value_prefix,
			processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_terminals_command(processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		if (!try_read_nonterminals_command(processed_command)) {
			return false;
		} else if (processed_command) {
			continue;
		}

		m_error_message = "Unknown command '" + command + "'";
		return false;
	}
}

bool c_grammar_parser::try_read_simple_command(
	const char *command_name,
	e_grammar_token_type value_type,
	std::string &value,
	bool &processed_command_out) {
	processed_command_out = false;
	if (next_token().token_type != e_grammar_token_type::k_identifier) {
		return true;
	}

	std::string command = next_token_string();
	if (command != command_name) {
		return true;
	}

	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_colon);
	m_token_index++;

	EXPECT_TOKEN(value_type);

	if (!value.empty()) {
		m_error_message = "'" + std::string(command_name) + "' already specified";
		return false;
	}

	value = next_token_string();

	if (value.empty()) {
		m_error_message = "Empty value for '" + std::string(command_name) + "'";
		return false;
	}

	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_semicolon);
	m_token_index++;

	processed_command_out = true;
	return true;
}

bool c_grammar_parser::try_read_terminals_command(bool &processed_command_out) {
	processed_command_out = false;
	if (next_token().token_type != e_grammar_token_type::k_identifier) {
		return true;
	}

	std::string command = next_token_string();
	if (command != "terminals") {
		return true;
	}

	if (m_terminals_read) {
		m_error_message = "'terminals' already specified";
	}

	m_terminals_read = true;

	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_colon);
	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_left_brace);
	m_token_index++;

	while (next_token().token_type != e_grammar_token_type::k_right_brace) {
		if (!read_terminal()) {
			return false;
		}
	}

	m_token_index++;

	processed_command_out = true;
	return true;
}

bool c_grammar_parser::try_read_nonterminals_command(bool &processed_command_out) {
	processed_command_out = false;
	if (next_token().token_type != e_grammar_token_type::k_identifier) {
		return true;
	}

	std::string command = next_token_string();
	if (command != "nonterminals") {
		return true;
	}

	if (m_nonterminals_read) {
		m_error_message = "'nonterminals' already specified";
	}

	m_nonterminals_read = true;

	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_colon);
	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_left_brace);
	m_token_index++;

	while (next_token().token_type != e_grammar_token_type::k_right_brace) {
		if (!read_nonterminal()) {
			return false;
		}
	}

	m_token_index++;

	processed_command_out = true;
	return true;
}

bool c_grammar_parser::read_terminal() {
	s_grammar::s_terminal terminal;
	terminal.associativity = s_grammar::e_associativity::k_none;

	EXPECT_TOKEN(e_grammar_token_type::k_identifier);
	terminal.value = next_token_string();

	for (const s_grammar::s_terminal &existing_terminal: m_grammar->terminals) {
		if (existing_terminal.value == terminal.value) {
			m_error_message = "Duplicate terminal '" + terminal.value + "'";
			return false;
		}
	}

	for (const s_grammar::s_nonterminal &existing_nonterminal : m_grammar->nonterminals) {
		if (existing_nonterminal.value == terminal.value) {
			m_error_message = "Conflicting terminal and nonterminal '" + terminal.value + '"';
			return false;
		}
	}

	m_token_index++;

	if (next_token().token_type == e_grammar_token_type::k_left_parenthesis) {
		m_token_index++;

		EXPECT_TOKEN(e_grammar_token_type::k_identifier);
		std::string associativity = next_token_string();

		if (associativity == "left") {
			terminal.associativity = s_grammar::e_associativity::k_left;
		} else if (associativity == "right") {
			terminal.associativity = s_grammar::e_associativity::k_right;
		} else if (associativity == "nonassoc") {
			terminal.associativity = s_grammar::e_associativity::k_nonassoc;
		} else {
			m_error_message = "Invalid associativity";
			return false;
		}

		m_token_index++;

		EXPECT_TOKEN(e_grammar_token_type::k_right_parenthesis);
		m_token_index++;
	}

	EXPECT_TOKEN(e_grammar_token_type::k_semicolon);
	m_token_index++;

	m_grammar->terminals.push_back(terminal);
	return true;
}

bool c_grammar_parser::read_nonterminal() {
	EXPECT_TOKEN(e_grammar_token_type::k_identifier);
	std::string nonterminal_value = next_token_string();

	for (const s_grammar::s_terminal &existing_terminal : m_grammar->terminals) {
		if (existing_terminal.value == nonterminal_value) {
			m_error_message = "Conflicting terminal and nonterminal '" + nonterminal_value + "'";
			return false;
		}
	}

	bool is_new_nonterminal = false;
	s_grammar::s_nonterminal *nonterminal = nullptr;
	for (s_grammar::s_nonterminal &existing_nonterminal : m_grammar->nonterminals) {
		if (existing_nonterminal.value == nonterminal_value) {
			nonterminal = &existing_nonterminal;
			break;
		}
	}

	if (!nonterminal) {
		is_new_nonterminal = true;
		m_grammar->nonterminals.push_back(s_grammar::s_nonterminal());
		nonterminal = &m_grammar->nonterminals.back();
		nonterminal->value = nonterminal_value;
	}

	m_token_index++;

	if (next_token().token_type == e_grammar_token_type::k_left_parenthesis) {
		m_token_index++;

		EXPECT_TOKEN(e_grammar_token_type::k_string);
		std::string context_type = next_token_string();

		if (is_new_nonterminal) {
			nonterminal->context_type = context_type;
		} else {
			if (context_type != nonterminal->context_type) {
				m_error_message = "Nonterminal context type doesn't match";
				return false;
			}
		}

		m_token_index++;

		EXPECT_TOKEN(e_grammar_token_type::k_right_parenthesis);
		m_token_index++;
	}

	EXPECT_TOKEN(e_grammar_token_type::k_colon);
	m_token_index++;

	EXPECT_TOKEN(e_grammar_token_type::k_left_brace);
	m_token_index++;

	while (next_token().token_type != e_grammar_token_type::k_right_brace) {
		if (!read_rule(nonterminal->value)) {
			return false;
		}
	}

	m_token_index++;
	return true;
}

bool c_grammar_parser::read_rule(const std::string &nonterminal) {
	m_grammar->rules.push_back(s_grammar::s_rule());
	s_grammar::s_rule &rule = m_grammar->rules.back();
	rule.nonterminal = nonterminal;

	if (next_token().token_type == e_grammar_token_type::k_period) {
		// Empty rule
		m_token_index++;
	} else {
		// Read rule components
		while (next_token().token_type == e_grammar_token_type::k_identifier) {
			rule.components.push_back(s_grammar::s_rule_component());
			s_grammar::s_rule_component &component = rule.components.back();
			component.terminal_or_nonterminal = next_token_string();
			m_token_index++;

			if (next_token().token_type == e_grammar_token_type::k_left_parenthesis) {
				m_token_index++;

				EXPECT_TOKEN(e_grammar_token_type::k_identifier);
				component.argument = next_token_string();
				m_token_index++;

				EXPECT_TOKEN(e_grammar_token_type::k_right_parenthesis);
				m_token_index++;
			}
		}
	}

	if (next_token().token_type == e_grammar_token_type::k_arrow) {
		m_token_index++;

		EXPECT_TOKEN(e_grammar_token_type::k_identifier);
		rule.function = next_token_string();
		m_token_index++;
	}

	EXPECT_TOKEN(e_grammar_token_type::k_semicolon);
	m_token_index++;
	return true;
}

bool c_grammar_parser::finalize_and_validate() {
	if (m_grammar->grammar_name.empty()) {
		m_error_message = "'grammar_name' not specified";
		return false;
	}

	if (m_grammar->nonterminals.empty()) {
		m_error_message = "No nonterminals specified";
		return false;
	}

	if (m_grammar->terminal_type_name.empty()) {
		m_error_message = "'terminal_type_name' not specified";
		return false;
	}

	if (m_grammar->terminal_context_type.empty()) {
		m_error_message = "'terminal_context_type' not specified";
		return false;
	}

	if (m_grammar->terminal_value_prefix.empty()) {
		m_error_message = "'terminal_value_prefix' not specified";
		return false;
	}

	if (m_grammar->nonterminal_type_name.empty()) {
		m_error_message = "'nonterminal_type_name' not specified";
		return false;
	}

	if (m_grammar->nonterminal_value_prefix.empty()) {
		m_error_message = "'nonterminal_value_prefix' not specified";
		return false;
	}

	// Maps functions to a rule which uses that function; used to validate that all functions match
	std::unordered_map<std::string, const s_grammar::s_rule *> functions;

	for (s_grammar::s_rule &rule : m_grammar->rules) {
		IF_ASSERTS_ENABLED(bool found = false;)
		for (size_t index = 0; index < m_grammar->nonterminals.size(); index++) {
			if (rule.nonterminal == m_grammar->nonterminals[index].value) {
				IF_ASSERTS_ENABLED(found = true;)
				rule.nonterminal_index = index;
				break;
			}
		}
		wl_assert(found);

		// Make sure all the rule components are valid
		for (s_grammar::s_rule_component &component : rule.components) {
			bool found = false;
			bool has_context = false;

			for (size_t index = 0; index < m_grammar->terminals.size(); index++) {
				if (component.terminal_or_nonterminal == m_grammar->terminals[index].value) {
					component.is_terminal = true;
					component.terminal_or_nonterminal_index = index;
					found = true;
					has_context = true;
					break;
				}
			}

			if (!found) {
				for (size_t index = 0; index < m_grammar->nonterminals.size(); index++) {
					if (component.terminal_or_nonterminal == m_grammar->nonterminals[index].value) {
						component.is_terminal = false;
						component.terminal_or_nonterminal_index = index;
						found = true;
						has_context = !m_grammar->nonterminals[index].context_type.empty();
						break;
					}
				}
			}

			if (!found) {
				m_error_message = "'" + component.terminal_or_nonterminal + "' is not a terminal or nonterminal";
				return false;
			}

			if (!component.argument.empty()) {
				if (!has_context) {
					m_error_message = "'" + component.terminal_or_nonterminal
						+ "' cannot be used as an argument because it has no associated context type";
					return false;
				}

				for (const s_grammar::s_rule_component &other_component : rule.components) {
					if (&component != &other_component && component.argument == other_component.argument) {
						m_error_message = "Function '" + rule.function + "' has multiple arguments with the same name";
						return false;
					}
				}
			}
		}

		if (!rule.function.empty()) {
			auto iter = functions.find(rule.function);
			if (iter == functions.end()) {
				functions.insert(std::make_pair(rule.function, &rule));
			} else {
				// Verify that the two function usages match
				const s_grammar::s_rule &other_rule = *iter->second;

				if (rule.nonterminal != other_rule.nonterminal) {
					m_error_message =
						"Function '" + rule.function + "' is referenced by multiple rules with different nonterminals";
				}

				std::vector<const s_grammar::s_rule_component *> argument_components_a;
				std::vector<const s_grammar::s_rule_component *> argument_components_b;

				for (const s_grammar::s_rule_component &component : rule.components) {
					if (!component.argument.empty()) {
						argument_components_a.push_back(&component);
					}
				}

				for (const s_grammar::s_rule_component &component : other_rule.components) {
					if (!component.argument.empty()) {
						argument_components_b.push_back(&component);
					}
				}

				if (argument_components_a.size() != argument_components_b.size()) {
					m_error_message = "Function '" + rule.function
						+ "' is referenced by multiple rules with different argument counts";
					return false;
				}

				for (size_t index = 0; index < argument_components_a.size(); index++) {
					const s_grammar::s_rule_component &argument_component_a = *argument_components_a[index];
					const s_grammar::s_rule_component &argument_component_b = *argument_components_b[index];

					if (argument_component_a.terminal_or_nonterminal != argument_component_b.terminal_or_nonterminal) {
						m_error_message = "Function '" + rule.function
							+ "' is referenced by multiple rules with different argument types";
						return false;
					}

					if (argument_component_a.argument != argument_component_b.argument) {
						m_error_message = "Function '" + rule.function
							+ "' is referenced by multiple rules with different argument names";
						return false;
					}
				}
			}
		}
	}

	return true;
}
