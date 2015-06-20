#ifndef WAVELANG_COMPILER_UTILITY_H__
#define WAVELANG_COMPILER_UTILITY_H__

#include "common\common.h"
#include <string>
#include <vector>

enum e_compiler_result {
	k_compiler_result_success,

	k_compiler_result_failed_to_open_file,
	k_compiler_result_failed_to_read_file,

	k_compiler_result_preprocessor_error,

	k_compiler_result_lexer_error,
	k_compiler_result_invalid_token,

	k_compiler_result_unexpected_token,
	k_compiler_result_parser_error,

	k_compiler_result_missing_return_statement,
	k_compiler_result_extraneous_return_statement,
	k_compiler_result_duplicate_return_statement,
	k_compiler_result_statements_after_return,
	k_compiler_result_unassigned_output,
	k_compiler_result_duplicate_identifier,
	k_compiler_result_undeclared_identifier,
	k_compiler_result_type_mismatch,
	k_compiler_result_value_expected,
	k_compiler_result_unassigned_named_value_expected,
	k_compiler_result_unassigned_named_value_used,
	k_compiler_result_incorrect_argument_count,
	k_compiler_result_cyclic_module_call,
	k_compiler_result_no_entry_point,
	k_compiler_result_invalid_entry_point,
	k_compiler_result_syntax_error,

	k_compiler_result_count
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
		result = k_compiler_result_success;
		source_location.clear();
		message.clear();
	}
};

struct s_compiler_source_file {
	std::string filename;
	std::vector<char> source;
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

	bool is_valid_start_identifier_character(char c);
	bool is_valid_identifier_character(char c);

	bool is_number(char c);

	bool is_valid_source_character(char c);
}

#include "compiler_utility.inl"

#endif // WAVELANG_COMPILER_UTILITY_H__