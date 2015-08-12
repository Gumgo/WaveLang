#include "compiler/compiler.h"
#include "compiler/preprocessor.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/ast.h"
#include "compiler/ast_builder.h"
#include "compiler/ast_validator.h"
#include "execution_graph/execution_graph.h"
#include "compiler/execution_graph_builder.h"
#include "compiler/execution_graph_optimizer.h"
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>

static void output_error(const s_compiler_context &context, const s_compiler_result &result);

// Converts all separators to the platform-standard symbol
static void standardize_path(std::string &inout_path);

static s_compiler_result read_and_preprocess_source_file(
	s_compiler_context &context,
	size_t source_file_index);

static void fixup_source(std::vector<char> &inout_source);

s_compiler_result c_compiler::compile(const char *root_path, const char *source_filename,
	c_execution_graph *out_execution_graph) {
	wl_assert(root_path);
	wl_assert(source_filename);
	wl_assert(out_execution_graph);
	wl_assert(out_execution_graph->get_node_count() == 0);

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

	standardize_path(context.root_path);

	// List of source files we need to process
	context.source_files.push_back(s_compiler_source_file());
	{
		s_compiler_source_file &first_source_file = context.source_files.back();
		first_source_file.filename = source_filename;
		standardize_path(first_source_file.filename);
	}

	// While we keep resolving #import lines, keep processing source files
	// The list will grow as we are looping over it
	for (size_t index = 0; index < context.source_files.size(); index++) {
		result = read_and_preprocess_source_file(context, index);
		if (result.result != k_compiler_result_success) {
			output_error(context, result);
			return result;
		}
	}

	c_lexer::initialize_lexer();

	// Run the lexer on the preprocessed files
	s_lexer_output lexer_output;
	{
		std::vector<s_compiler_result> lexer_errors;
		result = c_lexer::process(context, lexer_output, lexer_errors);

		for (size_t error = 0; error < lexer_errors.size(); error++) {
			output_error(context, lexer_errors[error]);
		}

		if (result.result != k_compiler_result_success) {
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

		if (result.result != k_compiler_result_success) {
			output_error(context, result);
			return result;
		}
	}

	c_parser::shutdown_parser();

	// Build the AST from the result of the parser
	std::auto_ptr<c_ast_node> ast(c_ast_builder::build_ast(lexer_output, parser_output));

	if (result.result != k_compiler_result_success) {
		output_error(context, result);
		return result;
	}

	// Validate the AST
	{
		std::vector<s_compiler_result> syntax_errors;
		result = c_ast_validator::validate(ast.get(), syntax_errors);

		for (size_t error = 0; error < syntax_errors.size(); error++) {
			output_error(context, syntax_errors[error]);
		}

		if (result.result != k_compiler_result_success) {
			output_error(context, result);
			return result;
		}
	}

	// Build the execution graph - it should be impossible to fail during this step if we have performed proper
	// validation at all previous stages. Any unexpected conditions should be caught with an assert.
	c_execution_graph_builder::build_execution_graph(ast.get(), out_execution_graph);

	// Optimize the graph. This can fail if constant inputs don't resolve to constants.
	{
		std::vector<s_compiler_result> optimization_errors;
		result = c_execution_graph_optimizer::optimize_graph(out_execution_graph, optimization_errors);

		for (size_t error = 0; error < optimization_errors.size(); error++) {
			output_error(context, optimization_errors[error]);
		}

		if (result.result != k_compiler_result_success) {
			output_error(context, result);
			return result;
		}
	}

	wl_assert(out_execution_graph->validate());

	return result;
}

static void output_error(const s_compiler_context &context, const s_compiler_result &result) {
	// $TODO reverse-escape characters in the message
	std::cout << "COMPILE ERROR (code " << result.result << ")";
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

static void standardize_path(std::string &inout_path) {
	// $TODO Make correct symbol for platform
	for (size_t index = 0; index < inout_path.length(); index++) {
		if (inout_path[index] == '\\') {
			inout_path[index] = '/';
		}
	}
}

static s_compiler_result read_and_preprocess_source_file(
	s_compiler_context &context,
	size_t source_file_index) {
	// Initially assume success
	s_compiler_result result;
	result.clear();

	s_compiler_source_file &source_file = context.source_files[source_file_index];

	// Construct the full filename
	std::string full_filename = context.root_path + source_file.filename;

	{
		// Try to open the file and read it in
		std::ifstream source_file_in(full_filename.c_str(), std::ios::binary | std::ios::ate);

		if (!source_file_in.is_open()) {
			result.result = k_compiler_result_failed_to_open_file;
			result.source_location.source_file_index = static_cast<int32>(source_file_index);
			result.message = "Failed to open source file '" + full_filename + "'";
			return result;
		}

		std::streampos full_file_size = source_file_in.tellg();
		source_file_in.seekg(0);

		if (full_file_size > static_cast<std::streampos>(std::numeric_limits<size_t>::max())) {
			result.result = k_compiler_result_failed_to_read_file;
			result.source_location.source_file_index = static_cast<int32>(source_file_index);
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
			result.result = k_compiler_result_failed_to_read_file;
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
		result = c_preprocessor::preprocess(preprocessor_source, preprocessor_output);

		if (result.result != k_compiler_result_success) {
			result.source_location.source_file_index = static_cast<int32>(source_file_index);
			return result;
		}

		// Check to see if there are any new imports
		for (size_t index = 0; index < preprocessor_output.imports.size(); index++) {
			std::string import_path = preprocessor_output.imports[index].to_std_string();
			standardize_path(import_path);

			bool found_match = false;
			for (size_t existing_index = 0;
				 !found_match && existing_index < context.source_files.size();
				 existing_index++) {
				// Check if the strings are equal, ignoring case
				// If so, we won't add the import to the list
				found_match = string_compare_case_insensitive(
					import_path.c_str(), context.source_files[existing_index].filename.c_str());
			}

			if (!found_match) {
				// This is a new import, add it to the list
				context.source_files.push_back(s_compiler_source_file());
				s_compiler_source_file &new_source_file = context.source_files.back();
				new_source_file.filename = import_path;
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