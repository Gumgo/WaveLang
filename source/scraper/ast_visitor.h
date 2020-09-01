#pragma once

#include "common/common.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/CompilerInstance.h>
#pragma warning(pop)

class c_scraper_result;

clang::ASTConsumer *create_ast_visitor(clang::CompilerInstance &compiler_instance, c_scraper_result *scraper_result);

