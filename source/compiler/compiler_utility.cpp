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

	bool last_char_was_dot = false;
	for (size_t index = 1; index < str.get_length(); index++) {
		if (is_valid_identifier_character(str[index])) {
			bool is_dot = (str[index] == '.');
			if (is_dot && last_char_was_dot) {
				// Can't have two dots in a row
				return 0;
			}

			last_char_was_dot = is_dot;
		} else {
			if (last_char_was_dot) {
				// Can't end in a dot
				return 0;
			} else {
				return index;
			}
		}
	}

	if (last_char_was_dot) {
		// Can't end in a dot
		return 0;
	}

	return str.get_length();
}

bool compiler_utility::is_valid_identifier(const char *str) {
	wl_assert(str);

	if (!is_valid_start_identifier_character(str[0])) {
		// Handles empty string - first character will be null terminator
		return false;
	}

	const char *current_char = str + 1;
	bool last_char_was_dot = false;
	while (true) {
		char ch = *current_char;
		current_char++;

		if (ch == '\0') {
			break;
		} else if (is_valid_identifier_character(ch)) {
			bool is_dot = (ch == '.');
			if (is_dot && last_char_was_dot) {
				// Can't have two dots in a row
				return false;
			}

			last_char_was_dot = is_dot;
		} else {
			return false;
		}
	}

	if (last_char_was_dot) {
		// Can't end in a dot
		return false;
	}

	return true;
}

bool compiler_utility::is_valid_start_identifier_character(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

bool compiler_utility::is_valid_identifier_character(char c) {
	return is_valid_start_identifier_character(c) || is_number(c) || (c == '.');
}

bool compiler_utility::is_number(char c) {
	return (c >= '0' && c <= '9');
}

bool compiler_utility::is_valid_source_character(char c) {
	return c >= 0x20 && c <= 0x7E;
}

size_t compiler_utility::resolve_escape_sequence(c_compiler_string str, char *result_out) {
	char result = 0;
	size_t advance = 0;

	if (!str.is_empty()) {
		switch (str[0]) {
		case 'b':
			result = '\b';
			advance = 1;
			break;

		case 'f':
			result = '\f';
			advance = 1;
			break;

		case 'n':
			result = '\n';
			advance = 1;
			break;

		case 'r':
			result = '\r';
			advance = 1;
			break;

		case 't':
			result = '\t';
			advance = 1;
			break;

		case 'v':
			result = '\v';
			advance = 1;
			break;

		case '\'':
			result = '\'';
			advance = 1;
			break;

		case '"':
			result = '"';
			advance = 1;
			break;

		case '\\':
			result = '\\';
			advance = 1;
			break;

		case 'x':
			// The next two characters should form a hex digit
			if (str.get_length() >= 3) {
				bool valid = true;
				int32 value[2];

				for (size_t i = 1; i < 3; i++) {
					char ch = str[i];
					int32 val = 0;

					if (ch >= '0' && ch <= '9') {
						val = ch - '0';
					} else if (ch >= 'a' && ch <= 'f') {
						val = 10 + (ch - 'a');
					} else if (ch >= 'A' && ch <= 'F') {
						val = 10 + (ch - 'A');
					} else {
						valid = false;
					}

					value[i - 1] = val;
				}

				if (valid) {
					int32 full_value = value[0] * 16 + value[1];
					wl_assert(full_value >= 0 && full_value <= 255);
					full_value = (full_value <= 127) ? full_value : (full_value - 256);

					// Don't allow null-terminators into strings - makes things easier
					if (full_value != 0) {
						result = static_cast<char>(full_value);
						advance = 3;
					}
				}
			}
			break;

		default:
			// No valid resolution
			break;
		}
	}

	if (advance > 0 && result_out) {
		*result_out = result;
	}

	return advance;
}

std::string compiler_utility::escape_string(c_compiler_string unescaped_string) {
	std::string escaped_string;

	// Go through each character and resolve escape sequences
	for (size_t index = 0; index < unescaped_string.get_length(); index++) {
		char ch = unescaped_string[index];
		if (ch == '\\') {
			index++;
			char escape_result;
			size_t advance = compiler_utility::resolve_escape_sequence(
				unescaped_string.advance(index), &escape_result);
			wl_vassert(advance > 0, "Invalid escape sequence");

			index += advance;
			escaped_string.push_back(escape_result);
		} else {
			escaped_string.push_back(ch);
		}
	}

	return escaped_string;
}
