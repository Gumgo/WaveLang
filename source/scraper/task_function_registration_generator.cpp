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

static constexpr size_t k_invalid_task_argument_index = static_cast<size_t>(-1);

static constexpr const char *k_bool_strings[] = { "false", "true" };
static_assert(array_count(k_bool_strings) == 2, "Invalid bool strings");

static constexpr const char *k_task_primitive_type_enum_strings[] = {
	"e_task_primitive_type::k_real",
	"e_task_primitive_type::k_bool",
	"e_task_primitive_type::k_string"
};
static_assert(array_count(k_task_primitive_type_enum_strings) == enum_count<e_task_primitive_type>(),
	"Invalid task primitive type enum strings");

static constexpr const char *k_task_qualifier_enum_strings[] = {
	"e_task_qualifier::k_in",
	"e_task_qualifier::k_out",
	"e_task_qualifier::k_constant"
};
static_assert(array_count(k_task_qualifier_enum_strings) == enum_count<e_task_qualifier>(),
	"Invalid task qualifier enum strings");

static constexpr const char *k_task_primitive_type_getter_names[] = {
	"real",
	"bool",
	"string"
};
static_assert(array_count(k_task_primitive_type_getter_names) == enum_count<e_task_primitive_type>(),
	"Primitive type getter names mismatch");

static constexpr const char *k_task_qualifier_getter_names[] = {
	"_in",
	"_out",
	"_in" // k_constant still uses "in" for the getter
};
static_assert(array_count(k_task_qualifier_getter_names) == enum_count<e_task_qualifier>(),
	"Qualifier getter names mismatch");

struct s_task_argument_mapping {
	std::string source;
	size_t source_index;
	c_task_qualified_data_type type;
	bool is_unshared;
};

struct s_task_mapping {
	size_t native_module_index;
	std::vector<s_task_argument_mapping> task_arguments;
	std::vector<size_t> task_memory_query_argument_indices;
	std::vector<size_t> task_initializer_argument_indices;
	std::vector<size_t> task_voice_initializer_argument_indices;
	std::vector<size_t> task_function_argument_indices;
};

static bool generate_task_function_mapping(
	const c_scraper_result *result,
	size_t task_function_index,
	s_task_mapping &mapping);
static bool map_native_module_arguments_to_task_arguments(
	const s_native_module_declaration &native_module,
	const std::vector<s_task_function_argument_declaration> &task_arguments,
	const char *function_type,
	const char *function_name,
	bool affects_unshared,
	s_task_mapping &task_mapping,
	std::vector<size_t> &task_argument_indices);
static size_t find_native_module_argument(const s_native_module_declaration &native_module, const char *argument_name);
static bool is_task_argument_compatible_with_native_module_argument(
	const s_task_function_argument_declaration &task_argument,
	const s_native_module_argument_declaration &native_module_argument,
	const char *task_function_name,
	const char *native_module_name);
static void write_task_arguments(
	std::ofstream &out,
	const s_task_mapping &mapping,
	const std::vector<size_t> argument_indices);

bool generate_task_function_registration(
	const c_scraper_result *result,
	const char *registration_function_name,
	std::ofstream &out) {
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

	// Generate library registration
	for (size_t library_index = 0; library_index < result->get_library_count(); library_index++) {
		const s_library_declaration &library = result->get_library(library_index);

		const s_library_engine_initializer_declaration *library_engine_initializer = nullptr;
		const s_library_engine_deinitializer_declaration *library_engine_deinitializer = nullptr;
		const s_library_tasks_pre_initializer_declaration *library_tasks_pre_initializer = nullptr;
		const s_library_tasks_post_initializer_declaration *library_tasks_post_initializer = nullptr;

		for (size_t index = 0; index < result->get_library_engine_initializer_count(); index++) {
			const s_library_engine_initializer_declaration &declaration =
				result->get_library_engine_initializer(index);
			if (library_index == declaration.library_index) {
				if (library_engine_initializer) {
					std::cerr << "Multiple engine initializers specified for library '" << library.name << "'\n";
					return false;
				}

				library_engine_initializer = &declaration;
			}
		}

		for (size_t index = 0; index < result->get_library_engine_deinitializer_count(); index++) {
			const s_library_engine_deinitializer_declaration &declaration =
				result->get_library_engine_deinitializer(index);
			if (library_index == declaration.library_index) {
				if (library_engine_deinitializer) {
					std::cerr << "Multiple engine deinitializers specified for library '" << library.name << "'\n";
					return false;
				}

				library_engine_deinitializer = &declaration;
			}
		}

		for (size_t index = 0; index < result->get_library_tasks_pre_initializer_count(); index++) {
			const s_library_tasks_pre_initializer_declaration &declaration =
				result->get_library_tasks_pre_initializer(index);
			if (library_index == declaration.library_index) {
				if (library_tasks_pre_initializer) {
					std::cerr << "Multiple tasks pre-initializers specified for library '" << library.name << "'\n";
					return false;
				}

				library_tasks_pre_initializer = &declaration;
			}
		}

		for (size_t index = 0; index < result->get_library_tasks_post_initializer_count(); index++) {
			const s_library_tasks_post_initializer_declaration &declaration =
				result->get_library_tasks_post_initializer(index);
			if (library_index == declaration.library_index) {
				if (library_tasks_post_initializer) {
					std::cerr << "Multiple tasks post-initializers specified for library '" << library.name << "'\n";
					return false;
				}

				library_tasks_post_initializer = &declaration;
			}
		}

		out << TAB_STR "{" NEWLINE_STR;
		out << TAB2_STR "s_task_function_library library;" NEWLINE_STR;
		out << TAB2_STR "zero_type(&library);" NEWLINE_STR;
		out << TAB2_STR "library.id = " << id_to_string(library.id) << ";" NEWLINE_STR;
		out << TAB2_STR "library.name.set_verify(\"" << library.name << "\");" NEWLINE_STR;
		out << TAB2_STR "library.version = " << library.version << ";" NEWLINE_STR;
		out << TAB2_STR "library.engine_initializer = "
			<< (library_engine_initializer ? library_engine_initializer->function_call.c_str() : "nullptr")
			<< ";" NEWLINE_STR;
		out << TAB2_STR "library.engine_deinitializer = "
			<< (library_engine_deinitializer ? library_engine_deinitializer->function_call.c_str() : "nullptr")
			<< ";" NEWLINE_STR;
		out << TAB2_STR "library.tasks_pre_initializer = "
			<< (library_tasks_pre_initializer ? library_tasks_pre_initializer->function_call.c_str() : "nullptr")
			<< ";" NEWLINE_STR;
		out << TAB2_STR "library.tasks_post_initializer = "
			<< (library_tasks_post_initializer ? library_tasks_post_initializer->function_call.c_str() : "nullptr")
			<< ";" NEWLINE_STR;
		out << TAB2_STR "result &= c_task_function_registry::register_task_function_library(library);" NEWLINE_STR;
		out << TAB_STR "}" NEWLINE_STR;
	}

	// Generate each task function
	for (size_t index = 0; index < result->get_task_function_count(); index++) {
		const s_task_function_declaration &task_function = result->get_task_function(index);
		const s_task_mapping &task_mapping = task_mappings[index];
		const s_library_declaration &library = result->get_library(task_function.library_index);
		const s_native_module_declaration &native_module = result->get_native_module(task_mapping.native_module_index);
		wl_assert(task_function.library_index == native_module.library_index);

		out << TAB_STR "{" NEWLINE_STR;
		out << TAB2_STR "s_task_function task_function;" NEWLINE_STR;
		out << TAB2_STR "zero_type(&task_function);" NEWLINE_STR;
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
			e_task_primitive_type primitive_type = argument_mapping.type.get_data_type().get_primitive_type();
			e_task_qualifier qualifier = argument_mapping.type.get_qualifier();
			out << TAB2_STR "task_function.argument_types[" << arg <<
				"] = c_task_qualified_data_type(c_task_data_type(" <<
				k_task_primitive_type_enum_strings[enum_index(primitive_type)] <<
				", " << k_bool_strings[argument_mapping.type.get_data_type().is_array()] << "), " <<
				k_task_qualifier_enum_strings[enum_index(qualifier)] << ");" NEWLINE_STR;
			out << TAB2_STR "task_function.arguments_unshared[" << arg <<
				"] = " << (argument_mapping.is_unshared ? "true" : "false") << ";" NEWLINE_STR;
		}

		out << TAB2_STR "task_function.native_module_uid = s_native_module_uid::build(" <<
			id_to_string(library.id) << ", " << id_to_string(native_module.id) << ");" NEWLINE_STR;

		std::vector<size_t> native_module_task_function_argument_indices(
			native_module.arguments.size(),
			k_invalid_task_argument_index);

		for (size_t task_arg = 0; task_arg < task_mapping.task_arguments.size(); task_arg++) {
			size_t source_index = task_mapping.task_arguments[task_arg].source_index;
			wl_assert(source_index != k_invalid_native_module_argument_index);
			wl_assert(native_module_task_function_argument_indices[source_index] == k_invalid_task_argument_index);
			native_module_task_function_argument_indices[source_index] = task_arg;
		}

		for (size_t arg = 0; arg < native_module_task_function_argument_indices.size(); arg++) {
			out << TAB2_STR "task_function.task_function_argument_indices[" << arg << "] = ";
			if (native_module_task_function_argument_indices[arg] == k_invalid_task_argument_index) {
				out << "k_invalid_task_argument_index";
			} else {
				out << native_module_task_function_argument_indices[arg];
			}
			out << ";" NEWLINE_STR;
		}

		out << TAB2_STR "result &= c_task_function_registry::register_task_function(task_function);" NEWLINE_STR;
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
	const c_scraper_result *result,
	size_t task_function_index,
	s_task_mapping &mapping) {
	const s_task_function_declaration &task_function = result->get_task_function(task_function_index);

	// First, find the source native module
	const s_native_module_declaration *native_module = nullptr;
	for (size_t index = 0; index < result->get_native_module_count(); index++) {
		const s_native_module_declaration &native_module_to_check = result->get_native_module(index);

		if (task_function.library_index == native_module_to_check.library_index
			&& task_function.native_module_source == native_module_to_check.identifier) {
			native_module = &native_module_to_check;
			mapping.native_module_index = index;
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
		"task function",
		task_function.name.c_str(),
		true,
		mapping,
		mapping.task_function_argument_indices)) {
		return false;
	}

	if (task_memory_query
		&& !map_native_module_arguments_to_task_arguments(
			*native_module,
			task_memory_query->arguments,
			"task memory query",
			task_memory_query->name.c_str(),
			false,
			mapping,
			mapping.task_memory_query_argument_indices)) {
		return false;
	}

	if (task_initializer
		&& !map_native_module_arguments_to_task_arguments(
			*native_module,
			task_initializer->arguments,
			"task initializer",
			task_initializer->name.c_str(),
			false,
			mapping,
			mapping.task_initializer_argument_indices)) {
		return false;
	}

	if (task_voice_initializer
		&& !map_native_module_arguments_to_task_arguments(
			*native_module,
			task_voice_initializer->arguments,
			"task voice initializer",
			task_voice_initializer->name.c_str(),
			false,
			mapping,
			mapping.task_voice_initializer_argument_indices)) {
		return false;
	}

	return true;
}

static bool map_native_module_arguments_to_task_arguments(
	const s_native_module_declaration &native_module,
	const std::vector<s_task_function_argument_declaration> &task_arguments,
	const char *function_type,
	const char *function_name,
	bool affects_unshared,
	s_task_mapping &task_mapping,
	std::vector<size_t> &task_argument_indices) {
	wl_assert(task_argument_indices.empty());

	// Track which native module arguments have been referenced by this task function's arguments
	std::vector<bool> native_module_argument_usage(native_module.arguments.size(), false);

	// Iterate over each task function argument and find its matching native module argument. Validate that the task
	// argument is compatible with the native module argument(s) and mark each one as used.
	for (size_t function_argument_index = 0;
		function_argument_index < task_arguments.size();
		function_argument_index++) {
		const s_task_function_argument_declaration &argument = task_arguments[function_argument_index];

		size_t task_argument_index = k_invalid_task_argument_index;

		// Determine if we've already added this task argument
		for (size_t index = 0; index < task_mapping.task_arguments.size(); index++) {
			const s_task_argument_mapping &argument_mapping = task_mapping.task_arguments[index];
			wl_assert(!argument_mapping.source.empty());

			if (argument.source == argument_mapping.source) {
				if (argument.type != argument_mapping.type) {
					std::cerr << "Argument '" << argument.name << "' of " << function_type << " '" << function_name <<
						"' doesn't match existing task argument\n";
					return false;
				}

				task_argument_index = index;
				break;
			} else {
				// This is not a match, keep searching
			}
		}

		// If we don't find a match, add a new task argument
		if (task_argument_index == k_invalid_task_argument_index) {
			s_task_argument_mapping argument_mapping;
			argument_mapping.source = argument.source;
			if (!argument.source.empty()) {
				argument_mapping.source_index = find_native_module_argument(native_module, argument.source.c_str());
				wl_assert(argument_mapping.source_index != k_invalid_native_module_argument_index);

				if (!is_task_argument_compatible_with_native_module_argument(
					argument,
					native_module.arguments[argument_mapping.source_index],
					function_name,
					native_module.identifier.c_str())) {
					return false;
				}
			} else {
				argument_mapping.source_index = k_invalid_native_module_argument_index;
			}

			argument_mapping.type = argument.type;
			argument_mapping.is_unshared = false;

			task_argument_index = task_mapping.task_arguments.size();
			task_mapping.task_arguments.push_back(argument_mapping);
		}

		s_task_argument_mapping &argument_mapping = task_mapping.task_arguments[task_argument_index];
		if (affects_unshared) {
			// Only task functions affect is_unshared, other related functions (e.g. initializers) don't
			argument_mapping.is_unshared = argument.is_unshared;
		}

		// Make sure the source native module arguments haven't already been referenced by this function
		if (!argument_mapping.source.empty()) {
			if (native_module_argument_usage[argument_mapping.source_index]) {
				std::cerr << "Native module '" << native_module.identifier << "' argument '" <<
					argument_mapping.source << "' cannot be referenced by multiple " <<
					function_type << " arguments in '" << function_name << "'\n";
			} else {
				native_module_argument_usage[argument_mapping.source_index] = true;
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
	return k_invalid_native_module_argument_index;
}

static bool is_task_argument_compatible_with_native_module_argument(
	const s_task_function_argument_declaration &task_argument,
	const s_native_module_argument_declaration &native_module_argument,
	const char *task_function_name,
	const char *native_module_name) {
	static constexpr e_task_primitive_type k_task_primitive_types_from_native_module_primitive_types[] = {
		e_task_primitive_type::k_real,	// e_native_module_primitive_type::k_real
		e_task_primitive_type::k_bool,	// e_native_module_primitive_type::k_bool
		e_task_primitive_type::k_string	// e_native_module_primitive_type::k_string
	};
	static_assert(array_count(k_task_primitive_types_from_native_module_primitive_types) ==
		enum_count<e_native_module_primitive_type>(),
		"Native module/task primitive type mismatch");

	// Check to see if the types themselves are compatible
	e_native_module_primitive_type primitive_type = native_module_argument.type.get_data_type().get_primitive_type();
	c_task_data_type task_data_type_from_native_module_data_type = c_task_data_type(
		k_task_primitive_types_from_native_module_primitive_types[enum_index(primitive_type)],
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
	bool compatible = false;
	switch (native_module_qualifier) {
	case e_native_module_qualifier::k_in:
		compatible = (task_qualifier == e_task_qualifier::k_in);
		break;

	case e_native_module_qualifier::k_out:
		compatible = (task_qualifier == e_task_qualifier::k_out);
		break;

	case e_native_module_qualifier::k_constant:
		compatible = (task_qualifier == e_task_qualifier::k_in) || (task_qualifier == e_task_qualifier::k_constant);
		break;
	}

	if (!compatible) {
		std::cerr << "Native module '" << native_module_name << "' argument '" << native_module_argument.name <<
			"' qualifier is incompatible with task '" << task_function_name << "' argument '" << task_argument.name <<
			"' qualifier\n";
		return false;
	}

	return true;
}

static void write_task_arguments(
	std::ofstream &out,
	const s_task_mapping &mapping,
	const std::vector<size_t> argument_indices) {
	// First argument is the context
	out << TAB2_STR << "context" << (argument_indices.empty() ? ");" : ",") << NEWLINE_STR;

	for (size_t arg = 0; arg < argument_indices.size(); arg++) {
		size_t task_argument_index = argument_indices[arg];
		const s_task_argument_mapping &argument_mapping = mapping.task_arguments[task_argument_index];
		e_task_primitive_type primitive_type = argument_mapping.type.get_data_type().get_primitive_type();

		out << TAB2_STR "context.arguments[" << task_argument_index << "].get_" <<
			k_task_primitive_type_getter_names[enum_index(primitive_type)];

		if (argument_mapping.type.get_qualifier() == e_task_qualifier::k_constant) {
			out << "_constant";
		} else {
			out << "_buffer";
		}

		if (argument_mapping.type.get_data_type().is_array()) {
			out << "_array";
		}

		out << k_task_qualifier_getter_names[enum_index(argument_mapping.type.get_qualifier())];

		out << "()" << ((arg + 1 == argument_indices.size()) ? ");" : ",") << NEWLINE_STR;
	}
}
