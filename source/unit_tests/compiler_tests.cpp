#include "common/common.h"

#include "compiler/compiler.h"
#include "compiler/compiler_context.h"

#include "execution_graph/instrument.h"
#include "execution_graph/native_module_registration.h"
#include "execution_graph/native_module_registry.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

// Used so we can specify the expected error using its string name
static constexpr const char *k_compiler_error_strings[] = {
	"k_failed_to_find_file",
	"k_failed_to_open_file",
	"k_failed_to_read_file",
	"k_invalid_token",
	"k_unexpected_token",
	"k_self_referential_import",
	"k_failed_to_resolve_import",
	"k_unrecognized_instrument_global",
	"k_illegal_instrument_global",
	"k_invalid_instrument_global_parameters",
	"k_duplicate_instrument_global",
	"k_illegal_data_type",
	"k_type_mismatch",
	"k_illegal_type_conversion",
	"k_inconsistent_array_element_data_types",
	"k_return_type_mismatch",
	"k_illegal_for_loop_range_type",
	"k_illegal_value_data_type",
	"k_illegal_global_scope_value_data_type",
	"k_missing_global_scope_value_initializer",
	"k_illegal_out_argument",
	"k_illegal_argument_ordering",
	"k_duplicate_argument",
	"k_declaration_conflict",
	"k_unassigned_out_argument",
	"k_missing_return_statement",
	"k_identifier_resolution_not_allowed",
	"k_identifier_resolution_failed",
	"k_ambiguous_identifier_resolution",
	"k_not_callable_type",
	"k_invalid_named_argument",
	"k_too_many_arguments_provided",
	"k_duplicate_argument_provided",
	"k_argument_direction_mismatch",
	"k_missing_argument",
	"k_ambiguous_module_overload_resolution",
	"k_empty_module_overload_resolution",
	"k_invalid_out_argument",
	"k_invalid_assignment",
	"k_invalid_if_statement_data_type",
	"k_illegal_break_statement",
	"k_illegal_continue_statement",
	"k_illegal_variable_subscript_assignment",
	"k_ambiguous_entry_point",
	"k_invalid_entry_point",
	"k_incompatible_entry_points",
	"k_self_referential_constant",
	"k_module_call_depth_limit_exceeded",
	"k_array_index_out_of_bounds",
	"k_native_module_error",
	"k_invalid_native_module_implementation"
};

STATIC_ASSERT(is_enum_fully_mapped<e_compiler_error>(k_compiler_error_strings));

static constexpr const char *k_compiler_tests_directory = "compiler_tests";
static constexpr const char *k_test_extension = ".wl";

static std::string_view get_next_token(std::string_view &line) {
	size_t start_offset = 0;

	// Skip whitespace
	while (start_offset < line.length()) {
		char c = line[start_offset];
		if (c == ' ' || c == '\t') {
			start_offset++;
		} else {
			break;
		}
	}

	size_t end_offset = start_offset;
	while (end_offset < line.length()) {
		char c = line[end_offset];
		if (c == ' ' || c == '\t') {
			break;
		} else {
			end_offset++;
		}
	}

	std::string_view result = line.substr(start_offset, end_offset - start_offset);
	line = line.substr(end_offset);
	return result;
}

static void run_compiler_test(const char *test_definition_filename) {
	std::ifstream file(test_definition_filename);
	ASSERT_TRUE(file.is_open());

	std::filesystem::remove_all(k_compiler_tests_directory);
	ASSERT_TRUE(std::filesystem::create_directory(k_compiler_tests_directory));

	std::vector<std::tuple<std::string, e_compiler_error>> tests;
	std::ofstream output_file;
	std::string line;
	while (std::getline(file, line)) {
		if (line.starts_with("###")) {
			std::string_view line_remaining = line;

			get_next_token(line_remaining); // Skip ###
			std::string_view command = get_next_token(line_remaining);
			if (command == "FILE") {
				std::string_view path_string = get_next_token(line_remaining);
				// The rest of the line can be a comment

				std::filesystem::path path(path_string);
				ASSERT_TRUE(path.has_extension());

				// Create each subdirectory
				std::filesystem::path current_path(k_compiler_tests_directory);
				for (auto path_element : path) {
					if (!path_element.has_extension()) {
						if (!std::filesystem::exists(current_path / path_element)) {
							ASSERT_TRUE(std::filesystem::create_directory(current_path / path_element));
						}
					}
				}

				// Create and open the file
				if (output_file.is_open()) {
					output_file.close();
				}

				output_file.open(std::filesystem::path(k_compiler_tests_directory) / path);
				ASSERT_TRUE(output_file.is_open());
			} else if (command == "TEST") {
				std::string_view test_name = get_next_token(line_remaining);
				std::string_view expected_result_string = get_next_token(line_remaining);
				// The rest of the line can be a comment

				e_compiler_error expected_result = e_compiler_error::k_invalid;
				if (expected_result_string == "success") {
					// We expect compilation to succeed
				} else {
					for (e_compiler_error compiler_error : iterate_enum<e_compiler_error>()) {
						if (expected_result_string == k_compiler_error_strings[enum_index(compiler_error)]) {
							expected_result = compiler_error;
							break;
						}
					}

					ASSERT_NE(expected_result, e_compiler_error::k_invalid);
					ASSERT_TRUE(output_file.is_open());
				}

				tests.push_back(std::make_tuple(std::string(test_name), expected_result));

				// Create and open the test file
				if (output_file.is_open()) {
					output_file.close();
				}

				output_file.open(
					std::filesystem::path(k_compiler_tests_directory) / (std::string(test_name) + k_test_extension));
			} else {
				FAIL();
			}

			if (output_file.is_open()) {
				output_file.close();
			}
		} else if (output_file.is_open()) {
			output_file << line << std::endl;
			ASSERT_FALSE(output_file.fail());
		} else {
			// No file open yet, only allow empty lines
			ASSERT_TRUE(line.empty());
		}
	}

	if (output_file.is_open()) {
		output_file.close();
	}

	// Run each test
	for (const std::tuple<std::string, e_compiler_error> &test : tests) {
		const std::string &test_name = std::get<0>(test);
		e_compiler_error expected_result = std::get<1>(test);

		std::filesystem::path test_path =
			std::filesystem::path(k_compiler_tests_directory) / (test_name + k_test_extension);

		bool test_passed = true;
		std::string error_message;

		{
			c_native_module_registry::initialize();
			register_native_modules();

			std::vector<void *> library_contexts(
				c_native_module_registry::get_native_module_library_count(),
				nullptr);
			for (h_native_module_library library_handle : c_native_module_registry::iterate_native_module_libraries()) {
				const s_native_module_library &library =
					c_native_module_registry::get_native_module_library(library_handle);
				if (library.compiler_initializer) {
					library_contexts[library_handle.get_data()] = library.compiler_initializer();
				}
			}

			c_compiler_context context = c_compiler_context(library_contexts);
			std::unique_ptr<c_instrument> result(c_compiler::compile(context, test_path.string().c_str()));

			for (h_native_module_library library_handle : c_native_module_registry::iterate_native_module_libraries()) {
				const s_native_module_library &library =
					c_native_module_registry::get_native_module_library(library_handle);
				if (library.compiler_deinitializer) {
					library.compiler_deinitializer(library_contexts[library_handle.get_data()]);
				}
			}

			c_native_module_registry::shutdown();

			if (expected_result == e_compiler_error::k_invalid && !result) {
				test_passed = false;
				error_message = "expected success but compilation failed";
			} else if (expected_result != e_compiler_error::k_invalid) {
				if (result) {
					test_passed = false;
					error_message = "expected error '"
						+ std::string(k_compiler_error_strings[enum_index(expected_result)])
						+ "' but compilation succeeded";
				} else {
					bool found = false;
					for (size_t i = 0; i < context.get_error_count(); i++) {
						if (context.get_error(i).error == expected_result) {
							found = true;
							break;
						}
					}

					if (!found) {
						test_passed = false;
						error_message = "expected error '"
							+ std::string(k_compiler_error_strings[enum_index(expected_result)])
							+ "' but it did not occur despite compilation failing";
					}
				}
			}

		}

		// This outputs test name and error message only if we failed
		EXPECT_TRUE(test_passed) << " (" << test_name << ", " << error_message << ")";
	}
}

TEST(Compiler, Thing) {

}
