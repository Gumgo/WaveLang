#pragma once

#include "common/common.h"

#include "compiler/source_file.h"

#include <iostream>
#include <vector>

enum class e_compiler_warning {
	k_entry_point_argument_initializer_ignored,
	k_native_module_warning,

	k_count,
};

enum class e_compiler_error {
	// File errors
	k_failed_to_find_file,
	k_failed_to_open_file,
	k_failed_to_read_file,

	// Lexer/parser errors
	k_invalid_token,
	k_unexpected_token,

	// Import errors
	k_self_referential_import,
	k_failed_to_resolve_import,

	// Instrument globals errors
	k_unrecognized_instrument_global,
	k_illegal_instrument_global,
	k_invalid_instrument_global_parameters,
	k_duplicate_instrument_global,

	// Data type errors
	k_illegal_data_type,
	k_type_mismatch,
	k_illegal_type_conversion,
	k_inconsistent_array_element_data_types,
	k_return_type_mismatch,
	k_illegal_for_loop_range_type,

	// Value declaration errors
	k_illegal_value_data_type,
	k_illegal_global_scope_value_data_type,
	k_missing_global_scope_value_initializer,

	// Module declaration errors
	k_illegal_out_argument,
	k_illegal_argument_ordering,
	k_duplicate_argument,
	k_declaration_conflict,
	k_unassigned_out_argument,
	k_missing_return_statement,

	// Identifier resolution errors
	k_identifier_resolution_not_allowed,
	k_identifier_resolution_failed,
	k_ambiguous_identifier_resolution,

	// Module call errors
	k_not_callable_type,
	k_invalid_named_argument,
	k_too_many_arguments_provided,
	k_duplicate_argument_provided,
	k_argument_direction_mismatch,
	k_missing_argument,
	k_ambiguous_module_overload_resolution,
	k_empty_module_overload_resolution,
	k_invalid_out_argument,

	// Statement errors
	k_invalid_assignment,
	k_invalid_if_statement_data_type,
	k_illegal_break_statement,
	k_illegal_continue_statement,
	k_illegal_variable_subscript_assignment,

	// Entry point errors
	k_ambiguous_entry_point,
	k_invalid_entry_point,
	k_incompatible_entry_points,

	// Evaluation errors
	k_self_referential_constant,
	k_module_call_depth_limit_exceeded,
	k_array_index_out_of_bounds,
	k_native_module_error,
	k_invalid_native_module_implementation,

	k_count
};

class c_compiler_context {
public:
	c_compiler_context(c_wrapped_array<void *> native_module_library_contexts);

	void message(
		const s_compiler_source_location &location,
		const char *format,
		...);
	void vmessage(
		const s_compiler_source_location &location,
		const char *format,
		va_list args);
	void message(
		const char *format,
		...);
	void vmessage(
		const char *format,
		va_list args);
	void warning(
		e_compiler_warning warning,
		const s_compiler_source_location &location,
		const char *format, ...);
	void vwarning(
		e_compiler_warning warning,
		const s_compiler_source_location &location,
		const char *format,
		va_list args);
	void warning(
		e_compiler_warning warning,
		const char *format,
		...);
	void vwarning(
		e_compiler_warning warning,
		const char *format,
		va_list args);
	void error(
		e_compiler_error error,
		const s_compiler_source_location &location,
		const char *format,
		...);
	void verror(
		e_compiler_error error,
		const s_compiler_source_location &location,
		const char *format,
		va_list args);
	void error(
		e_compiler_error error,
		const char *format, ...);
	void verror(
		e_compiler_error error,
		const char *format,
		va_list args);

	size_t get_warning_count() const;
	size_t get_error_count() const;

	size_t get_source_file_count() const;
	s_compiler_source_file &get_source_file(h_compiler_source_file handle);
	const s_compiler_source_file &get_source_file(h_compiler_source_file handle) const;
	h_compiler_source_file get_or_add_source_file(const char *path, bool &was_added_out);

	void *get_native_module_library_context(h_native_module_library native_module_library_handle);

private:
	void output_to_stream(
		std::ostream &stream,
		const char *prefix,
		int32 code,
		const s_compiler_source_location &location,
		const char *format,
		va_list args) const;
	std::string get_source_location_string(const s_compiler_source_location &location) const;

	c_wrapped_array<void *> m_native_module_library_contexts;

	// We use unique_ptr to make sure that existing source file references remain valid when a new source file is added
	std::vector<std::unique_ptr<s_compiler_source_file>> m_source_files;

	size_t m_warning_count;
	size_t m_error_count;
};