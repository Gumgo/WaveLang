#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

#include "compiler/token.h"
#include "compiler/lr_parser.h"

#include "execution_graph/native_module_registry.h"

struct s_compiler_source_file_handle_identifier {};
using h_compiler_source_file = c_handle<s_compiler_source_file_handle_identifier, size_t>;

class c_ast_node;

struct s_compiler_source_location {
	h_compiler_source_file source_file_handle;
	int32 line;
	int32 character;

	s_compiler_source_location(
		h_compiler_source_file source_file_handle = h_compiler_source_file::invalid(),
		int32 line = -1,
		int32 character = -1) {
		this->source_file_handle = source_file_handle;
		this->line = line;
		this->character = character;
	}

	// Returns true if at least one component of the source location is valid
	bool is_valid() const {
		return source_file_handle.is_valid();
	}

	void clear() {
		source_file_handle = h_compiler_source_file::invalid();
		line = -1;
		character = -1;
	}
};

struct s_compiler_source_file_import {
	// Exactly one of these will be valid:
	h_compiler_source_file source_file_handle;
	h_native_module_library native_module_library_handle;

	// These are used in error messages
	std::string import_path_string;
	std::string import_as_string;

	// List of components making up the import-as section of the import
	std::vector<std::string> import_as_components;
};

struct s_compiler_source_file {
	std::string path;									// Resolved unique source file path
	std::vector<char> source;							// Raw source of the file
	std::vector<s_token> tokens;						// Tokens resulting from the lexer
	c_lr_parse_tree parse_tree;							// Parse tree resulting from the parser
	std::vector<s_compiler_source_file_import> imports;	// Source files or native module libraries imported by this file
	std::unique_ptr<c_ast_node> ast;					// Abstract syntax tree for this file
};
