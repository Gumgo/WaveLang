#include "common\common.h"
#include "compiler\compiler.h"
#include "execution_graph\execution_graph.h"
#include <iostream>

// $TODO move somewhere better?
static const char *k_wavelang_synth_extension = "wls";

int main(int argc, char **argv) {
	for (int arg = 0; arg < argc; arg++) {
		std::cout << "Compiling '" << argv[arg] << "'\n";
		c_execution_graph execution_graph;
		s_compiler_result result = c_compiler::compile(".\\", "synth_1.txt", &execution_graph);

		if (result.result == k_compiler_result_success) {
			std::string out_fname = argv[arg];
			// Add or replace extension
			size_t last_slash = out_fname.find_last_of('/');
			size_t last_backslash = out_fname.find_last_of('\\');
			size_t last_separator = (last_slash == std::string::npos) ? last_backslash : last_slash;
			size_t last_dot = out_fname.find_last_of('.');
			// no dot, no separator => don't remove
			// no dot, separator => don't remove
			// dot, no separator => remove
			// dot before separator => don't remove
			// dot after separator => remove
			if (last_dot != std::string::npos &&
				(last_separator == std::string::npos || last_dot > last_separator)) {
				out_fname.resize(last_dot);
			}

			out_fname += '.';
			out_fname += k_wavelang_synth_extension;

			std::cout << "Compiled '" << argv[arg] << "' successfully, saving result to '" << out_fname << "'\n";
		}
	}

	return 0;
}