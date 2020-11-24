#include "engine/task_function_registry.h"

#include <vector>
#include <unordered_map>

// $TODO $PLUGIN error reporting for registration errors. Important when we have plugins - we will want a global error
// reporting system for that.

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

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_task_function_mapping(const s_native_module &native_module, const s_task_function &task_function);

static constexpr e_task_primitive_type k_native_module_primitive_type_to_task_primitive_type_mapping[] = {
	e_task_primitive_type::k_real,	// e_native_module_primitive_type::k_real
	e_task_primitive_type::k_bool,	// e_native_module_primitive_type::k_bool
	e_task_primitive_type::k_string	// e_native_module_primitive_type::k_string
};
static_assert(array_count(k_native_module_primitive_type_to_task_primitive_type_mapping) ==
	enum_count<e_native_module_primitive_type>(),
	"Native module primitive type to task primitive type mismatch");

static e_task_primitive_type convert_native_module_primitive_type_to_task_primitive_type(
	e_native_module_primitive_type native_module_primitive_type) {
	wl_assert(valid_enum_index(native_module_primitive_type));
	return k_native_module_primitive_type_to_task_primitive_type_mapping[enum_index(native_module_primitive_type)];
}
#endif // IS_TRUE(ASSERTS_ENABLED)

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
		g_task_function_registry_data.task_function_mappings[index] = s_task_function_uid::k_invalid;
	}

	g_task_function_registry_state = e_task_function_registry_state::k_registering;
}

bool c_task_function_registry::end_registration() {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_registering);
	g_task_function_registry_state = e_task_function_registry_state::k_finalized;
	return true;
}

bool c_task_function_registry::register_task_function_library(const s_task_function_library &library) {
	wl_assert(g_task_function_registry_state == e_task_function_registry_state::k_registering);

	// Make sure there isn't already a library registered with this ID
	auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(library.id);
	if (it != g_task_function_registry_data.task_function_library_ids_to_indices.end()) {
		return false;
	}

	// Validate that there is a matching native module library
	if (!c_native_module_registry::is_native_module_library_registered(library.id)) {
		return false;
	}

	// Make sure the details of the library are consistent
	h_native_module_library native_module_library_handle =
		c_native_module_registry::get_native_module_library_handle(library.id);
	const s_native_module_library &native_module_library =
		c_native_module_registry::get_native_module_library(native_module_library_handle);
	if (library.name != native_module_library.name || library.version != native_module_library.version) {
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
	wl_assert(task_function.function);
	wl_assert(task_function.argument_count <= k_max_task_function_arguments);
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t arg = 0; arg < task_function.argument_count; arg++) {
		wl_assert(task_function.argument_types[arg].is_valid());
		wl_assert(task_function.argument_types[arg].is_legal());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// $TODO $PLUGIN Check that the library referenced by the UID is registered

	// Check that the library referenced by the UID is registered
	{
		auto it = g_task_function_registry_data.task_function_library_ids_to_indices.find(
			task_function.uid.get_library_id());
		if (it == g_task_function_registry_data.task_function_library_ids_to_indices.end()) {
			return false;
		}
	}

	// Check that the UID isn't already in use
	if (g_task_function_registry_data.task_function_uids_to_indices.find(task_function.uid) !=
		g_task_function_registry_data.task_function_uids_to_indices.end()) {
		return false;
	}

	// Look up the native module
	h_native_module native_module_handle =
		c_native_module_registry::get_native_module_handle(task_function.native_module_uid);
	if (!native_module_handle.is_valid()) {
		return false;
	}

	// Make sure the same native module doesn't map to two task functions
	if (g_task_function_registry_data.task_function_mappings[native_module_handle.get_data()]
		!= s_task_function_uid::k_invalid) {
		return false;
	}

	// $TODO $PLUGIN validate that the library UID on all tasks matches the library UID on the native module
#if IS_TRUE(ASSERTS_ENABLED)
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_handle);
	validate_task_function_mapping(native_module, task_function);
#endif // IS_TRUE(ASSERTS_ENABLED)

	uint32 index = cast_integer_verify<uint32>(g_task_function_registry_data.task_functions.size());

	// Append the native module
	g_task_function_registry_data.task_functions.push_back(task_function);
	g_task_function_registry_data.task_function_uids_to_indices.insert(std::make_pair(task_function.uid, index));

	g_task_function_registry_data.task_function_mappings[native_module_handle] = task_function.uid;

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

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_task_function_mapping(const s_native_module &native_module, const s_task_function &task_function) {
	// Keep track of whether each task argument has been mapped to
	s_static_array<bool, k_max_task_function_arguments> task_argument_mappings;
	zero_type(&task_argument_mappings);

	// Ensure that all indices are valid and that all mapping types match
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		// Make sure we're mapping to a valid task argument index
		uint32 mapping_index = task_function.task_function_argument_indices[arg];
		if (mapping_index == k_invalid_task_argument_index) {
			// This native module argument wasn't used by the task function.
			continue;
		}

		wl_assert(valid_index(mapping_index, task_function.argument_count));
		wl_assert(!task_argument_mappings[mapping_index]);
		task_argument_mappings[mapping_index] = true;

		c_native_module_data_type native_module_type = native_module.arguments[arg].type.get_data_type();
		c_task_qualified_data_type task_type = task_function.argument_types[mapping_index];

		// For all qualifier types, the primitive type and whether it's an array must match
		wl_assert(task_type.get_data_type().get_primitive_type() ==
			convert_native_module_primitive_type_to_task_primitive_type(native_module_type.get_primitive_type()));
		wl_assert(task_type.get_data_type().is_array() == native_module_type.is_array());

		e_native_module_qualifier native_module_qualifier = native_module.arguments[arg].type.get_qualifier();
		e_task_qualifier task_qualifier = task_type.get_qualifier();
		switch (native_module_qualifier) {
		case e_native_module_qualifier::k_in:
			wl_assert(task_qualifier == e_task_qualifier::k_in);
			break;

		case e_native_module_qualifier::k_out:
			wl_assert(task_qualifier == e_task_qualifier::k_out);
			wl_assert(!native_module_type.is_array());
			break;

		case e_native_module_qualifier::k_constant:
			wl_assert(task_qualifier == e_task_qualifier::k_in || task_qualifier == e_task_qualifier::k_constant);
			break;

		default:
			wl_unreachable();
		}
	}

	for (size_t task_arg = 0; task_arg < task_function.argument_count; task_arg++) {
		wl_assert(task_argument_mappings[task_arg]);
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)
