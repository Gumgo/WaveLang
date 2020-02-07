#pragma once

#include "common/common.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/AST/ASTDiagnostic.h>
#include <clang/AST/Decl.h>
#include <clang/Frontend/CompilerInstance.h>
#pragma warning(pop)

// Utility class for reporting scraper errors through clang's diagnostic mechanism
class c_scraper_diagnostic {
public:
	c_scraper_diagnostic(clang::CompilerInstance &compiler_instance);

	clang::DiagnosticBuilder error(const clang::SourceLocation &source_location, const char *message_format);

	// For convenience - takes source location from the decl
	clang::DiagnosticBuilder error(const clang::Decl *decl, const char *message_format);

private:
	clang::CompilerInstance &m_compiler_instance;
};

