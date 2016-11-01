#include "scraper/diagnostic.h"

c_diagnostic::c_diagnostic(clang::CompilerInstance &compiler_instance)
	: m_compiler_instance(compiler_instance) {
}

clang::DiagnosticBuilder c_diagnostic::error(const clang::SourceLocation &source_location, const char *message_format) {
	// This will either create a new diagnostic ID or return an existing one based on a hash of message_format
	unsigned diagnostic_id = m_compiler_instance.getDiagnostics().getDiagnosticIDs()->getCustomDiagID(
		clang::DiagnosticIDs::Error, message_format);

	// Report the diagnostic and return an object we can use the << operator on
	return m_compiler_instance.getDiagnostics().Report(source_location, diagnostic_id);
}

clang::DiagnosticBuilder c_diagnostic::error(const clang::Decl *decl, const char *message_format) {
	wl_assert(decl);
	return error(decl->getSourceRange().getBegin(), message_format);
}
