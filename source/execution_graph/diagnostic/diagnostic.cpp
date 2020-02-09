#include "execution_graph/diagnostic/diagnostic.h"

c_diagnostic::c_diagnostic(f_diagnostic_callback diagnostic_callback, void *diagnostic_callback_context) {
	wl_assert(diagnostic_callback);
	m_diagnostic_callback = diagnostic_callback;
	m_diagnostic_callback_context = diagnostic_callback_context;
}

void c_diagnostic::info(const char *message) {
	wl_assert(message);
	m_diagnostic_callback(m_diagnostic_callback_context, e_diagnostic_level::k_info, message);
}

void c_diagnostic::warning(const char *message) {
	wl_assert(message);
	m_diagnostic_callback(m_diagnostic_callback_context, e_diagnostic_level::k_warning, message);
}

void c_diagnostic::error(const char *message) {
	wl_assert(message);
	m_diagnostic_callback(m_diagnostic_callback_context, e_diagnostic_level::k_error, message);
}
