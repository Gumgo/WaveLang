#ifndef WAVELANG_EXECUTION_GRAPH_DIAGNOSTIC_DIAGNOSTIC_H__
#define WAVELANG_EXECUTION_GRAPH_DIAGNOSTIC_DIAGNOSTIC_H__

#include "common/common.h"

#include <string>

enum e_diagnostic_level {
	k_diagnostic_level_info,
	k_diagnostic_level_warning,
	k_diagnostic_level_error,

	k_diagnostic_level_count
};

typedef void (*f_diagnostic_callback)(void *context, e_diagnostic_level diagnostic_level, const std::string &message);

class c_diagnostic {
public:
	c_diagnostic(f_diagnostic_callback diagnostic_callback, void *diagnostic_callback_context);

	void info(const char *message);
	void warning(const char *message);
	void error(const char *message);

private:
	f_diagnostic_callback m_diagnostic_callback;
	void *m_diagnostic_callback_context;
};

#endif // WAVELANG_EXECUTION_GRAPH_DIAGNOSTIC_DIAGNOSTIC_H__
