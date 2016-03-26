#include "compiler/preprocessor.h"
#include "compiler/lexer.h"

struct s_preprocessor_command_executor {
	const char *command_name;
	void *context;
	f_preprocessor_command_executor command_executor;
};

static const size_t k_max_preprocessor_command_executors = 8;

static s_preprocessor_command_executor g_preprocessor_command_executors[k_max_preprocessor_command_executors];
static size_t g_preprocessor_command_executor_count = 0;

static s_compiler_result preprocessor_command_import(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

void c_preprocessor::initialize_preprocessor() {
	g_preprocessor_command_executor_count = 0;

	// Register the "native" preprocessor commands. Currently the only one is #import.
	register_preprocessor_command("import", nullptr, preprocessor_command_import);
}

void c_preprocessor::shutdown_preprocessor() {
	g_preprocessor_command_executor_count = 0;
}

// Registers a callback to be called when a preprocessor token with the provided name is encountered. Note that
// command_name should have static lifetime as the string is not copied internally upon registration.
void c_preprocessor::register_preprocessor_command(const char *command_name,
	void *context, f_preprocessor_command_executor command_executor) {
	wl_vassert(g_preprocessor_command_executor_count < k_max_preprocessor_command_executors,
		"Too many preprocessor commands");

#ifdef ASSERTS_ENABLED
	// Validate the name
	wl_assert(command_name);
	{
		const char *ch = command_name;
		wl_assert(*ch != '\0');
		wl_assert(compiler_utility::is_valid_start_identifier_character(*ch));
		ch++;
		while (*ch != '\0') {
			wl_assert(compiler_utility::is_valid_identifier_character(*ch));
			ch++;
		}
	}

	// Make sure the command name is unique
	for (size_t index = 0; index < g_preprocessor_command_executor_count; index++) {
		wl_vassert(strcmp(g_preprocessor_command_executors[index].command_name, command_name) != 0,
			"Duplicate preprocessor command");
	}
#endif // ASSERTS_ENABLED

	s_preprocessor_command_executor &executor =
		g_preprocessor_command_executors[g_preprocessor_command_executor_count];
	executor.command_name = command_name;
	executor.context = context;
	executor.command_executor = command_executor;

	g_preprocessor_command_executor_count++;
}

s_compiler_result c_preprocessor::preprocess(
	c_compiler_string source, int32 source_file_index, c_preprocessor_output &output) {
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
			size_t leading_whitespace = compiler_utility::skip_whitespace(line);
			int32 pos = static_cast<int32>(leading_whitespace);
			c_compiler_string preprocessor_line = line.advance(leading_whitespace);
			wl_assert(preprocessor_line[0] == '#');

			preprocessor_line = preprocessor_line.advance(1);
			pos++;

			c_preprocessor_command_arguments arguments;
			c_compiler_string line_remaining = preprocessor_line;
			bool eol = false;

			bool preprocessor_command_found = false;
			s_token preprocessor_command_token;

			// Keep reading tokens until we reach the end of the line or a comment symbol
			while (!eol) {
				if (preprocessor_command_found) {
					// Only skip whitespace if we're not reading the initial token for the command. We expect this to
					// take the form #command, so any whitespace following the # is an error.
					size_t whitespace = compiler_utility::skip_whitespace(line_remaining);
					line_remaining = line_remaining.advance(whitespace);
					pos += cast_integer_verify<int32>(whitespace);
				}

				if (line_remaining.is_empty()) {
					eol = true;
				} else {
					s_token token = c_lexer::read_next_token(line_remaining);

					if (!preprocessor_command_found &&
						((token.token_type >= k_token_type_first_keyword &&
						  token.token_type <= k_token_type_last_keyword) ||
						 token.token_type == k_token_type_identifier)) {
						// Detect any identifier, keywords shouldn't matter in this context
						preprocessor_command_token = token;
						preprocessor_command_found = true;
					} else if (preprocessor_command_found &&
						(token.token_type == k_token_type_constant_real ||
						 token.token_type == k_token_type_constant_bool ||
						 token.token_type == k_token_type_constant_string)) {
						// Only these token types are allowed in preprocessor commands
						token.source_location.source_file_index = source_file_index;
						token.source_location.line = line_number;
						token.source_location.pos = pos;
						arguments.add_argument(token);
					} else if (token.token_type == k_token_type_comment) {
						// Ignore the rest of the line because it is a comment
						eol = true;
					} else {
						// Report the error using the string returned
						result.result = k_compiler_result_preprocessor_error;
						result.source_location.source_file_index = source_file_index;
						result.source_location.line = line_number;
						result.source_location.pos = pos;
						result.message = "Invalid token '" + token.token_string.to_std_string() + "'";
						return result;
					}

					line_remaining = line_remaining.advance(token.token_string.get_length());
					if (token.token_type == k_token_type_constant_string) {
						// We need to also skip the quotes because they're not included in the string
						line_remaining = line_remaining.advance(2);
					}

					pos += cast_integer_verify<int32>(token.token_string.get_length());
				}
			}

			wl_assert(preprocessor_command_found == !preprocessor_command_token.token_string.is_empty());
			if (!preprocessor_command_found) {
				result.result = k_compiler_result_preprocessor_error;
				result.source_location.source_file_index = source_file_index;
				result.source_location.line = line_number;
				result.source_location.pos = pos;
				result.message = "Preprocessor command not provided";
				return result;
			}

			arguments.set_command(preprocessor_command_token);

			// Look up the preprocessor command
			size_t command_index;
			for (command_index = 0; command_index < g_preprocessor_command_executor_count; command_index++) {
				const s_preprocessor_command_executor &executor = g_preprocessor_command_executors[command_index];

				if (preprocessor_command_token.token_string == executor.command_name) {
					s_compiler_result execution_result = executor.command_executor(executor.context, arguments, output);
					if (execution_result.result != k_compiler_result_success) {
						return execution_result;
					}

					break;
				}
			}

			if (command_index == g_preprocessor_command_executor_count) {
				// We failed to find a matching command
				result.result = k_compiler_result_preprocessor_error;
				result.source_location = preprocessor_command_token.source_location;
				result.message = "Preprocessor command '" +
					preprocessor_command_token.token_string.to_std_string() + "' not found";
				return result;
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

static s_compiler_result preprocessor_command_import(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_compiler_result result;
	result.clear();

	if (arguments.get_argument_count() != 1 ||
		arguments.get_argument(0).token_type != k_token_type_constant_string) {
		result.result = k_compiler_result_preprocessor_error;
		result.source_location = arguments.get_command().source_location;
		result.message = "Invalid import command";
		return result;
	}

	std::string escaped_import_path = compiler_utility::escape_string(arguments.get_argument(0).token_string);
	output.imports.push_back(escaped_import_path);
	return result;
}
