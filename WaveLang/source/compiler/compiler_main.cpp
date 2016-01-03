#include "common/common.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_module_registration.h"
#include "compiler/compiler.h"
#include "execution_graph/execution_graph.h"
#include "common/utility/graphviz_generator.h"
#include <iostream>
#include <getopt.h>

#if PREDEFINED(PLATFORM_WINDOWS)
#ifdef _DEBUG
#pragma comment(lib, "getopt_d.lib")
#else // _DEBUG
#pragma comment(lib, "getopt.lib")
#endif // _DEBUG
#endif // PREDEFINED(PLATFORM_WINDOWS)

// $TODO move somewhere better?
static const char *k_wavelang_synth_extension = "wls";

int main(int argc, char **argv) {
	c_native_module_registry::initialize();
	register_native_modules(true);

	// Command line options
	bool output_execution_graph = false;
	bool condense_large_arrays = false;
	bool command_line_option_error = false;
	int32 first_file_argument_index;

	{
		// Read the command line options
		int32 getopt_result;
		extern int optind;
		while ((getopt_result = getopt(argc, argv, "gG")) != -1) {
			switch (getopt_result) {
			case 'g':
				output_execution_graph = true;
				break;

			case 'G':
				output_execution_graph = true;
				condense_large_arrays = true;
				break;

			case '?':
				command_line_option_error = true;
				break;

			default:
				// Unhandled case?
				wl_unreachable();
			}
		}

		first_file_argument_index = optind;
	}

	if (first_file_argument_index >= argc) {
		command_line_option_error = true;
	}

	if (command_line_option_error) {
		std::cerr << "usage: " << argv[0] << " [-g] fname1 [fname2 ...]\n";
		return 1;
	}

	// Compile each input file
	for (int arg = first_file_argument_index; arg < argc; arg++) {
		std::cout << "Compiling '" << argv[arg] << "'\n";
		c_execution_graph execution_graph;
		s_compiler_result result = c_compiler::compile(".\\", argv[arg], &execution_graph);

		if (result.result == k_compiler_result_success) {
			std::string fname_no_ext = argv[arg];
			// Add or replace extension
			size_t last_slash = fname_no_ext.find_last_of('/');
			size_t last_backslash = fname_no_ext.find_last_of('\\');
			size_t last_separator = (last_slash == std::string::npos) ? last_backslash : last_slash;
			size_t last_dot = fname_no_ext.find_last_of('.');
			// no dot, no separator => don't remove
			// no dot, separator => don't remove
			// dot, no separator => remove
			// dot before separator => don't remove
			// dot after separator => remove
			if (last_dot != std::string::npos &&
				(last_separator == std::string::npos || last_dot > last_separator)) {
				fname_no_ext.resize(last_dot);
			}

			std::string out_fname = fname_no_ext + '.' + k_wavelang_synth_extension;
			std::cout << "Compiled '" << argv[arg] << "' successfully, saving result to '" << out_fname << "'\n";

			e_execution_graph_result save_result = execution_graph.save(out_fname.c_str());
			if (save_result != k_execution_graph_result_success) {
				std::cerr << "Failed to save '" << out_fname << "' (result code " << save_result << ")\n";
			}

			if (output_execution_graph) {
				std::string graph_fname = fname_no_ext + '.' + k_graphviz_file_extension;
				if (execution_graph.generate_graphviz_file(graph_fname.c_str(), condense_large_arrays)) {
					std::cout << "Saved execution graph to '" << graph_fname << "'\n";
				} else {
					std::cerr << "Failed to save '" << graph_fname << "'\n";
				}
			}
		} else {
			std::cerr << "Failed to compile '" << argv[arg] << "'\n";
		}
	}

	c_native_module_registry::shutdown();
	return 0;
}