#include "engine/task_function_registry.h"

#include "execution_graph/native_module_registry.h"

#include <vector>
#include <unordered_map>

// $TODO $PLUGIN error reporting for registration errors. Important when we have plugins - we will want a global error
// reporting system for that.

enum e_task_function_registry_state {
	k_task_function_registry_state_uninitialized,
	k_task_function_registry_state_initialized,
	k_task_function_registry_state_registering,
	k_task_function_registry_state_finalized,

	k_task_function_registry_state_count,
};

struct s_task_function_mapping_list_internal {
	size_t first_mapping_index;
	size_t mapping_count;
};

template<>
struct std::hash<s_task_function_uid> {
	size_t operator()(const s_task_function_uid &key) const {
		return std::hash<uint64>()(key.data_uint64);
	}
};

struct s_task_function_registry_data {
	std::vector<s_task_function> task_functions;
	std::unordered_map<s_task_function_uid, uint32> task_function_uids_to_indices;

	// Each native module maps to N different tasks. The mapping is stored here
	std::vector<s_task_function_mapping_list_internal> task_function_mapping_lists;
	std::vector<s_task_function_mapping> task_function_mappings;
};

static e_task_function_registry_state g_task_function_registry_state = k_task_function_registry_state_uninitialized;
static s_task_function_registry_data g_task_function_registry_data;

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_task_function_mapping(
	const s_native_module &native_module, const s_task_function_mapping &task_function_mapping);
#endif // IS_TRUE(ASSERTS_ENABLED)

static const e_task_primitive_type k_native_module_primitive_type_to_task_primitive_type_mapping[] = {
	k_task_primitive_type_real,		// k_native_module_primitive_type_real
	k_task_primitive_type_bool,		// k_native_module_primitive_type_bool
	k_task_primitive_type_string	// k_native_module_primitive_type_string
};
static_assert(NUMBEROF(k_native_module_primitive_type_to_task_primitive_type_mapping) ==
	k_native_module_primitive_type_count,
	"Native module primitive type to task primitive type mismatch");

static e_task_primitive_type convert_native_module_primitive_type_to_task_primitive_type(
	e_native_module_primitive_type native_module_primitive_type) {
	wl_assert(VALID_INDEX(native_module_primitive_type, k_native_module_primitive_type_count));
	return k_native_module_primitive_type_to_task_primitive_type_mapping[native_module_primitive_type];
}

void c_task_function_registry::initialize() {
	wl_assert(g_task_function_registry_state == k_task_function_registry_state_uninitialized);
	g_task_function_registry_state = k_task_function_registry_state_initialized;
}

void c_task_function_registry::shutdown() {
	wl_assert(g_task_function_registry_state != k_task_function_registry_state_uninitialized);

	// Clear and free all memory
	g_task_function_registry_data.task_functions.clear();
	g_task_function_registry_data.task_functions.shrink_to_fit();
	g_task_function_registry_data.task_function_uids_to_indices.clear();

	g_task_function_registry_data.task_function_mapping_lists.clear();
	g_task_function_registry_data.task_function_mapping_lists.shrink_to_fit();
	g_task_function_registry_data.task_function_mappings.clear();
	g_task_function_registry_data.task_function_mappings.shrink_to_fit();

	g_task_function_registry_state = k_task_function_registry_state_uninitialized;
}

void c_task_function_registry::begin_registration() {
	wl_assert(g_task_function_registry_state == k_task_function_registry_state_initialized);

	// Clear all task mappings initially
	uint32 native_module_count = c_native_module_registry::get_native_module_count();
	g_task_function_registry_data.task_function_mapping_lists.resize(native_module_count);
	for (uint32 index = 0; index < native_module_count; index++) {
		ZERO_STRUCT(&g_task_function_registry_data.task_function_mapping_lists[index]);
	}

	g_task_function_registry_state = k_task_function_registry_state_registering;
}

bool c_task_function_registry::end_registration() {
	wl_assert(g_task_function_registry_state == k_task_function_registry_state_registering);
	g_task_function_registry_state = k_task_function_registry_state_finalized;
	return true;
}

bool c_task_function_registry::register_task_function(const s_task_function &task_function) {
	wl_assert(g_task_function_registry_state == k_task_function_registry_state_registering);

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

	// Check that the UID isn't already in use
	if (g_task_function_registry_data.task_function_uids_to_indices.find(task_function.uid) !=
		g_task_function_registry_data.task_function_uids_to_indices.end()) {
		return false;
	}

	uint32 index = cast_integer_verify<uint32>(g_task_function_registry_data.task_functions.size());

	// Append the native module
	g_task_function_registry_data.task_functions.push_back(s_task_function());
	memcpy(&g_task_function_registry_data.task_functions.back(), &task_function, sizeof(task_function));

	g_task_function_registry_data.task_function_uids_to_indices.insert(
		std::make_pair(task_function.uid, index));

	return true;
}

bool c_task_function_registry::register_task_function_mapping_list(
	s_native_module_uid native_module_uid,
	c_task_function_mapping_list task_function_mapping_list) {
	wl_assert(g_task_function_registry_state == k_task_function_registry_state_registering);

	// Look up the native module
	uint32 native_module_index = c_native_module_registry::get_native_module_index(native_module_uid);
	if (native_module_index == k_invalid_native_module_index) {
		return false;
	}

	s_task_function_mapping_list_internal &list =
		g_task_function_registry_data.task_function_mapping_lists[native_module_index];
	// Don't double-register
	wl_assert(list.mapping_count == 0);

#if IS_TRUE(ASSERTS_ENABLED)
	const s_native_module &native_module = c_native_module_registry::get_native_module(native_module_index);
	// Validate each mapping
	for (size_t index = 0; index < task_function_mapping_list.get_count(); index++) {
		validate_task_function_mapping(native_module, task_function_mapping_list[index]);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Append to the global list of mappings
	list.first_mapping_index = g_task_function_registry_data.task_function_mappings.size();
	list.mapping_count = task_function_mapping_list.get_count();

	for (size_t index = 0; index < list.mapping_count; index++) {
		g_task_function_registry_data.task_function_mappings.push_back(task_function_mapping_list[index]);
	}

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
	wl_assert(VALID_INDEX(index, get_task_function_count()));
	return g_task_function_registry_data.task_functions[index];
}

c_task_function_mapping_list c_task_function_registry::get_task_function_mapping_list(uint32 native_module_index) {
	wl_assert(VALID_INDEX(native_module_index, c_native_module_registry::get_native_module_count()));
	const s_task_function_mapping_list_internal &list =
		g_task_function_registry_data.task_function_mapping_lists[native_module_index];
	return c_task_function_mapping_list((list.mapping_count == 0) ?
		nullptr : &g_task_function_registry_data.task_function_mappings[list.first_mapping_index],
		list.mapping_count);
}

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_task_function_mapping(
	const s_native_module &native_module, const s_task_function_mapping &task_function_mapping) {
	uint32 task_function_index =
		c_task_function_registry::get_task_function_index(task_function_mapping.task_function_uid);
	wl_assert(task_function_index != k_invalid_task_function_index);

	const s_task_function &task_function = c_task_function_registry::get_task_function(task_function_index);

	// Keep track of whether each task argument has been mapped to. Inout arguments are mapped to twice, which is why we
	// use two arrays.
	s_static_array<bool, k_max_task_function_arguments> task_argument_input_mappings;
	s_static_array<bool, k_max_task_function_arguments> task_argument_output_mappings;
	ZERO_STRUCT(&task_argument_input_mappings);
	ZERO_STRUCT(&task_argument_output_mappings);

	// Ensure that all indices are valid and that all mapping types match
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		e_task_function_mapping_native_module_input_type input_type =
			task_function_mapping.native_module_input_types[arg];

		// Make sure we're mapping to a valid task argument index
		uint32 mapping_index = task_function_mapping.native_module_argument_mapping.mapping[arg];
		wl_assert(VALID_INDEX(mapping_index, task_function.argument_count));

		c_native_module_data_type native_module_type = native_module.arguments[arg].type.get_data_type();
		c_task_qualified_data_type task_type = task_function.argument_types[mapping_index];

		// For all qualifier types, the primitive type and whether it's an array must match
		wl_assert(task_type.get_data_type().get_primitive_type() ==
			convert_native_module_primitive_type_to_task_primitive_type(native_module_type.get_primitive_type()));
		wl_assert(task_type.get_data_type().is_array() == native_module_type.is_array());

		if (native_module_qualifier_is_input(native_module.arguments[arg].type.get_qualifier())) {
			// Input arguments should have a valid input mapping
			wl_assert(
				input_type == k_task_function_mapping_native_module_input_type_variable ||
				input_type == k_task_function_mapping_native_module_input_type_branchless_variable);

			// Make sure the native module argument type matches up with what it's being mapped to
			wl_assert(task_type.get_qualifier() == k_task_qualifier_in ||
				task_type.get_qualifier() == k_task_qualifier_inout);

			wl_assert(!task_argument_input_mappings[mapping_index]);
			task_argument_input_mappings[mapping_index] = true;
		} else {
			wl_assert(native_module.arguments[arg].type.get_qualifier() == k_native_module_qualifier_out);
			// Output arguments should be "ignored" in the mapping
			wl_assert(input_type == k_task_function_mapping_native_module_input_type_none);

			// Make sure the native module argument type matches up with what it's being mapped to
			wl_assert(!native_module_type.is_array());
			wl_assert(task_type.get_qualifier() == k_task_qualifier_out ||
				task_type.get_qualifier() == k_task_qualifier_inout);

			wl_assert(!task_argument_output_mappings[mapping_index]);
			task_argument_output_mappings[mapping_index] = true;
		}
	}

	for (size_t task_arg = 0; task_arg < task_function.argument_count; task_arg++) {
		bool in_mapped = task_argument_input_mappings[task_arg];
		bool out_mapped = task_argument_output_mappings[task_arg];

		c_task_qualified_data_type type = task_function.argument_types[task_arg];
		switch (type.get_qualifier()) {
		case k_task_qualifier_in:
			wl_assert(in_mapped);
			wl_assert(!out_mapped);
			break;

		case k_task_qualifier_out:
			wl_assert(!in_mapped);
			wl_assert(out_mapped);
			break;

		case k_task_qualifier_inout:
			wl_assert(in_mapped);
			wl_assert(out_mapped);
			break;

		default:
			wl_unreachable();
		}
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)
