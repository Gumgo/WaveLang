#include "common/utility/file_utility.h"

#include "compiler/ast.h"
#include "compiler/ast_builder.h"
#include "compiler/ast_validator.h"
#include "compiler/compiler.h"
#include "compiler/execution_graph_builder.h"
#include "compiler/execution_graph_optimizer.h"
#include "compiler/instrument_globals_parser.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/preprocessor.h"

#include "execution_graph/instrument.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

static void output_error(const s_compiler_context &context, const s_compiler_result &result);

static s_compiler_result read_and_preprocess_source_file(
	s_compiler_context &context,
	size_t source_file_index);

static void fixup_source(std::vector<char> &inout_source);

s_compiler_result c_compiler::compile(
	const char *root_path,
	const char *source_filename,
	c_instrument *out_instrument) {
	wl_assert(root_path);
	wl_assert(source_filename);
	wl_assert(out_instrument);
	wl_assert(out_instrument->get_instrument_variant_count() == 0);

	s_compiler_result result;
	result.clear();

	s_compiler_context context;
	// Fix up the root path by adding a backslash if necessary
	context.root_path = root_path;

	if (!context.root_path.empty() &&
		context.root_path.back() != '/' &&
		context.root_path.back() != '\\') {
		context.root_path.push_back('/');
	}

	// List of source files we need to process
	context.source_files.push_back(s_compiler_source_file());
	{
		s_compiler_source_file &first_source_file = context.source_files.back();
		first_source_file.filename = source_filename;

		// This source file is available as an "import" to itself
		first_source_file.imports.push_back(true);
	}

	// We need to initialize the lexer early because it is used by the preprocessor
	c_lexer::initialize_lexer();
	c_preprocessor::initialize_preprocessor();

	s_instrument_globals_context instrument_globals_context;

	{
		instrument_globals_context.clear();

		c_instrument_globals_parser::register_preprocessor_commands(&instrument_globals_context);

		// While we keep resolving #import lines, keep processing source files
		// The list will grow as we are looping over it
		for (size_t index = 0; index < context.source_files.size(); index++) {
			result = read_and_preprocess_source_file(context, index);
			if (result.result != e_compiler_result::k_success) {
				output_error(context, result);
				return result;
			}
		}

		instrument_globals_context.assign_defaults();
	}

	c_preprocessor::shutdown_preprocessor();

	// Run the lexer on the preprocessed files
	s_lexer_output lexer_output;
	{
		std::vector<s_compiler_result> lexer_errors;
		result = c_lexer::process(context, lexer_output, lexer_errors);

		for (size_t error = 0; error < lexer_errors.size(); error++) {
			output_error(context, lexer_errors[error]);
		}

		if (result.result != e_compiler_result::k_success) {
			output_error(context, result);
			return result;
		}
	}

	c_lexer::shutdown_lexer();

	c_parser::initialize_parser();

	// Run the parser on the lexer's output
	s_parser_output parser_output;
	{
		std::vector<s_compiler_result> parser_errors;
		result = c_parser::process(context, lexer_output, parser_output, parser_errors);

		for (size_t error = 0; error < parser_errors.size(); error++) {
			output_error(context, parser_errors[error]);
		}

		if (result.result != e_compiler_result::k_success) {
			output_error(context, result);
			return result;
		}
	}

	c_parser::shutdown_parser();

	// Build the AST from the result of the parser
	std::unique_ptr<c_ast_node> ast(c_ast_builder::build_ast(lexer_output, parser_output));

	if (result.result != e_compiler_result::k_success) {
		output_error(context, result);
		return result;
	}

	// Validate the AST
	{
		std::vector<s_compiler_result> syntax_errors;
		result = c_ast_validator::validate(&context, ast.get(), syntax_errors);

		for (size_t error = 0; error < syntax_errors.size(); error++) {
			output_error(context, syntax_errors[error]);
		}

		if (result.result != e_compiler_result::k_success) {
			output_error(context, result);
			return result;
		}
	}

	// Loop over each instance of instrument globals and build an instrument variant
	{
		std::vector<s_instrument_globals> instrument_globals_set =
			instrument_globals_context.build_instrument_globals_set();

		for (size_t index = 0; index < instrument_globals_set.size(); index++) {
			c_instrument_variant *instrument_variant = new c_instrument_variant();

			// Add to the instrument immediately to prevent memory leaks
			out_instrument->add_instrument_variant(instrument_variant);

			// Store the globals in the instrument variant so they are accessible in future compilation phases
			instrument_variant->set_instrument_globals(instrument_globals_set[index]);

			// Build the execution graphs. This can fail if loops don't resolve to constants.
			{
				std::vector<s_compiler_result> graph_errors;
				result = c_execution_graph_builder::build_execution_graphs(ast.get(), instrument_variant, graph_errors);

				for (size_t error = 0; error < graph_errors.size(); error++) {
					output_error(context, graph_errors[error]);
				}

				if (result.result != e_compiler_result::k_success) {
					output_error(context, result);
					return result;
				}
			}

			// Optimize the graphs. This can fail if constant inputs don't resolve to constants.

			if (instrument_variant->get_voice_execution_graph()) {
				std::vector<s_compiler_result> optimization_errors;
				result = c_execution_graph_optimizer::optimize_graph(
					instrument_variant->get_voice_execution_graph(),
					&instrument_variant->get_instrument_globals(),
					optimization_errors);

				for (size_t error = 0; error < optimization_errors.size(); error++) {
					output_error(context, optimization_errors[error]);
				}

				if (result.result != e_compiler_result::k_success) {
					output_error(context, result);
					return result;
				}
			}

			if (instrument_variant->get_fx_execution_graph()) {
				std::vector<s_compiler_result> optimization_errors;
				result = c_execution_graph_optimizer::optimize_graph(
					instrument_variant->get_fx_execution_graph(),
					&instrument_variant->get_instrument_globals(),
					optimization_errors);

				for (size_t error = 0; error < optimization_errors.size(); error++) {
					output_error(context, optimization_errors[error]);
				}

				if (result.result != e_compiler_result::k_success) {
					output_error(context, result);
					return result;
				}
			}
		}
	}

	wl_assert(out_instrument->validate());

	return result;
}

static void output_error(const s_compiler_context &context, const s_compiler_result &result) {
	// $TODO reverse-escape characters in the message
	std::cout << "COMPILE ERROR (code " << enum_index(result.result) << ")";
	if (result.source_location.source_file_index >= 0) {
		std::cout << " in file " << context.source_files[result.source_location.source_file_index].filename;

		if (result.source_location.line >= 0) {
			std::cout << " (" << result.source_location.line;

			if (result.source_location.pos >= 0) {
				std::cout << ", " << result.source_location.pos;
			}

			std::cout << ")";
		}
	}

	std::cout << ": " << result.message << "\n";
}

static s_compiler_result read_and_preprocess_source_file(
	s_compiler_context &context,
	size_t source_file_index) {
	// Initially assume success
	s_compiler_result result;
	result.clear();

	s_compiler_source_file &source_file = context.source_files[source_file_index];

	// Construct the full filename
	std::string full_filename;
	if (is_path_relative(source_file.filename.c_str())) {
		full_filename = context.root_path + source_file.filename;
	} else {
		full_filename = source_file.filename;
	}

	{
		// Try to open the file and read it in
		std::ifstream source_file_in(full_filename.c_str(), std::ios::binary | std::ios::ate);

		if (!source_file_in.is_open()) {
			result.result = e_compiler_result::k_failed_to_open_file;
			result.source_location.source_file_index = cast_integer_verify<int32>(source_file_index);
			result.message = "Failed to open source file '" + full_filename + "'";
			return result;
		}

		std::streampos full_file_size = source_file_in.tellg();
		source_file_in.seekg(0);

		// cast_integer_verify doesn't work with std::streampos
		if (full_file_size > static_cast<std::streampos>(std::numeric_limits<int32>::max())) {
			result.result = e_compiler_result::k_failed_to_read_file;
			result.source_location.source_file_index = cast_integer_verify<int32>(source_file_index);
			result.message = "Source file '" + full_filename + "' is too big";
			return result;
		}

		size_t file_size = static_cast<size_t>(full_file_size);
		wl_assert(static_cast<std::streampos>(file_size) == full_file_size);

		source_file.source.resize(file_size);
		if (file_size > 0) {
			source_file_in.read(&source_file.source.front(), file_size);
		}

		if (source_file_in.fail()) {
			result.result = e_compiler_result::k_failed_to_read_file;
			result.source_location.source_file_index = static_cast<int32>(source_file_index);
			result.message = "Failed to read source file '" + full_filename + "'";
			return result;
		}
	}

	fixup_source(source_file.source);

	// Run the preprocessor
	if (!source_file.source.empty()) {
		c_compiler_string preprocessor_source(&source_file.source.front(), source_file.source.size());
		c_preprocessor_output preprocessor_output;
		result = c_preprocessor::preprocess(
			preprocessor_source, static_cast<int32>(source_file_index), preprocessor_output);

		if (result.result != e_compiler_result::k_success) {
			result.source_location.source_file_index = cast_integer_verify<int32>(source_file_index);
			return result;
		}

		// Check to see if there are any new imports
		for (size_t index = 0; index < preprocessor_output.imports.size(); index++) {
			const std::string &import_path = preprocessor_output.imports[index];
			std::string full_import_path;
			if (is_path_relative(import_path.c_str())) {
				full_import_path = context.root_path + import_path;
			} else {
				full_import_path = import_path;
			}

			bool found_match = false;
			for (size_t existing_index = 0;
				!found_match && existing_index < context.source_files.size();
				existing_index++) {
				// Check if the paths point to the same file
				// If so, we won't add the import to the list
				std::string full_existing_path;
				if (is_path_relative(context.source_files[existing_index].filename.c_str())) {
					full_existing_path = context.root_path + context.source_files[existing_index].filename;
				} else {
					full_existing_path = context.source_files[existing_index].filename;
				}
				found_match = are_file_paths_equivalent(
					full_import_path.c_str(),
					full_existing_path.c_str());

				if (found_match) {
					// This source file is importing the matching source file
					context.source_files[source_file_index].imports[existing_index] = true;
				}
			}

			if (!found_match) {
				// This is a new import, add it to the list
				context.source_files.push_back(s_compiler_source_file());
				s_compiler_source_file &new_source_file = context.source_files.back();
				new_source_file.filename = import_path;
				new_source_file.imports.resize(context.source_files.size() - 1, false);

				// Add a new import entry to all source files
				for (size_t add_import_index = 0; add_import_index < context.source_files.size(); add_import_index++) {
					context.source_files[add_import_index].imports.push_back(false);
				}

				// This source file, and the new import itself, have import access
				context.source_files[source_file_index].imports.back() = true;
				context.source_files.back().imports.back() = true;
			}
		}
	}

	return result;
}

static void fixup_source(std::vector<char> &inout_source) {
	// Fixup the file. For simplicity, just remove all \r symbols
	size_t read_index = 0;
	size_t write_index = 0;
	while (read_index < inout_source.size()) {
		if (inout_source[read_index] != '\r') {
			inout_source[write_index] = inout_source[read_index];
			write_index++;
		}

		read_index++;
	}

	inout_source.resize(write_index);
}
