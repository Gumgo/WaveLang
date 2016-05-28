#include "execution_graph/native_module_registry.h"
#include <vector>
#include <unordered_map>

// $TODO $PLUGIN error reporting for registration errors. Important when we have plugins - we will want a global error
// reporting system for that.

enum e_native_module_registry_state {
	k_native_module_registry_state_uninitialized,
	k_native_module_registry_state_initialized,
	k_native_module_registry_state_registering,
	k_native_module_registry_state_finalized,

	k_native_module_registry_state_count,
};

struct s_native_operator {
	c_static_string<k_max_native_module_name_length> native_module_name;
};

template<>
struct std::hash<s_native_module_uid> {
	size_t operator()(const s_native_module_uid &key) const {
		return std::hash<uint64>()(key.data_uint64);
	}
};

struct s_native_module_registry_data {
	std::vector<s_native_module> native_modules;
	std::unordered_map<s_native_module_uid, uint32> native_module_uids_to_indices;
	s_native_operator native_operators[k_native_operator_count];

	bool optimizations_enabled;
	std::vector<s_native_module_optimization_rule> optimization_rules;
};

static e_native_module_registry_state g_native_module_registry_state = k_native_module_registry_state_uninitialized;
static s_native_module_registry_data g_native_module_registry_data;

static bool do_native_modules_conflict(const s_native_module &native_module_a, const s_native_module &native_module_b);

#if PREDEFINED(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule);
#else // PREDEFINED(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {}
#endif // PREDEFINED(ASSERTS_ENABLED)

void c_native_module_registry::initialize() {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_uninitialized);

	ZERO_STRUCT(&g_native_module_registry_data.native_operators);
	g_native_module_registry_data.optimizations_enabled = false;

	g_native_module_registry_state = k_native_module_registry_state_initialized;
}

void c_native_module_registry::shutdown() {
	wl_assert(g_native_module_registry_state != k_native_module_registry_state_uninitialized);

	// Clear and free all memory
	g_native_module_registry_data.native_modules.clear();
	g_native_module_registry_data.native_modules.shrink_to_fit();
	g_native_module_registry_data.native_module_uids_to_indices.clear();

	g_native_module_registry_state = k_native_module_registry_state_uninitialized;
}

void c_native_module_registry::begin_registration(bool optimizations_enabled) {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_initialized);
	g_native_module_registry_state = k_native_module_registry_state_registering;

	g_native_module_registry_data.optimizations_enabled = optimizations_enabled;
}

bool c_native_module_registry::end_registration() {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_registering);

	// Make sure all the native operators are set and point to existing native modules. Don't bother checking arguments
	// here though, errors will show up as argument mismatches during compilation.
	for (uint32 index = 0; index < k_native_operator_count; index++) {
		if (g_native_module_registry_data.native_operators[index].native_module_name.is_empty()) {
			return false;
		}

		bool found = false;
		for (uint32 native_module_index = 0;
			 !found && native_module_index < g_native_module_registry_data.native_modules.size();
			 native_module_index++) {
			found = (g_native_module_registry_data.native_modules[native_module_index].name ==
				g_native_module_registry_data.native_operators[index].native_module_name);
		}

		if (!found) {
			return false;
		}
	}

	g_native_module_registry_state = k_native_module_registry_state_finalized;
	return true;
}

bool c_native_module_registry::register_native_module(const s_native_module &native_module) {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_registering);

	// Validate that the native module is set up correctly
	native_module.name.validate();
	wl_assert(!native_module.name.is_empty());
	wl_assert(native_module.argument_count <= k_max_native_module_arguments);
	wl_assert(native_module.argument_count == native_module.in_argument_count + native_module.out_argument_count);
	wl_assert(!native_module.first_output_is_return || native_module.out_argument_count > 0);
#if PREDEFINED(ASSERTS_ENABLED)
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		wl_assert(VALID_INDEX(native_module.arguments[arg].qualifier, k_native_module_qualifier_count));
		wl_assert(native_module.arguments[arg].type.is_valid());
	}
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Check that the UID isn't already in use
	if (g_native_module_registry_data.native_module_uids_to_indices.find(native_module.uid) !=
		g_native_module_registry_data.native_module_uids_to_indices.end()) {
		return false;
	}

	// Check that this native module isn't identical to an existing one
	for (uint32 other_index = 0; other_index < g_native_module_registry_data.native_modules.size(); other_index++) {
		const s_native_module &other_native_module = g_native_module_registry_data.native_modules[other_index];

		if (do_native_modules_conflict(native_module, other_native_module)) {
			return false;
		}
	}

	uint32 index = cast_integer_verify<uint32>(g_native_module_registry_data.native_modules.size());

	// Append the native module
	g_native_module_registry_data.native_modules.push_back(s_native_module());
	memcpy(&g_native_module_registry_data.native_modules.back(), &native_module, sizeof(native_module));

	g_native_module_registry_data.native_module_uids_to_indices.insert(
		std::make_pair(native_module.uid, index));

	return true;
}

void c_native_module_registry::register_native_operator(
	e_native_operator native_operator, const char *native_module_name) {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_registering);

	wl_assert(VALID_INDEX(native_operator, k_native_operator_count));
	wl_assert(strlen(native_module_name) <= k_max_native_module_name_length - 1);
	g_native_module_registry_data.native_operators[native_operator].native_module_name.set_verify(native_module_name);
}

void c_native_module_registry::register_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_registering);

	if (g_native_module_registry_data.optimizations_enabled) {
		validate_optimization_rule(optimization_rule);
		g_native_module_registry_data.optimization_rules.push_back(optimization_rule);
	} else {
		// Just ignore it. In the runtime, we don't need to store optimizations.
	}
}

uint32 c_native_module_registry::get_native_module_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.native_modules.size());
}

uint32 c_native_module_registry::get_native_module_index(s_native_module_uid native_module_uid) {
	auto it = g_native_module_registry_data.native_module_uids_to_indices.find(native_module_uid);
	if (it == g_native_module_registry_data.native_module_uids_to_indices.end()) {
		return k_invalid_native_module_index;
	} else {
		return it->second;
	}
}

const s_native_module &c_native_module_registry::get_native_module(uint32 index) {
	wl_assert(VALID_INDEX(index, get_native_module_count()));
	return g_native_module_registry_data.native_modules[index];
}

const char *c_native_module_registry::get_native_module_for_native_operator(e_native_operator native_operator) {
	wl_assert(VALID_INDEX(native_operator, k_native_operator_count));
	return g_native_module_registry_data.native_operators[native_operator].native_module_name.get_string();
}

uint32 c_native_module_registry::get_optimization_rule_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.optimization_rules.size());
}

const s_native_module_optimization_rule &c_native_module_registry::get_optimization_rule(uint32 index) {
	wl_assert(VALID_INDEX(index, get_optimization_rule_count()));
	return g_native_module_registry_data.optimization_rules[index];
}

static bool do_native_modules_conflict(const s_native_module &native_module_a, const s_native_module &native_module_b) {
	if (native_module_a.name != native_module_b.name) {
		return false;
	}

	// If the arguments match, it's an overload conflict. We currently consider only types when detecting overload
	// conflicts, because using qualifiers would cause ambiguity. The return value cannot be used to resolve overloads,
	// so we need to do a bit of work if the first output argument is used as the return value.

	size_t argument_count_a = native_module_a.first_output_is_return ?
		native_module_a.argument_count - 1 : native_module_a.argument_count;
	size_t argument_count_b = native_module_b.first_output_is_return ?
		native_module_b.argument_count - 1 : native_module_b.argument_count;

	if (argument_count_a != argument_count_b) {
		return false;
	}

	size_t arg_a_index = 0;
	size_t arg_b_index = 0;
	bool first_out_arg_found_a = false;
	bool first_out_arg_found_b = false;

	bool all_match = true;
	for (size_t arg = 0; all_match && arg < argument_count_a; arg++) {
		// Skip the first output argument we find if it's used as a return value

		if (native_module_a.first_output_is_return &&
			native_module_a.arguments[arg_a_index].qualifier == k_native_module_qualifier_out &&
			!first_out_arg_found_a) {
			arg_a_index++;
			first_out_arg_found_a = true;
		}

		if (native_module_b.first_output_is_return &&
			native_module_b.arguments[arg_b_index].qualifier == k_native_module_qualifier_out &&
			!first_out_arg_found_b) {
			arg_b_index++;
			first_out_arg_found_b = true;
		}

		if (native_module_a.arguments[arg_a_index].type != native_module_b.arguments[arg_b_index].type) {
			all_match = false;
		}

		arg_a_index++;
		arg_b_index++;
	}

	return all_match;
}

#if PREDEFINED(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {
	// $TODO $PLUGIN validation
}
#endif // PREDEFINED(ASSERTS_ENABLED)