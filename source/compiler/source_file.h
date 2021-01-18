#pragma once

#include "common/common.h"

#include "compiler/lr_parser.h"
#include "compiler/source_location.h"
#include "compiler/token.h"

#include "execution_graph/native_module_registry.h"

#include <memory>

class c_ast_node;

// This exists because std::unique_ptr doesn't work with incomplete types by default
struct s_delete_ast_node {
	void operator()(c_ast_node *ast_node);
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
	std::unique_ptr<c_ast_node, s_delete_ast_node> ast;	// Abstract syntax tree for this file
};
