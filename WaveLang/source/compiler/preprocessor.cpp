#include "compiler/preprocessor.h"

enum e_preprocessor_token {
	k_preprocessor_token_invalid,

	// Right now we only have one token type
	k_preprocessor_token_import,

	k_preprocessor_token_count
};

static e_preprocessor_token get_preprocessor_token(c_compiler_string line, size_t &out_token_length);

static bool process_import(c_compiler_string line, c_compiler_string &out_import_string);

s_compiler_result c_preprocessor::preprocess(c_compiler_string source, c_preprocessor_output &output) {
	s_compiler_result result;
	result.clear();

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

		if (is_valid_preprocessor_line(line)) {
			// We expect to find # directly after skipping the whitespace
			c_compiler_string preprocessor_line = line.advance(compiler_utility::skip_whitespace(line));
			wl_assert(preprocessor_line[0] == '#');

			preprocessor_line = preprocessor_line.advance(1);

			size_t preprocessor_token_length = 0;
			e_preprocessor_token preprocessor_token = get_preprocessor_token(
				preprocessor_line, preprocessor_token_length);
			wl_assert(preprocessor_token_length <= preprocessor_line.get_length());

			switch (preprocessor_token) {
			case k_preprocessor_token_invalid:
				result.result = k_compiler_result_preprocessor_error;
				result.source_location.line = line_number;
				result.message = "Preprocessor error: '" + line.to_std_string() + "'";
				return result;

			case k_preprocessor_token_import:
				{
					// Store the import string result here
					c_compiler_string import_string;

					if (!process_import(preprocessor_line.advance(preprocessor_token_length), import_string)) {
						result.result = k_compiler_result_preprocessor_error;
						result.source_location.line = line_number;
						result.message = "Preprocessor error: '" + line.to_std_string() + "'";
						return result;
					} else {
						// Success! Add to the import list
						output.imports.push_back(import_string);
					}
				}
				break;

			default:
				wl_unreachable();
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

bool c_preprocessor::is_valid_preprocessor_line(c_compiler_string line) {
	size_t leading_whitespace = compiler_utility::skip_whitespace(line);
	if (leading_whitespace == line.get_length()) {
		// No preprocessor symbol at all
		return false;
	}

	// Return true if the first non-whitespace character is #
	// Don't actually validate the preprocessor line in this function
	return line[leading_whitespace] == '#';
}

static e_preprocessor_token get_preprocessor_token(c_compiler_string line, size_t &out_token_length) {
	e_preprocessor_token result = k_preprocessor_token_invalid;

	size_t identifier_length = compiler_utility::find_identifier_length(line);
	c_compiler_string identifier = line.substr(0, identifier_length);

	if (identifier == "import") {
		result = k_preprocessor_token_import;
	}
	// Add "else if" for each token type

	if (result != k_preprocessor_token_invalid) {
		out_token_length = identifier_length;
	}

	return result;
}

static bool process_import(c_compiler_string line, c_compiler_string &out_import_string) {
	// 1) Skip whitespace
	size_t leading_whitespace = compiler_utility::skip_whitespace(line);
	line = line.advance(leading_whitespace);

	// 2) Next character must be "
	if (line.get_length() == 0 || line[0] != '"') {
		return false;
	}

	line = line.advance(1);

	// 3) Read until another "
	bool found = false;
	size_t end_index;
	for (end_index = 0; end_index < line.get_length(); end_index++) {
		if (line[end_index] == '"') {
			found = true;
			break;
		}
	}

	if (!found) {
		return false;
	}

	out_import_string = line.substr(0, end_index);

	line = line.advance(end_index + 1);

	// 4) All following characters must be whitespace
	line = line.advance(compiler_utility::skip_whitespace(line));

	// The line should now be empty - if not, it means there were non-whitespace characters
	if (!line.is_empty()) {
		return false;
	}

	return true;
}