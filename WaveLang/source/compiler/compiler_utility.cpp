#include "compiler/compiler_utility.h"

bool compiler_utility::is_whitespace(char c) {
	return c == ' ' || c == '\n' || c == '\t';
}


size_t compiler_utility::skip_whitespace(c_compiler_string str) {
	for (size_t index = 0; index < str.get_length(); index++) {
		if (!is_whitespace(str[index])) {
			// Return index of the first non-whitespace character
			return index;
		}
	}

	// Entire string is whitespace!
	return str.get_length();
}

size_t compiler_utility::find_line_length(c_compiler_string str) {
	size_t line_length;
	for (line_length = 0; line_length < str.get_length(); line_length++) {
		if (str[line_length] == '\n') {
			line_length++;
			break;
		}
	}

	return line_length;
}

size_t compiler_utility::find_identifier_length(c_compiler_string str) {
	if (str.get_length() > 0) {
		if (!is_valid_start_identifier_character(str[0])) {
			return 0;
		}
	}

	for (size_t index = 1; index < str.get_length(); index++) {
		if (!is_valid_identifier_character(str[index])) {
			return index;
		}
	}

	return str.get_length();
}

bool compiler_utility::is_valid_start_identifier_character(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

bool compiler_utility::is_valid_identifier_character(char c) {
	return is_valid_start_identifier_character(c) || is_number(c);
}

bool compiler_utility::is_number(char c) {
	return (c >= '0' && c <= '9');
}

bool compiler_utility::is_valid_source_character(char c) {
	return c >= 0x20 && c <= 0x7E;
}