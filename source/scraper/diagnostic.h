#ifndef WAVELANG_SCRAPER_DIAGNOSTIC_H__
#define WAVELANG_SCRAPER_DIAGNOSTIC_H__

#include "common/common.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/AST/ASTDiagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#pragma warning(pop)

// Utility class for reporting scraper errors through clang's diagnostic mechanism
class c_diagnostic {
public:
	c_diagnostic(clang::CompilerInstance &compiler_instance);

	clang::DiagnosticBuilder error(const clang::SourceLocation &source_location, const char *message_format);

private:
	clang::CompilerInstance &m_compiler_instance;
};

#endif // WAVELANG_SCRAPER_DIAGNOSTIC_H__
