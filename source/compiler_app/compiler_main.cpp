#include "common/common.h"
#include "common/math/floating_point.h"
#include "common/utility/graphviz_generator.h"

#include "compiler/compiler.h"

#include "compiler_app/getopt/getopt.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/instrument.h"
#include "execution_graph/native_module_registration.h"
#include "execution_graph/native_module_registry.h"

#include <iostream>

// $TODO move somewhere better?
static constexpr const char *k_wavelang_synth_extension = "wls";

static constexpr const char *k_documentation_filename = "registered_native_modules.txt";

int main(int argc, char **argv) {
	int32 result = 0;

	initialize_floating_point_behavior();

	c_native_module_registry::initialize();
	register_native_modules();

	std::vector<void *> library_contexts(c_native_module_registry::get_native_module_library_count(), nullptr);
	for (h_native_module_library library_handle : c_native_module_registry::iterate_native_module_libraries()) {
		const s_native_module_library &library = c_native_module_registry::get_native_module_library(library_handle);
		if (library.compiler_initializer) {
			library_contexts[library_handle.get_data()] = library.compiler_initializer();
		}
	}

	// Command line options
	bool output_documentation = false;
	bool output_execution_graph = false;
	bool condense_large_arrays = false;
	bool command_line_option_error = false;
	int32 first_file_argument_index;

	{
		// Read the command line options
		int32 getopt_result;
		extern int optind;
		while ((getopt_result = getopt(argc, argv, const_cast<char *>("dgG"))) != -1) {
			switch (getopt_result) {
			case 'd':
				output_documentation = true;
				break;

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

	if (first_file_argument_index >= argc
		&& !output_documentation) { // No compilation necessary if we're outputting documentation
		command_line_option_error = true;
	}

	if (command_line_option_error) {
		std::cerr << "usage: " << argv[0] << " [-d] [-g] [-G] fname1 [fname2 ...]\n";
		return 1;
	}

	if (output_documentation) {
		c_native_module_registry::output_registered_native_modules(k_documentation_filename);
	}

	// Compile each input file
	for (int32 arg = first_file_argument_index; arg < argc; arg++) {
		std::cout << "Compiling '" << argv[arg] << "'\n";
		c_compiler_context context = c_compiler_context(c_wrapped_array<void *>(library_contexts));
		std::unique_ptr<c_instrument> instrument(c_compiler::compile(context, argv[arg]));

		if (instrument) {
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
			if (last_dot != std::string::npos
				&& (last_separator == std::string::npos || last_dot > last_separator)) {
				fname_no_ext.resize(last_dot);
			}

			std::string out_fname = fname_no_ext + '.' + k_wavelang_synth_extension;
			std::cout << "Compiled '" << argv[arg] << "' successfully, saving result to '" << out_fname << "'\n";

			e_instrument_result save_result = instrument->save(out_fname.c_str());
			if (save_result != e_instrument_result::k_success) {
				std::cerr << "Failed to save '" << out_fname << "' (result code " << enum_index(save_result) << ")\n";
			}

			if (output_execution_graph) {
				for (uint32 variant_index = 0;
					variant_index < instrument->get_instrument_variant_count();
					variant_index++) {
					const c_instrument_variant *instrument_variant = instrument->get_instrument_variant(variant_index);

					const c_execution_graph *execution_graphs[] = {
						instrument_variant->get_voice_execution_graph(),
						instrument_variant->get_fx_execution_graph()
					};

					static constexpr const char *k_instrument_stages[] = {
						"voice",
						"fx"
					};

					for (size_t type = 0; type < array_count(execution_graphs); type++) {
						std::string graph_fname = fname_no_ext + '.' + std::string(k_instrument_stages[type]) +
							'.' + std::to_string(variant_index) + '.' + k_graphviz_file_extension;

						if (execution_graphs[type]) {
							bool execution_graph_result = execution_graphs[type]->generate_graphviz_file(
								graph_fname.c_str(), condense_large_arrays);

							if (execution_graph_result) {
								std::cout << "Saved execution graph to '" << graph_fname << "'\n";
							} else {
								std::cerr << "Failed to save '" << graph_fname << "'\n";
							}
						}
					}
				}
			}
		} else {
			std::cerr << "Failed to compile '" << argv[arg] << "'\n";
			result = 1;
		}
	}

	for (h_native_module_library library_handle : c_native_module_registry::iterate_native_module_libraries()) {
		const s_native_module_library &library = c_native_module_registry::get_native_module_library(library_handle);
		if (library.compiler_deinitializer) {
			library.compiler_deinitializer(library_contexts[library_handle.get_data()]);
		}
	}

	c_native_module_registry::shutdown();
	return result;
}
