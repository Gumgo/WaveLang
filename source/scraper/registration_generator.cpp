#include "scraper/registration_generator.h"
#include "scraper/scraper_result.h"

#include <fstream>
#include <iostream>
#include <map>

// This is more readable than \t and \n
#define TAB_STR "\t"
#define TAB2_STR "\t\t"
#define TAB3_STR "\t\t\t"
#define NEWLINE_STR "\n"

static const char *k_bool_strings[] = { "false", "true" };
static_assert(NUMBEROF(k_bool_strings) == 2, "Invalid bool strings");

static const char *k_native_module_primitive_type_enum_strings[] = {
	"k_native_module_primitive_type_real",
	"k_native_module_primitive_type_bool",
	"k_native_module_primitive_type_string"
};
static_assert(NUMBEROF(k_native_module_primitive_type_enum_strings) == k_native_module_primitive_type_count,
	"Invalid native module primitive type enum strings");

static const char *k_native_module_qualifier_enum_strings[] = {
	"k_native_module_qualifier_in",
	"k_native_module_qualifier_out",
	"k_native_module_qualifier_constant"
};
static_assert(NUMBEROF(k_native_module_qualifier_enum_strings) == k_native_module_qualifier_count,
	"Invalid native module qualifier enum strings");

static const char *k_native_operator_enum_strings[] = {
	"k_native_operator_noop",
	"k_native_operator_negation",
	"k_native_operator_addition",
	"k_native_operator_subtraction",
	"k_native_operator_multiplication",
	"k_native_operator_division",
	"k_native_operator_modulo",
	"k_native_operator_concatenation",
	"k_native_operator_not",
	"k_native_operator_equal",
	"k_native_operator_not_equal",
	"k_native_operator_less",
	"k_native_operator_greater",
	"k_native_operator_less_equal",
	"k_native_operator_greater_equal",
	"k_native_operator_and",
	"k_native_operator_or",
	"k_native_operator_array_dereference"
};
static_assert(NUMBEROF(k_native_operator_enum_strings) == k_native_operator_count,
	"Invalid native operator enum strings");

static const char *k_native_module_primitive_type_argument_strings[] = {
	"real",
	"bool",
	"string"
};
static_assert(NUMBEROF(k_native_module_primitive_type_argument_strings) == k_native_module_primitive_type_count,
	"Invalid native module primitive type argument strings");

static const char *k_native_module_qualifier_argument_strings[] = {
	"in",
	"out",
	"in"
};
static_assert(NUMBEROF(k_native_module_qualifier_argument_strings) == k_native_module_qualifier_count,
	"Invalid native module qualifier strings");

class c_optimization_rule_builder {
public:
	c_optimization_rule_builder(const c_scraper_result *scraper_result, const s_optimization_rule_declaration &rule);
	bool build(std::ofstream &out);
	std::string get_string_from_tokens() const;

private:
	enum e_token_type {
		k_token_type_unknown,
		k_token_type_identifier,
		k_token_type_real_constant,
		k_token_type_bool_constant,
		k_token_type_const,
		k_token_type_left_parenthesis,
		k_token_type_right_parenthesis,
		k_token_type_left_bracket,
		k_token_type_right_bracket,
		k_token_type_comma,
		k_token_type_source_to_target
	};

	struct s_identifier {
		size_t index;
		bool is_constant;
	};

	static e_token_type get_token_type(const std::string &token);
	std::string find_native_module_uid_string(const std::string &native_module_identifier) const;
	std::string get_build_identifier_string(const std::string &identifier, bool is_constant, bool building_source);
	void write_symbol_prefix(std::ofstream &out) const;

	const c_scraper_result *m_scraper_result;
	const s_optimization_rule_declaration &m_rule;

	// Maps identifier names to optimization rule variables/constants
	std::map<std::string, s_identifier> m_identifier_map;
	size_t m_variable_count;
	size_t m_constant_count;

	size_t m_symbol_index;
	const char *m_pattern_string;
};

bool generate_native_module_registration(
	const c_scraper_result *result, const char *registration_function_name, const char *fname) {
	wl_assert(result);
	wl_assert(fname);

	std::ofstream out(fname);

	// Generate header
	out << "// THIS FILE IS AUTO-GENERATED - DO NOT EDIT" NEWLINE_STR NEWLINE_STR;

	// Generate native module call wrappers
	for (size_t native_module_index = 0;
		 native_module_index < result->get_native_module_count();
		 native_module_index++) {
		const s_native_module_declaration &native_module = result->get_native_module(native_module_index);

		if (native_module.compile_time_call.empty()) {
			// No compile-time call
			continue;
		}

		out << "static void " << native_module.compile_time_call <<
			"_call_wrapper(c_native_module_compile_time_argument_list arguments) {" NEWLINE_STR;
		out << TAB_STR << native_module.compile_time_call << "(" NEWLINE_STR;

		for (size_t arg = 0; arg < native_module.arguments.size(); arg++) {
			c_native_module_qualified_data_type type = native_module.arguments[arg].type;
			out << TAB2_STR << "arguments[" << arg << "].get_" <<
				k_native_module_primitive_type_argument_strings[type.get_data_type().get_primitive_type()] <<
				(type.get_data_type().is_array() ? "_array_" : "_") <<
				k_native_module_qualifier_argument_strings[type.get_qualifier()] << "()";

			if (arg + 1 == native_module.arguments.size()) {
				out << ");" NEWLINE_STR;
			} else {
				out << "," NEWLINE_STR;
			}
		}

		out << "}" NEWLINE_STR NEWLINE_STR;
	}

	out << "bool " << registration_function_name << "() {" NEWLINE_STR;
	out << TAB_STR "bool result = true;" NEWLINE_STR;

	// Generate library registration
	for (size_t library_index = 0; library_index < result->get_library_count(); library_index++) {
		const s_library_declaration &library = result->get_library(library_index);
		out << TAB_STR "result &= c_native_module_registry::register_native_module_library(" <<
			library.id_string << ", \"" <<
			library.name << "\");" NEWLINE_STR;
	}

	// Generate each native module
	for (size_t native_module_index = 0;
		 native_module_index < result->get_native_module_count();
		 native_module_index++) {
		const s_native_module_declaration &native_module = result->get_native_module(native_module_index);

		size_t in_argument_count = 0;
		size_t out_argument_count = 0;
		size_t return_argument_index = k_invalid_argument_index;

		for (size_t arg = 0; arg < native_module.arguments.size(); arg++) {
			if (native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier())) {
				in_argument_count++;
			} else {
				wl_assert(native_module.arguments[arg].type.get_qualifier() == k_native_module_qualifier_out);
				out_argument_count++;

				if (native_module.arguments[arg].is_return_value) {
					return_argument_index = arg;
				}
			}
		}

		out << TAB_STR "{\n";
		out << TAB2_STR "s_native_module native_module;" NEWLINE_STR;
		out << TAB2_STR "ZERO_STRUCT(&native_module);" NEWLINE_STR;
		out << TAB2_STR "native_module.uid = " << native_module.uid_string << ";" NEWLINE_STR;

		out << TAB2_STR "native_module.name.set_verify(\"";
		if (native_module.native_operator.empty()) {
			out << native_module.name;
		} else {
			// The native module name is actually the operator name
			e_native_operator native_operator = k_native_operator_count;
			for (uint32 index = 0; index < k_native_operator_count; index++) {
				if (native_module.native_operator == k_native_operator_enum_strings[index]) {
					native_operator = static_cast<e_native_operator>(index);
				}
			}

			if (native_operator == k_native_operator_count) {
				std::cerr << "Invalid native operator '" << native_module.native_operator <<
					"' for native module '" << native_module.identifier << "'\n";
				return false;
			}

			out << get_native_operator_native_module_name(native_operator);
		}
		out << "\");" NEWLINE_STR;

		out << TAB2_STR "native_module.argument_count = " << native_module.arguments.size() << ";" NEWLINE_STR;
		out << TAB2_STR "native_module.in_argument_count = " << in_argument_count << ";" NEWLINE_STR;
		out << TAB2_STR "native_module.out_argument_count = " << out_argument_count << ";" NEWLINE_STR;
		out << TAB2_STR "native_module.return_argument_index = ";
		if (return_argument_index == k_invalid_argument_index) {
			out << "k_invalid_argument_index;" NEWLINE_STR;
		} else {
			out << return_argument_index << ";" NEWLINE_STR;
		}

		for (size_t arg = 0; arg < native_module.arguments.size(); arg++) {
			const s_native_module_argument_declaration &argument = native_module.arguments[arg];
			out << TAB2_STR "{" NEWLINE_STR;
			out << TAB3_STR "s_native_module_argument &arg = native_module.arguments[" << arg << "];" NEWLINE_STR;
			out << TAB3_STR "arg.name.set_verify(\"" << native_module.arguments[arg].name << "\");" NEWLINE_STR;
			out << TAB3_STR "arg.type = c_native_module_qualified_data_type(c_native_module_data_type(" <<
				k_native_module_primitive_type_enum_strings[argument.type.get_data_type().get_primitive_type()] <<
				", " << k_bool_strings[argument.type.get_data_type().is_array()] << "), " <<
				k_native_module_qualifier_enum_strings[argument.type.get_qualifier()] << ");" NEWLINE_STR;
			out << TAB2_STR "}" NEWLINE_STR;
		}

		out << TAB2_STR "native_module.compile_time_call = ";
		if (native_module.compile_time_call.empty()) {
			out << "nullptr;" NEWLINE_STR;
		} else {
			out << native_module.compile_time_call << "_call_wrapper;" NEWLINE_STR;
		}

		out << TAB2_STR "result &= c_native_module_registry::register_native_module(native_module);" NEWLINE_STR;
		out << TAB_STR "}\n";
	}

	// Generate each optimization rule
	for (size_t optimization_rule_index = 0;
		 optimization_rule_index < result->get_optimization_rule_count();
		 optimization_rule_index++) {
		const s_optimization_rule_declaration &optimization_rule =
			result->get_optimization_rule(optimization_rule_index);

		// Convert tokens to optimization rule symbols
		c_optimization_rule_builder builder(result, optimization_rule);
		if (!builder.build(out)) {
			std::cerr << "Failed to build optimization rule '" << builder.get_string_from_tokens() << "'\n";
			return false;
		}
	}

	out << TAB_STR "return result;" NEWLINE_STR;
	out << "}" NEWLINE_STR;

	if (out.fail()) {
		return false;
	}

	return true;
}

c_optimization_rule_builder::c_optimization_rule_builder(
	const c_scraper_result *scraper_result, const s_optimization_rule_declaration &rule)
	: m_scraper_result(scraper_result)
	, m_rule(rule) {
	m_variable_count = 0;
	m_constant_count = 0;
}

bool c_optimization_rule_builder::build(std::ofstream &out) {
	// Note: this function doesn't test for all error cases and can definitely produce invalid rules. It can also
	// probably produce valid rules from invalid syntax. I'm not too worried about it for now though, since we'll
	// validate the rules when they're registered at runtime, which is what matters most.

	// Note: rule.tokens has an extra empty token at the end to simplify token lookahead

	out << TAB_STR "{" NEWLINE_STR;

	// Generate a comment for the optimization rule
	out << TAB2_STR "// " << get_string_from_tokens() << NEWLINE_STR;

	// Build the rule
	out << TAB2_STR "s_native_module_optimization_rule rule;" NEWLINE_STR;
	out << TAB2_STR "ZERO_STRUCT(&rule);" NEWLINE_STR;

	bool building_source = true;
	m_symbol_index = 0;
	m_pattern_string = "source";
	bool next_identifier_is_constant = false;
	for (size_t index = 0; index < m_rule.tokens.size() - 1; index++) {
		bool is_constant = next_identifier_is_constant;
		next_identifier_is_constant = false;

		bool can_skip_comma = false;

		e_token_type token_type = get_token_type(m_rule.tokens[index]);
		e_token_type next_token_type = get_token_type(m_rule.tokens[index + 1]);

		if (is_constant && token_type != k_token_type_identifier) {
			// Only identifiers can have the const qualifier
			return false;
		}

		if (token_type == k_token_type_const) {
			next_identifier_is_constant = true;
		} else if (token_type == k_token_type_identifier) {
			if (next_token_type == k_token_type_left_parenthesis) {
				// Native module call
				if (is_constant) {
					return false;
				}

				std::string native_module_uid_string = find_native_module_uid_string(m_rule.tokens[index]);
				if (native_module_uid_string.empty()) {
					// Failed to find matching native module
					return false;
				}

				write_symbol_prefix(out);
				out << "build_native_module(" << native_module_uid_string << ");" NEWLINE_STR;
				m_symbol_index++;

				// Skip the left paren
				index++;
			} else if (next_token_type == k_token_type_left_bracket) {
				// Array dereference
				if (is_constant) {
					return false;
				}

				std::string identifier_string =
					get_build_identifier_string(m_rule.tokens[index], is_constant, building_source);
				if (identifier_string.empty()) {
					return false;
				}

				write_symbol_prefix(out);
				out << "build_array_dereference();" NEWLINE_STR;
				m_symbol_index++;
				write_symbol_prefix(out);
				out << identifier_string << ";" NEWLINE_STR;
				m_symbol_index++;

				// Skip the left bracket
				index++;
			} else {
				std::string identifier_string =
					get_build_identifier_string(m_rule.tokens[index], is_constant, building_source);
				if (identifier_string.empty()) {
					return false;
				}

				write_symbol_prefix(out);
				out << identifier_string << ";" NEWLINE_STR;
				m_symbol_index++;
				can_skip_comma = true;
			}
		} else if (token_type == k_token_type_real_constant) {
			// Real constant
			write_symbol_prefix(out);
			out << "build_real_value(" << m_rule.tokens[index] << ");" NEWLINE_STR;
			m_symbol_index++;
			can_skip_comma = true;
		} else if (token_type == k_token_type_bool_constant) {
			// Bool constant
			write_symbol_prefix(out);
			out << "build_bool_value(" << m_rule.tokens[index] << ");" NEWLINE_STR;
			m_symbol_index++;
			can_skip_comma = true;
		} else if (token_type == k_token_type_right_parenthesis) {
			// End of native module call
			write_symbol_prefix(out);
			out << "build_native_module_end();" NEWLINE_STR;
			m_symbol_index++;
			can_skip_comma = true;
		} else if (token_type == k_token_type_right_bracket) {
			// Nothing to do here: array dereference doesn't have an explicit "end" because it always takes exactly two
			// parameters, the variable and the index.
			can_skip_comma = true;
		} else if (token_type == k_token_type_source_to_target) {
			if (!building_source) {
				// We already switched to the target
				return false;
			}
			building_source = false;
			m_pattern_string = "target";
			m_symbol_index = 0;
		} else {
			// Anything else isn't allowed
			return false;
		}

		if (can_skip_comma && next_token_type == k_token_type_comma) {
			// Skip the comma. This can potentially lead to trailing commas, but it's not a big deal.
			index++;
		}
	}

	out << TAB2_STR "result &= c_native_module_registry::register_optimization_rule(rule);" NEWLINE_STR;
	out << TAB_STR "}" NEWLINE_STR;

	return true;
}

std::string c_optimization_rule_builder::get_string_from_tokens() const {
	std::string str;

	for (size_t index = 0; index < m_rule.tokens.size() - 1; index++) {
		if (m_rule.tokens[index] == ",") {
			str += ", ";
		} else if (m_rule.tokens[index] == "->") {
			str += " -> ";
		} else if (m_rule.tokens[index] == "const") {
			str += "const ";
		} else {
			str += m_rule.tokens[index];
		}
	}

	return str;
}

c_optimization_rule_builder::e_token_type c_optimization_rule_builder::get_token_type(const std::string &token) {
	if (token.empty()) {
		return k_token_type_unknown;
	}

	char first_char = token[0];
	if ((first_char >= 'A' && first_char <= 'Z') ||
		(first_char >= 'a' && first_char <= 'z') ||
		(first_char == '_')) {
		if (token == "false" || token == "true") {
			return k_token_type_bool_constant;
		} else if (token == "const") {
			return k_token_type_const;
		} else {
			return k_token_type_identifier;
		}
	} else if (token == "->") {
		return k_token_type_source_to_target;
	} else if ((first_char >= '0' && first_char <= '9') ||
		(first_char == '-')) {
		return k_token_type_real_constant;
	} else if (token == "(") {
		return k_token_type_left_parenthesis;
	} else if (token == ")") {
		return k_token_type_right_parenthesis;
	} else if (token == "[") {
		return k_token_type_left_bracket;
	} else if (token == "]") {
		return k_token_type_right_bracket;
	} else if (token == ",") {
		return k_token_type_comma;
	} else {
		return k_token_type_unknown;
	}
}

std::string c_optimization_rule_builder::find_native_module_uid_string(
	const std::string &native_module_identifier) const {
	for (size_t index = 0; index < m_scraper_result->get_native_module_count(); index++) {
		if (native_module_identifier == m_scraper_result->get_native_module(index).identifier) {
			return m_scraper_result->get_native_module(index).uid_string;
		}
	}

	// Empty string indicates that we failed to find a matching native module
	return std::string();
}

std::string c_optimization_rule_builder::get_build_identifier_string(
	const std::string &identifier, bool is_constant, bool building_source) {
	auto it = m_identifier_map.find(identifier);

	if (it == m_identifier_map.end()) {
		if (!building_source) {
			// If we're building the target pattern and this identifier doesn't exist, then the source pattern never
			// declared it which is an error
			return std::string();
		}

		s_identifier new_identifier;

		if (is_constant) {
			new_identifier.index = m_constant_count;
			m_constant_count++;
		} else {
			new_identifier.index = m_variable_count;
			m_variable_count++;
		}

		new_identifier.is_constant = is_constant;

		m_identifier_map.insert(std::make_pair(identifier, new_identifier));
	} else {
		if (building_source) {
			// We can't declare the same variable twice in the source pattern because we don't support checking whether
			// two variables in the source pattern hold the same value
			return std::string();
		} else if (is_constant) {
			// We aren't allowed to declare variables as constant in the target pattern because the target isn't what
			// we're matching against
			return std::string();
		}
	}

	// Re-lookup the identifier - it should definitely exist now
	it = m_identifier_map.find(identifier);
	wl_assert(it != m_identifier_map.end());

	const s_identifier &result = it->second;
	if (result.is_constant) {
		return "build_constant(" + std::to_string(result.index) + ")";
	} else {
		return "build_variable(" + std::to_string(result.index) + ")";
	}
}

void c_optimization_rule_builder::write_symbol_prefix(std::ofstream &out) const {
	out << TAB2_STR "rule." << m_pattern_string <<
		".symbols[" << m_symbol_index << "] = s_native_module_optimization_symbol::";
}
