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

// $TODO $COMPILER remove
struct s_compiler_source_file_OLD {
	std::string filename;
	std::vector<char> source;
	// Bitvector indicating which source files are available to this source file
	std::vector<bool> imports;
};

// $TODO $COMPILER remove
struct s_compiler_context_OLD {
	c_wrapped_array<void *> native_module_library_contexts;
	std::string root_path;
	std::vector<s_compiler_source_file> source_files;
};
