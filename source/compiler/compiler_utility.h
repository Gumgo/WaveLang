#pragma once

#include "common/common.h"

#include <string>
#include <vector>

enum class e_compiler_result {
	k_success,

	k_failed_to_open_file,
	k_failed_to_read_file,

	k_preprocessor_error,

	k_invalid_globals,

	k_lexer_error,
	k_invalid_token,

	k_unexpected_token,
	k_parser_error,

	k_missing_return_statement,
	k_extraneous_return_statement,
	k_duplicate_return_statement,
	k_statements_after_return,
	k_unassigned_output,
	k_duplicate_identifier,
	k_undeclared_identifier,
	k_missing_import,
	k_type_mismatch,
	k_named_value_expected,
	k_invalid_lhs_named_value_assignment,
	k_ambiguous_named_value_assignment,
	k_unassigned_named_value_used,
	k_incorrect_argument_count,
	k_cyclic_module_call,
	k_native_module_compile_time_call,
	k_no_entry_point,
	k_invalid_entry_point,
	k_syntax_error,

	k_constant_expected,
	k_invalid_loop_count,
	k_invalid_array_index,
	k_graph_error,

	k_count
};

struct s_compiler_source_location {
	int32 source_file_index;
	int32 line;
	int32 pos;

	inline void clear() {
		source_file_index = -1;
		line = -1;
		pos = -1;
	}
};

struct s_compiler_result {
	e_compiler_result result;
	s_compiler_source_location source_location;
	std::string message;

	inline void clear() {
		result = e_compiler_result::k_success;
		source_location.clear();
		message.clear();
	}
};

struct s_compiler_source_file {
	std::string filename;
	std::vector<char> source;
	// Bitvector indicating which source files are available to this source file
	std::vector<bool> imports;
};

struct s_compiler_context {
	std::string root_path;
	std::vector<s_compiler_source_file> source_files;
};

// This is a string based in static source data. It consists of a char pointer and a length. The string represented is
// NOT null terminated.
class c_compiler_string {
public:
	inline c_compiler_string();
	inline c_compiler_string(const char *str, size_t length);

	inline const char *get_str() const;
	inline size_t get_length() const;
	inline bool is_empty() const;
	inline bool operator==(const c_compiler_string &other) const;
	inline bool operator==(const char *other) const;
	inline bool operator!=(const c_compiler_string &other) const;
	inline bool operator!=(const char *other) const;

	inline char operator[](size_t index) const;

	// Returns a new string advanced by 'count' characters
	inline c_compiler_string advance(size_t count) const;
	inline c_compiler_string substr(size_t start, size_t length) const;

	inline std::string to_std_string() const;

private:
	const char *m_str;
	size_t m_length;
};

namespace compiler_utility {
	// Returns whether the given character is whitespace
	bool is_whitespace(char c);

	// Returns how many whitespace characters to increment
	size_t skip_whitespace(c_compiler_string str);

	size_t find_line_length(c_compiler_string str);

	// Finds the length of the identifier starting at str. Returns 0 if there is no valid identifier there.
	size_t find_identifier_length(c_compiler_string str);

	// Returns whether the given string is a valid identifier
	bool is_valid_identifier(const char *str);

	bool is_valid_start_identifier_character(char c);
	bool is_valid_identifier_character(char c);

	bool is_number(char c);

	bool is_valid_source_character(char c);

	// Attempts to resolve the first escape sequence detected in str. The initial backslash should be left off, as it is
	// assumed that it has already been detected when this function is called. If escape sequence resolution is
	// successful, the resulting character is stored in out_result (if a non-null pointer is provided) and the number of
	// characters used in the sequence is returned (e.g. \xab returns 3, \t returns 1). If no matching escape sequence
	// is found, 0 is returned.
	size_t resolve_escape_sequence(c_compiler_string str, char *out_result);

	// All escape sequences must be valid, enforced with asserts
	std::string escape_string(c_compiler_string unescaped_string);
}

#include "compiler_utility.inl"

