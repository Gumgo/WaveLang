#include "common/utility/reporting.h"

#include "engine/task_function_registration.h"
#include "engine/task_function_registry.h"

#include "instrument/native_module_registration.h"

template<typename t_type>
static void reverse_linked_list(t_type *&list_head);

static const char *get_task_function_library_name(uint32 library_id);

static bool map_task_function_arguments(
	const std::unordered_map<std::string, s_native_module_uid> &native_module_identifier_map,
	s_task_function_registration_entry *task_function_entry);

void c_task_function_registration_utilities::validate_argument_names(
	size_t argument_count,
	const task_function_binding::s_task_function_argument_names &argument_names) {
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index_a = 0; index_a < argument_count; index_a++) {
		for (size_t index_b = index_a + 1; index_b < argument_count; index_b++) {
			wl_assert(strcmp(argument_names[index_a], argument_names[index_b]) != 0);
		}
	}
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_task_function_registration_utilities::map_arguments(
	const s_task_function &source_task_function,
	const task_function_binding::s_task_function_argument_names &source_argument_names,
	const s_task_function &mapped_task_function,
	const task_function_binding::s_task_function_argument_names &mapped_argument_names,
	task_function_binding::s_argument_index_map &argument_index_map) {
	for (size_t i = 0; i < argument_index_map.get_count(); i++) {
		argument_index_map[i] = k_invalid_task_argument_index;
	}

	for (size_t mapped_argument_index = 0;
		mapped_argument_index < mapped_task_function.argument_count;
		mapped_argument_index++) {
		for (size_t source_argument_index = 0;
			source_argument_index < source_task_function.argument_count;
			source_argument_index++) {
			bool match = strcmp(
				mapped_argument_names[mapped_argument_index],
				source_argument_names[source_argument_index]) == 0;
			if (match) {
				const s_task_function_argument &mapped_argument = mapped_task_function.arguments[mapped_argument_index];
				const s_task_function_argument &source_argument = source_task_function.arguments[source_argument_index];
				wl_assert(mapped_argument.argument_direction == source_argument.argument_direction);
				wl_assert(mapped_argument.type == source_argument.type);
				// "unshared" doesn't need to match - it's taken only from the source

				argument_index_map[mapped_argument_index] = source_argument_index;
				break;
			}
		}

		// Argument not found
		wl_assert(argument_index_map[mapped_argument_index] != k_invalid_task_argument_index);
	}
}

s_task_function_library_registration_entry *&s_task_function_library_registration_entry::registration_list() {
	static s_task_function_library_registration_entry *s_value = nullptr;
	return s_value;
}

s_task_function_library_registration_entry *&s_task_function_library_registration_entry::active_library() {
	static s_task_function_library_registration_entry *s_value = nullptr;
	return s_value;
}

void s_task_function_library_registration_entry::end_active_library_task_function_registration() {
	wl_assert(active_library());
	active_library() = nullptr;
}

bool register_task_functions() {
	wl_assert(!s_task_function_library_registration_entry::active_library());

	c_task_function_registry::begin_registration();

	// $TODO $PLUGIN iterate plugin DLLs to call "register_task_functions" on each one

	// The registration lists get built in reverse order, so flip them
	reverse_linked_list(s_task_function_library_registration_entry::registration_list());

	s_task_function_library_registration_entry *library_entry =
		s_task_function_library_registration_entry::registration_list();
	while (library_entry) {
		reverse_linked_list(library_entry->task_functions);

		if (!c_task_function_registry::register_task_function_library(library_entry->library)) {
			return false; // Error already reported
		}

		std::unordered_map<std::string, s_native_module_uid> native_module_identifier_map;
		c_native_module_registration_utilities::add_native_modules_to_identifier_map(
			library_entry->library.id,
			library_entry->library.version,
			native_module_identifier_map);

		s_task_function_registration_entry *task_function_entry = library_entry->task_functions;
		while (task_function_entry) {
			if (!map_task_function_arguments(native_module_identifier_map, task_function_entry)) {
				return false; // Error already reported
			}

			if (!c_task_function_registry::register_task_function(task_function_entry->task_function)) {
				return false; // Error already reported
			}

			task_function_entry = task_function_entry->next;
		}

		library_entry = library_entry->next;
	}

	return c_task_function_registry::end_registration();
}

template<typename t_type>
static void reverse_linked_list(t_type *&list_head) {
	t_type *current = list_head;
	t_type *previous = nullptr;
	while (current) {
		t_type *next = current->next;
		current->next = previous;
		if (!next) {
			list_head = current;
		}

		previous = current;
		current = next;
	}
}

static const char *get_task_function_library_name(uint32 library_id) {
	return c_task_function_registry::get_task_function_library(
		c_task_function_registry::get_task_function_library_index(library_id)).name.get_string();
}

static bool map_task_function_arguments(
	const std::unordered_map<std::string, s_native_module_uid> &native_module_identifier_map,
	s_task_function_registration_entry *task_function_entry) {
	const char *library = get_task_function_library_name(task_function_entry->task_function.uid.get_library_id());
	std::string identifier = std::string(library) + '.' + task_function_entry->native_module_identifier;
	auto iter = native_module_identifier_map.find(task_function_entry->native_module_identifier);
	if (iter == native_module_identifier_map.end()) {
		report_error(
			"Failed to map arguments for task function 0x%04x (library '%s'): native module '%s' was not found",
			task_function_entry->task_function.uid.get_task_function_id(),
			library,
			task_function_entry->native_module_identifier);
		return false;
	}

	task_function_entry->task_function.native_module_uid = iter->second;

	h_native_module native_module_handle = c_native_module_registry::get_native_module_handle(iter->second);
	wl_assert(native_module_handle.is_valid());
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);

	for (uint32 &index : task_function_entry->task_function.task_function_argument_indices) {
		index = k_invalid_task_argument_index;
	}

	// Each native module argument should map to at most one task function argument
	for (uint32 task_function_argument_index = 0;
		task_function_argument_index < task_function_entry->task_function.argument_count;
		task_function_argument_index++) {
		const char *task_function_argument_name =
			task_function_entry->argument_names[task_function_argument_index];

		size_t mapped_argument_index = k_invalid_native_module_argument_index;
		for (size_t native_module_argument_index = 0;
			native_module_argument_index < native_module.argument_count;
			native_module_argument_index++) {
			const char *native_module_argument_name =
				native_module.arguments[native_module_argument_index].name.get_string();

			if (strcmp(task_function_argument_name, native_module_argument_name) == 0) {
				if (task_function_entry->task_function.task_function_argument_indices[native_module_argument_index] !=
					k_invalid_task_argument_index) {
					report_error(
						"Argument '%s' of native module '%s' "
						"was mapped to multiple arguments of task function 0x%04x (library '%s')",
						native_module_argument_name,
						native_module.name.get_string(),
						task_function_entry->task_function.uid.get_task_function_id(),
						library);
					return false;
				}

				mapped_argument_index = native_module_argument_index;
				break;
			}
		}

		if (mapped_argument_index == k_invalid_native_module_argument_index) {
			report_error(
				"Argument '%s' of task function 0x%04x (library '%s') was not found in native module '%s'",
				task_function_argument_name,
				task_function_entry->task_function.uid.get_task_function_id(),
				library,
				native_module.name.get_string());
			return false;
		}

		task_function_entry->task_function.task_function_argument_indices[mapped_argument_index] =
			task_function_argument_index;
	}

	// Don't bother checking that the mapped types are valid - the registry will do that in a moment
	return true;
}
