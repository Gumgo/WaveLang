#include "compiler/lexer.h"
#include "compiler/preprocessor.h"
#include <unordered_map>

static const char *k_lexer_token_table[] = {
	"",			// k_token_type_invalid

	"const",	// k_token_type_keyword_const
	"in",		// k_token_type_keyword_in
	"out",		// k_token_type_keyword_out
	"module",	// k_token_type_keyword_module
	"void",		// k_token_type_keyword_void,
	"real",		// k_token_type_keyword_real,
	"string",	// k_token_type_keyword_string,
	"return",	// k_token_type_keyword_return

	"",			// k_token_type_identifier

	"",			// k_token_type_constant_real
	"",			// k_token_type_constant_string

	"(",		// k_token_type_left_parenthesis
	")",		// k_token_type_right_parenthesis

	"{",		// k_token_type_left_brace
	"}",		// k_token_type_right_brace

	",",		// k_token_type_comma
	";",		// k_token_type_semicolon

	":=",		// k_token_type_operator_assignment
	"+",		// k_token_type_operator_addition
	"-",		// k_token_type_operator_subtraction
	"*",		// k_token_type_multiplication
	"/",		// k_token_type_division
	"%",		// k_token_type_modulo

	"//"		// k_token_type_comment
};

static_assert(NUMBEROF(k_lexer_token_table) == k_token_type_count, "Invalid lexer token table");

typedef std::unordered_map<std::string, e_token_type> c_lexer_table;

struct s_lexer_globals {
	c_lexer_table keyword_table;
	c_lexer_table single_character_operator_table;
	c_lexer_table operator_table;
};

static bool g_lexer_initialized = false;
static s_lexer_globals g_lexer_globals;

static bool process_source_file(
	const s_compiler_source_file &source_file,
	int32 source_file_index,
	s_lexer_source_file_output &source_file_output,
	std::vector<s_compiler_result> &out_errors);

static s_token read_next_token(c_compiler_string str);

void c_lexer::initialize_lexer() {
	wl_assert(!g_lexer_initialized);

	for (size_t token_type = k_token_type_first_keyword;
		 token_type <= k_token_type_last_keyword;
		 token_type++) {
		g_lexer_globals.keyword_table.insert(std::make_pair(
			k_lexer_token_table[token_type], static_cast<e_token_type>(token_type)));
	}

	for (size_t token_type = k_token_type_first_single_character_operator;
		 token_type <= k_token_type_last_single_character_operator;
		 token_type++) {
		g_lexer_globals.single_character_operator_table.insert(std::make_pair(
			k_lexer_token_table[token_type], static_cast<e_token_type>(token_type)));
	}

	for (size_t token_type = k_token_type_first_operator;
		 token_type <= k_token_type_last_operator;
		 token_type++) {
		g_lexer_globals.operator_table.insert(std::make_pair(
			k_lexer_token_table[token_type], static_cast<e_token_type>(token_type)));
	}

	// Comment token goes in the operator table
	g_lexer_globals.operator_table.insert(std::make_pair(
		k_lexer_token_table[k_token_type_comment], k_token_type_comment));

	g_lexer_initialized = true;
}

void c_lexer::shutdown_lexer() {
	wl_assert(g_lexer_initialized);

	g_lexer_globals.keyword_table.clear();
	g_lexer_globals.single_character_operator_table.clear();
	g_lexer_globals.operator_table.clear();

	g_lexer_initialized = false;
}

s_compiler_result c_lexer::process(const s_compiler_context &context, s_lexer_output &output,
	std::vector<s_compiler_result> &out_errors) {
	wl_assert(g_lexer_initialized);
	s_compiler_result result;
	result.clear();

	bool failed = false;

	// Iterate over each source file and attempt to process it
	for (size_t source_file_index = 0; source_file_index < context.source_files.size(); source_file_index++) {
		const s_compiler_source_file &source_file = context.source_files[source_file_index];
		output.source_file_output.push_back(s_lexer_source_file_output());
		failed |= !process_source_file(source_file, static_cast<int32>(source_file_index),
			output.source_file_output.back(), out_errors);
	}

	if (failed) {
		// Don't associate the error with a particular file, those errors are collected through out_errors
		result.result = k_compiler_result_lexer_error;
		result.message = "Lexer encountered error(s)";
	}

	return result;
}

static bool process_source_file(
	const s_compiler_source_file &source_file,
	int32 source_file_index,
	s_lexer_source_file_output &source_file_output,
	std::vector<s_compiler_result> &out_errors) {
	// Initially assume success
	bool result = true;

	c_compiler_string source(
		source_file.source.empty() ? nullptr : &source_file.source.front(),
		source_file.source.size());

	// Keep reading tokens until we find EOF
	bool eof = false;
	size_t line_start = 0;
	int32 line_number = 0;
	while (!eof) {
		// Line length includes the \n, if it exists
		size_t line_length = compiler_utility::find_line_length(source.advance(line_start));
		size_t line_end = line_start + line_length;
		size_t next_line_start = line_start + line_length;

		// Cut off the \n character
		if (line_length > 0 && source[line_end - 1] == '\n') {
			line_end--;
			line_length--;
		}

		c_compiler_string line = source.substr(line_start, line_length);

		if (c_preprocessor::is_valid_preprocessor_line(line)) {
			// This line was already parsed by the preprocessor - skip it
		} else {
			c_compiler_string line_remaining = line;
			bool eol = false;
			int32 pos = 0;

			// Keep reading tokens until we reach the end of the line or a comment symbol
			while (!eol) {
				size_t whitespace = compiler_utility::skip_whitespace(line_remaining);
				line_remaining = line_remaining.advance(whitespace);
				pos += static_cast<int32>(whitespace);

				if (line_remaining.is_empty()) {
					eol = true;
				} else {
					s_token token = read_next_token(line_remaining);

					if (token.token_type == k_token_type_invalid) {
						// Report the error using the string returned
						out_errors.push_back(s_compiler_result());
						s_compiler_result &error = out_errors.back();

						error.clear();
						error.result = k_compiler_result_invalid_token;
						error.source_location.source_file_index = source_file_index;
						error.source_location.line = line_number;
						error.source_location.pos = pos;
						error.message = "Invalid token '" + token.token_string.to_std_string() + "'";
						result = false;
					} else if (token.token_type == k_token_type_comment) {
						// Ignore the rest of the line because it is a comment
						eol = true;
					} else {
						token.source_location.source_file_index = source_file_index;
						token.source_location.line = line_number;
						token.source_location.pos = pos;
						source_file_output.tokens.push_back(token);
					}

					line_remaining = line_remaining.advance(token.token_string.get_length());
					if (token.token_type == k_token_type_constant_string) {
						// We need to also skip the quotes because they're not included in the string
						line_remaining = line_remaining.advance(2);
					}

					pos += static_cast<int32>(token.token_string.get_length());
				}
			}
		}

		// Move to the next line
		line_start = next_line_start;
		wl_assert(line_start <= source.get_length());
		line_number++;

		// Check if we've reached the end of the file
		if (line_start == source.get_length()) {
			eof = true;
		}
	}

	return result;
}

static s_token read_next_token(
	c_compiler_string str) {
	// The algorithm is as follows:
	// 1) Detect identifiers, read up to any non-identifier symbol
	// 2) Detect constants, read up to any non-constant symbol
	// 3) Detect single character operators, read a single character
	// 3) Detect operators, read up to any non-operator

	s_token result;
	result.token_type = k_token_type_invalid;

	// First check if the starting character is valid
	if (!compiler_utility::is_valid_source_character(str[0])) {
		result.token_string = str.substr(0, 1);
		return result;
	}

	size_t identifier_length = compiler_utility::find_identifier_length(str);
	if (identifier_length != 0) {
		// Store the identifier, detect if it is a keyword
		result.token_string = str.substr(0, identifier_length);

		c_lexer_table::const_iterator match = g_lexer_globals.keyword_table.find(result.token_string.to_std_string());
		if (match != g_lexer_globals.keyword_table.end()) {
			result.token_type = match->second;
		} else {
			result.token_type = k_token_type_identifier;
		}

		return result;
	}

	// Check if the token is a constant real
	if (compiler_utility::is_number(str[0])) {
		bool error = false;
		bool decimal_point_found = false;

		size_t index;
		for (index = 1; index < str.get_length(); index++) {
			if (compiler_utility::is_number(str[index])) {
				// Keep reading
			} else if (str[index] == '.') {
				if (decimal_point_found) {
					// Two decimal points aren't allowed
					error = true;
				}
				decimal_point_found = true;
			} else {
				// We found the end of the constant
				break;
			}
		}

		// Set the token string even if we detect an error
		result.token_string = str.substr(0, index);

		if (error) {
			result.token_type = k_token_type_invalid;
		} else {
			// Make sure the value is valid
			try {
				std::stof(result.token_string.to_std_string());
				result.token_type = k_token_type_constant_real;
			} catch (const std::invalid_argument &) {
				result.token_type = k_token_type_invalid;
			} catch (const std::out_of_range &) {
				result.token_type = k_token_type_invalid;
			}
		}

		return result;
	}

	// Check if the token is the start of a string
	if (str[0] == '"') {
		bool error = false;
		bool found_end_quote = false;

		// Read until we find another quote or encounter an error
		size_t index;
		for (index = 1; index < str.get_length(); index++) {
			char ch = str[index];

			if (!compiler_utility::is_valid_source_character(ch)) {
				error = true;
			} else if (ch == '\\') {
				// Escape sequence
				index++;
				size_t advance = compiler_utility::resolve_escape_sequence(str.advance(index), nullptr);
				if (advance == 0) {
					// Error resolving escape sequence
					error = true;
				} else {
					// Keep the sequence in the string but advance over it. We will resolve it later.
					index += advance;
				}
			} else if (ch == '"') {
				// Found the end of the string
				found_end_quote = true;
				break;
			}
		}

		// Set the token string even if we detect an error
		// Leave off the start and end quotes
		result.token_string = str.substr(1, index - (found_end_quote ? 1 : 0));

		if (error) {
			result.token_type = k_token_type_invalid;
		} else {
			result.token_type = k_token_type_constant_string;
		}

		return result;
	}

	// Check for single character operators, which are always a single character
	{
		std::string ch;
		ch.push_back(str[0]);

		c_lexer_table::const_iterator match = g_lexer_globals.single_character_operator_table.find(ch);
		if (match != g_lexer_globals.single_character_operator_table.end()) {
			result.token_string = str.substr(0, 1);
			result.token_type = match->second;
			return result;
		}
	}

	// Check for operators
	{
		// Find the first non-operator symbol
		size_t operator_end_index;
		for (operator_end_index = 0; operator_end_index < str.get_length(); operator_end_index++) {
			char ch = str[operator_end_index];
			// We consider valid characters which are not identifiers, numbers, or whitespace to be operators
			bool is_operator =
				compiler_utility::is_valid_source_character(ch) &&
				!compiler_utility::is_whitespace(ch) &&
				!compiler_utility::is_valid_identifier_character(ch) &&
				!compiler_utility::is_number(ch);

			if (is_operator) {
				// Exclude single character operators
				std::string ch_str;
				ch_str.push_back(ch);
				c_lexer_table::const_iterator match = g_lexer_globals.single_character_operator_table.find(ch_str);

				if (match != g_lexer_globals.single_character_operator_table.end()) {
					// This is a single character operator, and thus not part of this operator
					is_operator = false;
				}
			}

			if (!is_operator) {
				break;
			}
		}

		// We should never get an empty string, because we already guarded against invalid character, and we've already
		// detected letters, numbers, underscores, and single character operators, and we should never enter this
		// function with whitespace at the beginning of the string.
		wl_assert(operator_end_index > 0);

		// Look up the operator
		{
			result.token_string = str.substr(0, operator_end_index);
			c_lexer_table::const_iterator match = g_lexer_globals.operator_table.find(
				result.token_string.to_std_string());

			if (match != g_lexer_globals.operator_table.end()) {
				result.token_type = match->second;
			} else {
				result.token_type = k_token_type_invalid;
			}

			return result;
		}
	}
}