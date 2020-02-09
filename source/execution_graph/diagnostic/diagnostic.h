#pragma once

#include "common/common.h"

#include <string>

enum class e_diagnostic_level {
	k_info,
	k_warning,
	k_error,

	k_count
};

using f_diagnostic_callback = void (*)(void *context, e_diagnostic_level diagnostic_level, const std::string &message);

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

