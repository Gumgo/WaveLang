#ifndef WAVELANG_COMPILER_PREPROCESSOR_H__
#define WAVELANG_COMPILER_PREPROCESSOR_H__

#include "common/common.h"

#include "compiler/compiler_utility.h"
#include "compiler/lexer.h"

#include <vector>

struct c_preprocessor_output {
	// These are std::string and not c_compiler_string because we must resolve escape sequences
	std::vector<std::string> imports;
};

class c_preprocessor_command_arguments {
public:
	void set_command(const s_token &command) {
		m_command = command;
	}

	void add_argument(const s_token &token) {
		m_arguments.push_back(token);
	}

	s_token get_command() const {
		return m_command;
	}

	size_t get_argument_count() const {
		return m_arguments.size();
	}

	const s_token &get_argument(size_t index) const {
		return m_arguments[index];
	}

private:
	s_token m_command;
	std::vector<s_token> m_arguments;
};

// Callback for executing a preprocessor command. Should return an error if something goes wrong.
typedef s_compiler_result (*f_preprocessor_command_executor)(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

class c_preprocessor {
public:
	static void initialize_preprocessor();
	static void shutdown_preprocessor();

	// Registers a callback to be called when a preprocessor token with the provided name is encountered. Note that
	// command_name should have static lifetime as the string is not copied internally upon registration.
	static void register_preprocessor_command(const char *command_name,
		void *context, f_preprocessor_command_executor command_executor);

	static s_compiler_result preprocess(
		c_compiler_string source, int32 source_file_index, c_preprocessor_output &output);
	static bool is_valid_preprocessor_line(c_compiler_string line);
};

#endif // WAVELANG_COMPILER_PREPROCESSOR_H__
