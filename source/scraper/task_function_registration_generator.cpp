#include "scraper/scraper_result.h"
#include "scraper/task_function_registration_generator.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

// This is more readable than \t and \n
#define TAB_STR "\t"
#define TAB2_STR "\t\t"
#define TAB3_STR "\t\t\t"
#define NEWLINE_STR "\n"

static const size_t k_invalid_task_argument_index = static_cast<size_t>(-1);

static const char *k_bool_strings[] = { "false", "true" };
static_assert(NUMBEROF(k_bool_strings) == 2, "Invalid bool strings");

static const char *k_task_primitive_type_enum_strings[] = {
	"k_task_primitive_type_real",
	"k_task_primitive_type_bool",
	"k_task_primitive_type_string"
};
static_assert(NUMBEROF(k_task_primitive_type_enum_strings) == k_task_primitive_type_count,
	"Invalid task primitive type enum strings");

static const char *k_task_qualifier_enum_strings[] = {
	"k_task_qualifier_in",
	"k_task_qualifier_out",
	"k_task_qualifier_inout"
};
static_assert(NUMBEROF(k_task_qualifier_enum_strings) == k_task_qualifier_count,
	"Invalid task qualifier enum strings");

static const char *k_task_function_mapping_native_module_input_type_enum_strings[] = {
	"k_task_function_mapping_native_module_input_type_variable",
	"k_task_function_mapping_native_module_input_type_branchless_variable",
	"k_task_function_mapping_native_module_input_type_none"
};
static_assert(NUMBEROF(k_task_function_mapping_native_module_input_type_enum_strings) ==
	k_task_function_mapping_native_module_input_type_count,
	"Invalid task function mapping native module input type enum strings");

static const char *k_task_primitive_type_getter_names[] = {
	"real",
	"bool",
	"string"
};
static_assert(NUMBEROF(k_task_primitive_type_getter_names) == k_task_primitive_type_count,
	"Primitive type getter names mismatch");

static const char *k_task_qualifier_getter_names[] = {
	"_buffer_or_constant_in",
	"_buffer_out",
	"_buffer_inout"
};
static_assert(NUMBEROF(k_task_qualifier_getter_names) == k_task_qualifier_count, "Qualifier getter names mismatch");

struct s_task_argument_mapping {
	std::string in_source;
	std::string out_source;
	size_t in_source_index;
	size_t out_source_index;
	c_task_qualified_data_type type;
	bool is_constant;
};

struct s_task_mapping {
	std::vector<s_task_argument_mapping> task_arguments;
	std::vector<size_t> task_memory_query_argument_indices;
	std::vector<size_t> task_initializer_argument_indices;
	std::vector<size_t> task_voice_initializer_argument_indices;
	std::vector<size_t> task_function_argument_indices;
};

struct s_native_module_to_task_function_mapping {
	size_t native_module_index;
	std::vector<size_t> sorted_task_function_indices;
};

static bool generate_task_function_mapping(
	const c_scraper_result *result, size_t task_function_index, s_task_mapping &mapping);
static bool map_native_module_arguments_to_task_arguments(
	const s_native_module_declaration &native_module,
	const std::vector<s_task_function_argument_declaration> &task_arguments,
	bool all_usages_must_be_possibly_constant,
	const char *function_type,
	const char *function_name,
	s_task_mapping &task_mapping,
	std::vector<size_t> &task_argument_indices);
static size_t find_native_module_argument(const s_native_module_declaration &native_module, const char *argument_name);
static bool is_task_argument_compatible_with_native_module_argument(
	const s_task_function_argument_declaration &task_argument,
	const s_native_module_argument_declaration &native_module_argument,
	const char *task_function_name, const char *native_module_name);
static void write_task_arguments(
	std::ofstream &out, const s_task_mapping &mapping, const std::vector<size_t> argument_indices);
static bool generate_native_module_to_task_function_mappings(
	const c_scraper_result *result,
	const std::vector<s_task_mapping> &task_mappings,
	std::vector<s_native_module_to_task_function_mapping> &mappings);

bool generate_task_function_registration(
	const c_scraper_result *result, const char *registration_function_name, std::ofstream &out) {
	wl_assert(result);
	wl_assert(registration_function_name);

	std::vector<s_task_mapping> task_mappings(result->get_task_function_count());

	{
		bool failed = false;
		for (size_t index = 0; index < result->get_task_function_count(); index++) {
			if (!generate_task_function_mapping(result, index, task_mappings[index])) {
				failed = true;
			}
		}

		if (failed) {
			return false;
		}
	}

	std::vector<s_native_module_to_task_function_mapping> native_module_to_task_function_mappings;

	if (!generate_native_module_to_task_function_mappings(
		result, task_mappings, native_module_to_task_function_mappings)) {
		return false;
	}

	out << "#ifdef TASK_FUNCTION_REGISTRATION_ENABLED" NEWLINE_STR NEWLINE_STR;

	// Generate task function call wrappers
	for (size_t index = 0; index < result->get_task_function_count(); index++) {
		const s_task_function_declaration &task_function = result->get_task_function(index);
		const s_task_mapping &mapping = task_mappings[index];
		const s_library_declaration &library = result->get_library(task_function.library_index);

		if (!task_function.memory_query.empty()) {
			out << "static size_t " << library.name << "_" << task_function.function << "_" <<
				task_function.memory_query << "_call_wrapper(const s_task_function_context &context) {" NEWLINE_STR;

			size_t call_index;
			for (call_index = 0; call_index < result->get_task_memory_query_count(); call_index++) {
				if (result->get_task_memory_query(call_index).name == task_function.memory_query) {
					out << TAB_STR "return " <<
						result->get_task_memory_query(call_index).function_call << "(" NEWLINE_STR;
					write_task_arguments(out, mapping, mapping.task_memory_query_argument_indices);
					break;
				}
			}

			wl_assert(call_index != result->get_task_memory_query_count());

			out << "}" NEWLINE_STR NEWLINE_STR;
		}

		if (!task_function.initializer.empty()) {
			out << "static void " << library.name << "_" << task_function.function << "_" <<
				task_function.initializer << "_call_wrapper(const s_task_function_context &context) {" NEWLINE_STR;

			size_t call_index;
			for (call_index = 0; call_index < result->get_task_initializer_count(); call_index++) {
				if (result->get_task_initializer(call_index).name == task_function.initializer) {
					out << TAB_STR << result->get_task_initializer(call_index).function_call << "(" NEWLINE_STR;
					write_task_arguments(out, mapping, mapping.task_initializer_argument_indices);
					break;
				}
			}

			wl_assert(call_index != result->get_task_initializer_count());

			out << "}" NEWLINE_STR NEWLINE_STR;
		}

		if (!task_function.voice_initializer.empty()) {
			out << "static void " << library.name << "_" << task_function.function << "_" <<
				task_function.voice_initializer <<
				"_call_wrapper(const s_task_function_context &context) {" NEWLINE_STR;

			size_t call_index;
			for (call_index = 0; call_index < result->get_task_voice_initializer_count(); call_index++) {
				if (result->get_task_voice_initializer(call_index).name == task_function.voice_initializer) {
					out << TAB_STR << result->get_task_voice_initializer(call_index).function_call << "(" NEWLINE_STR;
					write_task_arguments(out, mapping, mapping.task_voice_initializer_argument_indices);
					break;
				}
			}

			wl_assert(call_index != result->get_task_voice_initializer_count());

			out << "}" NEWLINE_STR NEWLINE_STR;
		}

		{
			out << "static void " << library.name << "_" << task_function.function <<
				"_call_wrapper(const s_task_function_context &context) {" NEWLINE_STR;

			out << TAB_STR << task_function.function_call << "(" NEWLINE_STR;
			write_task_arguments(out, mapping, mapping.task_function_argument_indices);

			out << "}" NEWLINE_STR NEWLINE_STR;
		}
	}

	out << "bool " << registration_function_name << "() {" NEWLINE_STR;
	out << TAB_STR "bool result = true;" NEWLINE_STR;

	// Generate each task function
	for (size_t index = 0; index < result->get_task_function_count(); index++) {
		const s_task_function_declaration &task_function = result->get_task_function(index);
		const s_task_mapping &task_mapping = task_mappings[index];
		const s_library_declaration &library = result->get_library(task_function.library_index);

		out << TAB_STR "{" NEWLINE_STR;
		out << TAB2_STR "s_task_function task_function;" NEWLINE_STR;
		out << TAB2_STR "ZERO_STRUCT(&task_function);" NEWLINE_STR;
		out << TAB2_STR "task_function.uid = s_task_function_uid::build(" <<
			id_to_string(library.id) << ", " << id_to_string(task_function.id) << ");" NEWLINE_STR;
		out << TAB2_STR "task_function.name.set_verify(\"" << task_function.name << "\");" NEWLINE_STR;

		if (!task_function.memory_query.empty()) {
			out << TAB2_STR "task_function.memory_query = " << library.name << "_" <<
				task_function.function << "_" << task_function.memory_query << "_call_wrapper;" NEWLINE_STR;
		}

		if (!task_function.initializer.empty()) {
			out << TAB2_STR "task_function.initializer = " << library.name << "_" <<
				task_function.function << "_" << task_function.initializer << "_call_wrapper;" NEWLINE_STR;
		}

		if (!task_function.voice_initializer.empty()) {
			out << TAB2_STR "task_function.voice_initializer = " << library.name << "_" <<
				task_function.function << "_" << task_function.voice_initializer << "_call_wrapper;" NEWLINE_STR;
		}

		out << TAB2_STR "task_function.function = " << library.name << "_" <<
			task_function.function << "_call_wrapper;" NEWLINE_STR;

		out << TAB2_STR "task_function.argument_count = " <<
			task_mapping.task_arguments.size() << ";" NEWLINE_STR;

		for (size_t arg = 0; arg < task_mapping.task_arguments.size(); arg++) {
			const s_task_argument_mapping &argument_mapping = task_mapping.task_arguments[arg];
			out << TAB2_STR "task_function.argument_types[" << arg <<
				"] = c_task_qualified_data_type(c_task_data_type(" <<
				k_task_primitive_type_enum_strings[argument_mapping.type.get_data_type().get_primitive_type()] <<
				", " << k_bool_strings[argument_mapping.type.get_data_type().is_array()] << "), " <<
				k_task_qualifier_enum_strings[argument_mapping.type.get_qualifier()] << ");" NEWLINE_STR;
		}

		out << TAB2_STR "result &= c_task_function_registry::register_task_function(task_function);" NEWLINE_STR;
		out << TAB_STR "}" NEWLINE_STR;
	}

	// Generate native module to task function mappings
	for (size_t index = 0; index < native_module_to_task_function_mappings.size(); index++) {
		const s_native_module_to_task_function_mapping &mapping = native_module_to_task_function_mappings[index];
		const s_native_module_declaration &native_module = result->get_native_module(mapping.native_module_index);
		const s_library_declaration &library = result->get_library(native_module.library_index);

		out << TAB_STR "{" NEWLINE_STR;
		out << TAB2_STR "s_task_function_mapping mappings[" <<
			mapping.sorted_task_function_indices.size() << "];" NEWLINE_STR;
		out << TAB2_STR "ZERO_STRUCT(mappings);" NEWLINE_STR;

		for (size_t mapping_index = 0; mapping_index < mapping.sorted_task_function_indices.size(); mapping_index++) {
			size_t task_function_index = mapping.sorted_task_function_indices[mapping_index];
			const s_task_function_declaration &task_function = result->get_task_function(task_function_index);
			const s_task_mapping &task_mapping = task_mappings[task_function_index];
			wl_assert(task_function.library_index == native_module.library_index);

			std::vector<e_task_function_mapping_native_module_input_type> native_module_input_types;
			std::vector<size_t> native_module_task_function_argument_indices;

			// Initialize the native module input types to their default values corresponding to each native module
			// argument type. We will overwrite them with the branchless version when inout buffers are encountered.
			for (size_t arg = 0; arg < native_module.arguments.size(); arg++) {
				native_module_task_function_argument_indices.push_back(k_invalid_task_argument_index);
				if (native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier())) {
					native_module_input_types.push_back(k_task_function_mapping_native_module_input_type_variable);
				} else {
					native_module_input_types.push_back(k_task_function_mapping_native_module_input_type_none);
				}
			}

			// Upgrade variables to branchless variables when we find inout buffers
			for (size_t task_arg = 0; task_arg < task_mapping.task_arguments.size(); task_arg++) {
				size_t in_source_index = task_mapping.task_arguments[task_arg].in_source_index;
				size_t out_source_index = task_mapping.task_arguments[task_arg].out_source_index;

				if (in_source_index != k_invalid_argument_index) {
					wl_assert(native_module_task_function_argument_indices[in_source_index] ==
						k_invalid_task_argument_index);
					native_module_task_function_argument_indices[in_source_index] = task_arg;
				}

				if (out_source_index != k_invalid_argument_index) {
					wl_assert(native_module_task_function_argument_indices[out_source_index] ==
						k_invalid_task_argument_index);
					native_module_task_function_argument_indices[out_source_index] = task_arg;
				}

				if (task_mapping.task_arguments[task_arg].type.get_qualifier() == k_task_qualifier_inout) {
					wl_assert(in_source_index != k_invalid_argument_index);
					wl_assert(native_module_input_types[in_source_index] ==
						k_task_function_mapping_native_module_input_type_variable);
					native_module_input_types[in_source_index] =
						k_task_function_mapping_native_module_input_type_branchless_variable;
				}
			}

			out << TAB2_STR "{" NEWLINE_STR;
			out << TAB3_STR "s_task_function_mapping &mapping = mappings[" << mapping_index << "];" NEWLINE_STR;
			out << TAB3_STR "mapping.task_function_uid = s_task_function_uid::build(" <<
				id_to_string(library.id) << ", " << id_to_string(task_function.id) << ");" NEWLINE_STR;

			for (size_t arg = 0; arg < native_module_input_types.size(); arg++) {
				out << TAB3_STR "mapping.native_module_argument_mapping[" << arg << "].input_type = " <<
					k_task_function_mapping_native_module_input_type_enum_strings[native_module_input_types[arg]] <<
					";" NEWLINE_STR;
				out << TAB3_STR "mapping.native_module_argument_mapping[" << arg <<
					"].task_function_argument_index = " <<  native_module_task_function_argument_indices[arg] <<
					";" NEWLINE_STR;
			}

			out << TAB2_STR "}" NEWLINE_STR;
		}

		out << TAB2_STR "c_task_function_registry::register_task_function_mapping_list(s_native_module_uid::build(" <<
			id_to_string(library.id) << ", " << id_to_string(native_module.id) <<
			"), c_task_function_mapping_list::construct(mappings));" NEWLINE_STR;

		out << TAB_STR "}" NEWLINE_STR;
	}

	out << TAB_STR "return result;" NEWLINE_STR;
	out << "}" NEWLINE_STR NEWLINE_STR;

	out << "#endif // TASK_FUNCTION_REGISTRATION_ENABLED" NEWLINE_STR NEWLINE_STR;

	if (out.fail()) {
		return false;
	}

	return true;
}

static bool generate_task_function_mapping(
	const c_scraper_result *result, size_t task_function_index, s_task_mapping &mapping) {
	const s_task_function_declaration &task_function = result->get_task_function(task_function_index);

	// First, find the source native module
	const s_native_module_declaration *native_module = nullptr;
	for (size_t index = 0; index < result->get_native_module_count(); index++) {
		const s_native_module_declaration &native_module_to_check = result->get_native_module(index);

		if (task_function.library_index == native_module_to_check.library_index &&
			task_function.native_module_source == native_module_to_check.identifier) {
			native_module = &native_module_to_check;
			break;
		}
	}

	if (!native_module) {
		std::cerr << "Failed to find source native module '" << task_function.native_module_source <<
			"' for task function '" << task_function.name << "'\n";
		return false;
	}

	// Find all the other functions associated with the task function
	const s_task_memory_query_declaration *task_memory_query = nullptr;
	const s_task_initializer_declaration *task_initializer = nullptr;
	const s_task_voice_initializer_declaration *task_voice_initializer = nullptr;

	if (!task_function.memory_query.empty()) {
		for (size_t index = 0; index < result->get_task_memory_query_count(); index++) {
			if (result->get_task_memory_query(index).name == task_function.memory_query) {
				task_memory_query = &result->get_task_memory_query(index);
				break;
			}
		}

		if (!task_memory_query) {
			std::cerr << "Failed to find task memory query '" << task_function.memory_query <<
				"' for task function '" << task_function.memory_query << "'\n";
			return false;
		}
	}

	if (!task_function.initializer.empty()) {
		for (size_t index = 0; index < result->get_task_initializer_count(); index++) {
			if (result->get_task_initializer(index).name == task_function.initializer) {
				task_initializer = &result->get_task_initializer(index);
				break;
			}
		}

		if (!task_initializer) {
			std::cerr << "Failed to find task initializer '" << task_function.initializer <<
				"' for task function '" << task_function.initializer << "'\n";
			return false;
		}
	}

	if (!task_function.voice_initializer.empty()) {
		for (size_t index = 0; index < result->get_task_voice_initializer_count(); index++) {
			if (result->get_task_voice_initializer(index).name == task_function.voice_initializer) {
				task_voice_initializer = &result->get_task_voice_initializer(index);
				break;
			}
		}

		if (!task_voice_initializer) {
			std::cerr << "Failed to find task voice initializer '" << task_function.voice_initializer <<
				"' for task function '" << task_function.voice_initializer << "'\n";
			return false;
		}
	}

	if (!map_native_module_arguments_to_task_arguments(
		*native_module,
		task_function.arguments,
		false,
		"task function",
		task_function.name.c_str(),
		mapping,
		mapping.task_function_argument_indices)) {
		return false;
	}

	if (task_memory_query &&
		!map_native_module_arguments_to_task_arguments(
			*native_module,
			task_memory_query->arguments,
			false,
			"task memory query",
			task_memory_query->name.c_str(),
			mapping,
			mapping.task_memory_query_argument_indices)) {
		return false;
	}

	if (task_initializer &&
		!map_native_module_arguments_to_task_arguments(
			*native_module,
			task_initializer->arguments,
			false,
			"task initializer",
			task_initializer->name.c_str(),
			mapping,
			mapping.task_initializer_argument_indices)) {
		return false;
	}

	if (task_voice_initializer &&
		!map_native_module_arguments_to_task_arguments(
			*native_module,
			task_voice_initializer->arguments,
			false,
			"task voice initializer",
			task_voice_initializer->name.c_str(),
			mapping,
			mapping.task_voice_initializer_argument_indices)) {
		return false;
	}

	return true;
}

static bool map_native_module_arguments_to_task_arguments(
	const s_native_module_declaration &native_module,
	const std::vector<s_task_function_argument_declaration> &task_arguments,
	bool all_usages_must_be_possibly_constant,
	const char *function_type,
	const char *function_name,
	s_task_mapping &task_mapping,
	std::vector<size_t> &task_argument_indices) {
	wl_assert(task_argument_indices.empty());

	// Track which native module arguments have been referenced by this task function's arguments
	std::vector<bool> native_module_argument_usage(native_module.arguments.size(), false);

	// Iterate over each task function argument and find its matching native module arguments, which can be up to 2 for
	// inout task arguments. Validate that the task argument is compatible with the native module argument(s) and mark
	// each one as used.
	for (size_t function_argument_index = 0;
		 function_argument_index < task_arguments.size();
		 function_argument_index++) {
		const s_task_function_argument_declaration &argument = task_arguments[function_argument_index];

		if (all_usages_must_be_possibly_constant && !argument.is_possibly_constant) {
			std::cerr << "All arguments to " << function_type << " '" << function_name <<
				"' must be possible constant inputs\n";
			return false;
		}

		size_t task_argument_index = k_invalid_task_argument_index;

		// Determine if we've already added this task argument
		for (size_t index = 0; index < task_mapping.task_arguments.size(); index++) {
			const s_task_argument_mapping &argument_mapping = task_mapping.task_arguments[index];
			wl_assert(!argument_mapping.in_source.empty() || !argument_mapping.out_source.empty());

			bool source_input_matches = (argument.in_source == argument_mapping.in_source);
			bool source_output_matches = (argument.out_source == argument_mapping.out_source);

			if (source_input_matches && source_output_matches) {
				if (argument.type != argument_mapping.type) {
					std::cerr << "Argument '" << argument.name << "' of " << function_type << " '" << function_name <<
						"' doesn't match existing task argument\n";
					return false;
				}

				task_argument_index = index;
				break;
			} else {
				// If only one of the source input and output matches, it means we're using a native module argument in
				// two different ways - e.g. we're using an argument as both an input and an inout, which is not
				// allowed.                                           
				if (source_input_matches && !argument_mapping.in_source.empty()) {
					std::cerr << "Native module '" << native_module.identifier << "' argument '" <<
						argument_mapping.in_source << "' cannot be referenced by multiple " << function_type <<
						" arguments in '" << function_name << "'\n";
					return false;
				} else if (source_output_matches && !argument_mapping.out_source.empty()) {
					std::cerr << "Native module '" << native_module.identifier << "' argument '" <<
						argument_mapping.out_source << "' cannot be referenced by multiple " << function_type <<
						" arguments in '" << function_name << "'\n";
					return false;
				} else {
					// This is not a match, keep searching
				}
			}
		}

		// If we don't find a match, add a new task argument
		if (task_argument_index == k_invalid_task_argument_index) {
			s_task_argument_mapping argument_mapping;
			argument_mapping.in_source = argument.in_source;
			if (!argument.in_source.empty()) {
				argument_mapping.in_source_index =
					find_native_module_argument(native_module, argument.in_source.c_str());
				wl_assert(argument_mapping.in_source_index != k_invalid_argument_index);

				if (!is_task_argument_compatible_with_native_module_argument(
					argument,
					native_module.arguments[argument_mapping.in_source_index],
					function_name,
					native_module.identifier.c_str())) {
					return false;
				}
			} else {
				argument_mapping.in_source_index = k_invalid_argument_index;
			}

			argument_mapping.out_source = argument.out_source;
			if (!argument.out_source.empty()) {
				argument_mapping.out_source_index =
					find_native_module_argument(native_module, argument.out_source.c_str());
				wl_assert(argument_mapping.out_source_index != k_invalid_argument_index);

				if (!is_task_argument_compatible_with_native_module_argument(
					argument,
					native_module.arguments[argument_mapping.out_source_index],
					function_name,
					native_module.identifier.c_str())) {
					return false;
				}
			} else {
				argument_mapping.out_source_index = k_invalid_argument_index;
			}

			argument_mapping.type = argument.type;
			argument_mapping.is_constant = argument.is_constant;

			task_argument_index = task_mapping.task_arguments.size();
			task_mapping.task_arguments.push_back(argument_mapping);
		}

		// Make sure the source native module arguments haven't already been referenced by this function
		const s_task_argument_mapping &argument_mapping =
			task_mapping.task_arguments[task_argument_index];

		if (!argument_mapping.in_source.empty()) {
			if (native_module_argument_usage[argument_mapping.in_source_index]) {
				std::cerr << "Native module '" << native_module.identifier << "' argument '" <<
					argument_mapping.in_source << "' cannot be referenced by multiple " <<
					function_type << " arguments in '" << function_name << "'\n";
			} else {
				native_module_argument_usage[argument_mapping.in_source_index] = true;
			}
		}

		if (!argument_mapping.out_source.empty()) {
			if (native_module_argument_usage[argument_mapping.out_source_index]) {
				std::cerr << "Native module '" << native_module.identifier << "' argument '" <<
					argument_mapping.out_source << "' cannot be referenced by multiple " <<
					function_type << " arguments in '" << function_name << "'\n";
			} else {
				native_module_argument_usage[argument_mapping.out_source_index] = true;
			}
		}

		// Point the task function argument to the correct argument index
		task_argument_indices.push_back(task_argument_index);
	}

	return true;
}

static size_t find_native_module_argument(const s_native_module_declaration &native_module, const char *argument_name) {
	for (size_t index = 0; index < native_module.arguments.size(); index++) {
		if (native_module.arguments[index].name == argument_name) {
			return index;
		}
	}

	std::cerr << "Native module '" << native_module.identifier << "' has no argument named '" << argument_name << "'\n";
	return k_invalid_argument_index;
}

static bool is_task_argument_compatible_with_native_module_argument(
	const s_task_function_argument_declaration &task_argument,
	const s_native_module_argument_declaration &native_module_argument,
	const char *task_function_name, const char *native_module_name) {
	static const e_task_primitive_type k_task_primitive_types_from_native_module_primitive_types[] = {
		k_task_primitive_type_real,		// k_native_module_primitive_type_real
		k_task_primitive_type_bool,		// k_native_module_primitive_type_bool
		k_task_primitive_type_string	// k_native_module_primitive_type_string
	};
	static_assert(NUMBEROF(k_task_primitive_types_from_native_module_primitive_types) ==
		k_native_module_primitive_type_count,
		"Native module/task primitive type mismatch");

	// Check to see if the types themselves are compatible
	c_task_data_type task_data_type_from_native_module_data_type = c_task_data_type(
		k_task_primitive_types_from_native_module_primitive_types[
			native_module_argument.type.get_data_type().get_primitive_type()],
		native_module_argument.type.get_data_type().is_array());
	if (task_data_type_from_native_module_data_type != task_argument.type.get_data_type()) {
		std::cerr << "Native module '" << native_module_name << "' argument '" << native_module_argument.name <<
			"' type is incompatible with task '" << task_function_name << "' argument '" << task_argument.name <<
			"' type\n";
		return false;
	}

	// Check whether qualifiers are compatible
	e_native_module_qualifier native_module_qualifier = native_module_argument.type.get_qualifier();
	e_task_qualifier task_qualifier = task_argument.type.get_qualifier();
	if ((task_argument.is_constant && native_module_qualifier != k_native_module_qualifier_constant) ||
		(task_qualifier == k_task_qualifier_in && !native_module_qualifier_is_input(native_module_qualifier)) ||
		(task_qualifier == k_task_qualifier_out && native_module_qualifier != k_native_module_qualifier_out)) {
		// Note: inout task qualifier is compatible with either in or out native module qualifier
		std::cerr << "Native module '" << native_module_name << "' argument '" << native_module_argument.name <<
			"' qualifier is incompatible with task '" << task_function_name << "' argument '" << task_argument.name <<
			"' qualifier\n";
		return false;
	}

	return true;
}

static void write_task_arguments(
	std::ofstream &out, const s_task_mapping &mapping, const std::vector<size_t> argument_indices) {
	// First argument is the context
	out << TAB2_STR << "context" << (argument_indices.empty() ? ");" : ",") << NEWLINE_STR;

	for (size_t arg = 0; arg < argument_indices.size(); arg++) {
		size_t task_argument_index = argument_indices[arg];
		const s_task_argument_mapping &argument_mapping = mapping.task_arguments[task_argument_index];

		out << TAB2_STR "context.arguments[" << task_argument_index << "].get_" <<
			k_task_primitive_type_getter_names[argument_mapping.type.get_data_type().get_primitive_type()];

		if (argument_mapping.type.get_data_type().is_array()) {
			out << "_array_in";
		} else {
			if (argument_mapping.is_constant) {
				out << "_constant_in";
			} else {
				out << k_task_qualifier_getter_names[argument_mapping.type.get_qualifier()];
			}
		}

		out << "()" << ((arg + 1 == argument_indices.size()) ? ");" : ",") << NEWLINE_STR;
	}
}

static bool generate_native_module_to_task_function_mappings(
	const c_scraper_result *result,
	const std::vector<s_task_mapping> &task_mappings,
	std::vector<s_native_module_to_task_function_mapping> &mappings) {
	wl_assert(result);
	wl_assert(mappings.empty());

	for (size_t index = 0; index < result->get_native_module_count(); index++) {
		const s_native_module_declaration &native_module = result->get_native_module(index);
		s_native_module_to_task_function_mapping mapping;
		mapping.native_module_index = index;

		for (size_t task_function = 0; task_function < result->get_task_function_count(); task_function++) {
			if (result->get_task_function(task_function).library_index == native_module.library_index &&
				result->get_task_function(task_function).native_module_source == native_module.identifier) {
				mapping.sorted_task_function_indices.push_back(task_function);
			}
		}

		if (mapping.sorted_task_function_indices.empty()) {
			// This native module has no associated task functions
			continue;
		}

		// Sort the mappings so that we prioritize inout buffers
		struct s_native_module_to_task_function_mapping_comparator {
			const std::vector<s_task_mapping> *task_mappings;

			bool operator()(size_t task_function_a, size_t task_function_b) const {
				const s_task_mapping &mapping_a = (*task_mappings)[task_function_a];
				const s_task_mapping &mapping_b = (*task_mappings)[task_function_b];

				// More inout buffers always goes first
				size_t inout_count_a = 0;
				size_t inout_count_b = 0;

				for (size_t index = 0; index < mapping_a.task_arguments.size(); index++) {
					if (mapping_a.task_arguments[index].type.get_qualifier() == k_task_qualifier_inout) {
						inout_count_a++;
					}
				}

				for (size_t index = 0; index < mapping_b.task_arguments.size(); index++) {
					if (mapping_b.task_arguments[index].type.get_qualifier() == k_task_qualifier_inout) {
						inout_count_b++;
					}
				}

				if (inout_count_a != inout_count_b) {
					return inout_count_a > inout_count_b;
				}

				// Fewer arguments wins
				if (mapping_a.task_arguments.size() != mapping_b.task_arguments.size()) {
					return mapping_a.task_arguments.size() < mapping_b.task_arguments.size();
				}

				// First inout argument wins
				for (size_t index = 0; index < mapping_a.task_arguments.size(); index++) {
					e_task_qualifier qualifier_a = mapping_a.task_arguments[index].type.get_qualifier();
					e_task_qualifier qualifier_b = mapping_b.task_arguments[index].type.get_qualifier();
					if (qualifier_a != qualifier_b) {
						if (qualifier_a == k_task_qualifier_inout) {
							return true;
						} else if (qualifier_b == k_task_qualifier_inout) {
							return false;
						}
					}
				}

				return false;
			}
		};

		s_native_module_to_task_function_mapping_comparator comparator;
		comparator.task_mappings = &task_mappings;

		std::sort(
			mapping.sorted_task_function_indices.begin(),
			mapping.sorted_task_function_indices.end(),
			comparator);

		mappings.push_back(mapping);
	}

	// No failure cases (currently)
	return true;
}