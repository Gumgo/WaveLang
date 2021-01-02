#include "common/utility/reporting.h"

#include "engine/task_function_registry.h"

#include <vector>
#include <unordered_map>

enum class e_task_function_registry_state {
	k_uninitialized,
	k_initialized,
	k_registering,
	k_finalized,

	k_count,
};

struct s_task_function_mapping_list_internal {
	size_t first_mapping_index;
	size_t mapping_count;
};

namespace std {
	template<>
	struct hash<s_task_function_uid> {
		size_t operator()(const s_task_function_uid &key) const {
			return hash<uint64>()(key.data_uint64);
		}
	};
}

struct s_task_function_registry_data {
	std::vector<s_task_function_library> task_function_libraries;
	std::unordered_map<uint32, uint32> task_function_library_ids_to_indices;

	std::vector<s_task_function> task_functions;
	std::unordered_map<s_task_function_uid, uint32> task_function_uids_to_indices;

	// Each native module maps to a task
	std::vector<s_task_function_uid> task_function_mappings;
};

static e_task_function_registry_state g_task_function_registry_state = e_task_function_registry_state::k_uninitialized;
static s_task_function_registry_data g_task_function_registry_data;

static bool validate_task_function_mapping(const s_native_module &native_module, const s_task_function &task_function);

static constexpr e_task_primitive_type k_native_module_primitive_type_to_task_primitive_type_mapping[] = {
	e_task_primitive_type::k_real,	// e_native_module_primitive_type::k_real
	e_task_primitive_type::k_bool,	// e_native_module_primitive_type::k_bool
	e_task_primitive_type::k_string	// e_native_module_primitive_type::k_string
};
STATIC_ASSERT(
	is_enum_fully_mapped<e_native_module_primitive_type>(
		k_native_module_primitive_type_to_task_primitive_type_mapping));

static e_task_primitive_type task_primitive_type_from_native_module_primitive_type(
	e_native_module_primitive_type native_module_primitive_type) {
	wl_assert(valid_enum_index(native_module_primitive_type));
	return k_native_module_primitive_type_to_task_primitive_type_mapping[enum_index(native_module_primitive_type)];
}

void c_task_function_registry::initialize() {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_uninitialized);
	g_task_function_registry_state = e_task_function_registry_state::k_initialized;
}

void c_task_function_registry::shutdown() {
	wl_assert(g_task_function_registry_state != e_task_function_registry_state::k_uninitialized);

	// Clear and free all memory
	g_task_function_registry_data.task_function_libraries.clear();
	g_task_function_registry_data.task_function_libraries.shrink_to_fit();
	g_task_function_registry_data.task_function_library_ids_to_indices.clear();
	g_task_function_registry_data.task_functions.clear();
	g_task_function_registry_data.task_functions.shrink_to_fit();
	g_task_function_registry_data.task_function_uids_to_indices.clear();

	g_task_function_registry_data.task_function_mappings.clear();
	g_task_function_registry_data.task_function_mappings.shrink_to_fit();

	g_task_function_registry_state = e_task_function_registry_state::k_uninitialized;
}

void c_task_function_registry::begin_registration() {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_initialized);

	// Clear all task mappings initially
	uint32 native_module_count = c_native_module_registry::get_native_module_count();
	g_task_function_registry_data.task_function_mappings.resize(native_module_count);
	for (uint32 index = 0; index < native_module_count; index++) {
		g_task_function_registry_data.task_function_mappings[index] = s_task_function_uid::invalid();
	}

	g_task_function_registry_state = e_task_function_registry_state::k_registering;
}

bool c_task_function_registry::end_registration() {
	for (h_native_module native_module_handle : c_native_module_registry::iterate_native_modules()) {
		bool always_runs_at_compile_time;
		bool always_runs_at_compile_time_if_dependent_constants_are_constant;
		const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);
		get_native_module_compile_time_properties(
			native_module,
			always_runs_at_compile_time,
			always_runs_at_compile_time_if_dependent_constants_are_constant);

		// If it's possible for the native module not to run at compile-time, make sure it has an associated task
		// function
		if (!always_runs_at_compile_time
			&& !get_task_function_mapping(native_module_handle).is_valid()) {
			report_error(
				"Native module 0x%04x ('%s') doesn't always run at compile-time "
				"but is not mapped to any task function for runtime evaluation",
				native_module.uid.get_module_id(),
				native_module.name.get_string());
			return false;
		}
	}

	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_registering);
	g_task_function_registry_state = e_task_function_registry_state::k_finalized;
	return true;
}

bool c_task_function_registry::register_task_function_library(const s_task_function_library &library) {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_registering);

	// Make sure there isn't already a library registered with this ID
	auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(library.id);
	if (it != g_task_function_registry_data.task_function_library_ids_to_indices.end()) {
		report_error(
			"Failed to register conflicting task function library 0x%04x ('%s')",
			library.id,
			library.name.get_string());
		return false;
	}

	// Validate that there is a matching native module library
	if (!c_native_module_registry::is_native_module_library_registered(library.id)) {
		report_error(
			"No matching native module library registered for task function library 0x%04x ('%s')",
			library.id,
			library.name.get_string());
		return false;
	}

	// Make sure the details of the library are consistent
	h_native_module_library native_module_library_handle =
		c_native_module_registry::get_native_module_library_handle(library.id);
	const s_native_module_library &native_module_library =
		c_native_module_registry::get_native_module_library(native_module_library_handle);
	if (library.version != native_module_library.version) {
		report_error(
			"Task function library 0x%04x ('%s') version does not match native module library version (%d vs. %d)",
			library.id,
			library.name.get_string(),
			library.version,
			native_module_library.version);
		return false;
	}

	if (library.name != native_module_library.name) {
		report_error(
			"Task function library 0x%04x ('%s') name does not match native module library name ('%s' vs. '%s')",
			library.id,
			library.name.get_string(),
			library.name.get_string(),
			native_module_library.name.get_string());
		return false;
	}

	// $TODO $PLUGIN make sure there isn't a library with the same name

	uint32 index = cast_integer_verify<uint32>(g_task_function_registry_data.task_function_libraries.size());

	g_task_function_registry_data.task_function_libraries.push_back(library);
	g_task_function_registry_data.task_function_library_ids_to_indices.insert(std::make_pair(library.id, index));
	return true;
}

bool c_task_function_registry::is_task_function_library_registered(uint32 library_id) {
	auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(library_id);
	return it != g_task_function_registry_data.task_function_library_ids_to_indices.end();
}

uint32 c_task_function_registry::get_task_function_library_index(uint32 library_id) {
	wl_assert(is_task_function_library_registered(library_id));
	auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(library_id);
	return it->second;
}

uint32 c_task_function_registry::get_task_function_library_count() {
	return cast_integer_verify<uint32>(g_task_function_registry_data.task_function_libraries.size());
}

const s_task_function_library &c_task_function_registry::get_task_function_library(uint32 index) {
	return g_task_function_registry_data.task_function_libraries[index];
}

bool c_task_function_registry::register_task_function(const s_task_function &task_function) {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_registering);

	// Validate that the task function is set up correctly
	// We don't need a memory query or initializer, but all task functions must have something to execute
	if (!task_function.function) {
		report_error(
			"Failed to register task function 0x%04x because no function was provided",
			task_function.uid.get_task_function_id());
		return false;
	}

	if (!validate_task_function(task_function)) {
		report_error(
			"Failed to register task function 0x%04x because it contains errors",
			task_function.uid.get_task_function_id());
		return false;
	}

	// $TODO $PLUGIN Check that the library referenced by the UID is registered

	// Check that the library referenced by the UID is registered
	{
		auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(
			task_function.uid.get_library_id());
		if (it == g_task_function_registry_data.task_function_library_ids_to_indices.end()) {
			report_error(
				"Task function library 0x%04x referenced by task function 0x%04x was not registered",
				task_function.uid.get_library_id(),
				task_function.uid.get_task_function_id());
			return false;
		}
	}

	// Check that the UID isn't already in use
	if (g_task_function_registry_data.task_function_uids_to_indices.find(task_function.uid) !=
		g_task_function_registry_data.task_function_uids_to_indices.end()) {
		report_error("Failed to register conflicting task function 0x%04x", task_function.uid.get_task_function_id());
		return false;
	}

	// Look up the native module
	h_native_module native_module_handle =
		c_native_module_registry::get_native_module_handle(task_function.native_module_uid);
	if (!native_module_handle.is_valid()) {
		report_error(
			"No matching native module registered for task function 0x%04x",
			task_function.uid.get_task_function_id());
		return false;
	}

	bool always_runs_at_compile_time;
	bool always_runs_at_compile_time_if_dependent_constants_are_constant;
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);
	get_native_module_compile_time_properties(
		native_module,
		always_runs_at_compile_time,
		always_runs_at_compile_time_if_dependent_constants_are_constant);

	// If the associated native module always runs at compile time, this task function can never run
	// $TODO $PLUGIN maybe we want a warning instead of an error
	if (always_runs_at_compile_time) {
		report_error(
			"Task function 0x%04x is associated with native module 0x%04x ('%s') which always runs at compile-time "
			"so the task function will never run",
			task_function.uid.get_task_function_id(),
			native_module.uid.get_module_id(),
			native_module.name.get_string());
		return false;
	}

	// Make sure the same native module doesn't map to two task functions
	if (g_task_function_registry_data.task_function_mappings[native_module_handle.get_data()].is_valid()) {
		report_error(
			"Failed to register task function 0x%04x because the associated native module 0x%04x ('%s') "
			"is already mapped to another task function",
			task_function.uid.get_task_function_id(),
			native_module.uid.get_module_id(),
			native_module.name.get_string());
		return false;
	}

	// $TODO $PLUGIN validate that the library UID on all tasks matches the library UID on the native module
	if (!validate_task_function_mapping(native_module, task_function)) {
		return false;
	}

	uint32 index = cast_integer_verify<uint32>(g_task_function_registry_data.task_functions.size());

	// Append the native module
	g_task_function_registry_data.task_functions.push_back(task_function);
	g_task_function_registry_data.task_function_uids_to_indices.insert(std::make_pair(task_function.uid, index));

	g_task_function_registry_data.task_function_mappings[native_module_handle.get_data()] = task_function.uid;

	return true;
}

uint32 c_task_function_registry::get_task_function_count() {
	return cast_integer_verify<uint32>(g_task_function_registry_data.task_functions.size());
}

uint32 c_task_function_registry::get_task_function_index(s_task_function_uid task_function_uid) {
	auto it = g_task_function_registry_data.task_function_uids_to_indices.find(task_function_uid);
	if (it == g_task_function_registry_data.task_function_uids_to_indices.end()) {
		return k_invalid_task_function_index;
	} else {
		return it->second;
	}
}

const s_task_function &c_task_function_registry::get_task_function(uint32 index) {
	wl_assert(valid_index(index, get_task_function_count()));
	return g_task_function_registry_data.task_functions[index];
}

s_task_function_uid c_task_function_registry::get_task_function_mapping(h_native_module native_module_handle) {
	wl_assert(valid_index(native_module_handle.get_data(), c_native_module_registry::get_native_module_count()));
	return g_task_function_registry_data.task_function_mappings[native_module_handle.get_data()];
}

static bool validate_task_function_mapping(const s_native_module &native_module, const s_task_function &task_function) {
	// Keep track of whether each task argument has been mapped to
	s_static_array<bool, k_max_task_function_arguments> task_argument_mappings;
	zero_type(&task_argument_mappings);

	// Ensure that all indices are valid and that all mapping types match
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		const s_native_module_argument &native_module_argument = native_module.arguments[arg];

		// Make sure we're mapping to a valid task argument index
		uint32 mapping_index = task_function.task_function_argument_indices[arg];
		if (mapping_index == k_invalid_task_argument_index) {
			// This native module argument wasn't used by the task function.
			if (native_module_argument.argument_direction == e_native_module_argument_direction::k_out) {
				// Output arguments must always be mapped
				report_error(
					"Native module 0x%04x ('%s') out argument '%s' is not mapped to task function 0x%04x",
					native_module.uid.get_module_id(),
					native_module.name.get_string(),
					native_module_argument.name.get_string(),
					task_function.uid.get_task_function_id());
				return false;
			}

			continue;
		}

		if (!valid_index(mapping_index, task_function.argument_count)) {
			report_error(
				"Native module 0x%04x ('%s') argument '%s' does not map to a valid argument of task function 0x%04x",
				native_module.uid.get_module_id(),
				native_module.name.get_string(),
				native_module_argument.name.get_string(),
				task_function.uid.get_task_function_id());
			return false;
		}

		if (task_argument_mappings[mapping_index]) {
			report_error(
				"Native module 0x%04x ('%s') argument '%s' maps to task function 0x%04x argument %u "
				"which is mapped to by another native module argument",
				native_module.uid.get_module_id(),
				native_module.name.get_string(),
				native_module_argument.name.get_string(),
				task_function.uid.get_task_function_id(),
				mapping_index);
			return false;
		}

		task_argument_mappings[mapping_index] = true;

		const s_task_function_argument &task_function_argument = task_function.arguments[mapping_index];

		if (native_module_argument.argument_direction == e_native_module_argument_direction::k_in) {
			if (task_function_argument.argument_direction != e_task_argument_direction::k_in) {
				report_error(
					"Native module 0x%04x ('%s') in argument '%s' "
					"does not map to an in argument of task function 0x%04x",
					native_module.uid.get_module_id(),
					native_module.name.get_string(),
					native_module_argument.name.get_string(),
					task_function.uid.get_task_function_id());
				return false;
			}
		} else {
			wl_assert(native_module_argument.argument_direction == e_native_module_argument_direction::k_out);
			if (task_function_argument.argument_direction != e_task_argument_direction::k_out) {
				report_error(
					"Native module 0x%04x ('%s') out argument '%s' "
					"does not map to an out argument of task function 0x%04x",
					native_module.uid.get_module_id(),
					native_module.name.get_string(),
					native_module_argument.name.get_string(),
					task_function.uid.get_task_function_id());
				return false;
			}

			// Task outputs can't be arrays
			if (task_function_argument.type.is_array()) {
				report_error(
					"Task function 0x%04x contains an out argument of an array type which is illegal",
					task_function.uid.get_task_function_id());
				return false;
			}
		}

		// For all qualifier types, the primitive type and whether it's an array must match
		e_task_primitive_type expected_primitive_type =
			task_primitive_type_from_native_module_primitive_type(native_module_argument.type.get_primitive_type());
		bool type_mapping_valid =
			task_function_argument.type.get_primitive_type() == expected_primitive_type
			&& task_function_argument.type.is_array() == native_module_argument.type.is_array();

		if (type_mapping_valid) {
			switch (native_module_argument.type.get_data_mutability()) {
			case e_native_module_data_mutability::k_variable:
				type_mapping_valid =
					task_function_argument.type.get_data_mutability() == e_task_data_mutability::k_variable;
				break;

			case e_native_module_data_mutability::k_constant:
				// Constants can be mapped to task function buffers - the buffer simply holds a constant value

			case e_native_module_data_mutability::k_dependent_constant:
				// Dependent constants can be variable, so the task function type must be a buffer
				type_mapping_valid =
					task_function_argument.type.get_data_mutability() == e_task_data_mutability::k_variable;
				break;

			default:
				wl_unreachable();
			}
		}

		if (!type_mapping_valid) {
			report_error(
				"Native module 0x%04x ('%s') argument '%s' maps to task function 0x%04x argument %u "
				"with an incompatible type ('%s' vs. '%s')",
				native_module.uid.get_module_id(),
				native_module.name.get_string(),
				native_module_argument.name.get_string(),
				task_function.uid.get_task_function_id(),
				mapping_index,
				native_module_argument.type.to_string().c_str(),
				task_function_argument.type.to_string().c_str());
			return false;
		}
	}

	for (size_t task_arg = 0; task_arg < task_function.argument_count; task_arg++) {
		if (!task_argument_mappings[task_arg]) {
			report_error(
				"Task function 0x%04x argument %llu has no mapping from native module 0x%04x ('%s')",
				task_function.uid.get_task_function_id(),
				task_arg,
				native_module.uid.get_module_id(),
				native_module.name.get_string());
			return false;
		}
	}

	return true;
}
