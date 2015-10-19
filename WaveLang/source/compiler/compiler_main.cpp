#include "common/common.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_module_registration.h"
#include "compiler/compiler.h"
#include "execution_graph/execution_graph.h"
#include <iostream>

// $TODO move somewhere better?
static const char *k_wavelang_synth_extension = "wls";

int main(int argc, char **argv) {
	c_native_module_registry::initialize();
	register_native_modules(true);

	for (int arg = 1; arg < argc; arg++) {
		std::cout << "Compiling '" << argv[arg] << "'\n";
		c_execution_graph execution_graph;
		s_compiler_result result = c_compiler::compile(".\\", argv[arg], &execution_graph);

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

			e_execution_graph_result save_result = execution_graph.save(out_fname.c_str());
			if (save_result != k_execution_graph_result_success) {
				std::cout << "Failed to save '" << out_fname << "' (result code " << save_result << ")\n";
			}
		}
	}

	c_native_module_registry::shutdown();
	return 0;
}