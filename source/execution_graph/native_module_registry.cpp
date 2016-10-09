#include "execution_graph/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <unordered_map>

// Strings for native module registry output
static const char *k_native_module_no_return_type_string = "void";

static const char *k_native_module_primitive_type_strings[] = {
	"real",		// k_native_module_primitive_type_real
	"bool",		// k_native_module_primitive_type_bool
	"string"	// k_native_module_primitive_type_string
};
static_assert(NUMBEROF(k_native_module_primitive_type_strings) == k_native_module_primitive_type_count,
	"Native module primitive type string mismatch");

static const char *k_native_module_qualifier_strings[] = {
	"in",	// k_native_module_qualifier_in
	"out",	// k_native_module_qualifier_out
	"const"	// k_native_module_qualifier_constant
};
static_assert(NUMBEROF(k_native_module_qualifier_strings) == k_native_module_qualifier_count,
	"Native module qualifier string mismatch");

// $TODO $PLUGIN error reporting for registration errors. Important when we have plugins - we will want a global error
// reporting system for that.

enum e_native_module_registry_state {
	k_native_module_registry_state_uninitialized,
	k_native_module_registry_state_initialized,
	k_native_module_registry_state_registering,
	k_native_module_registry_state_finalized,

	k_native_module_registry_state_count,
};

struct s_native_module_library {
	c_static_string<k_max_native_module_library_name_length> library_name;
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
	std::unordered_map<uint32, s_native_module_library> native_module_libraries;

	std::vector<s_native_module> native_modules;
	std::unordered_map<s_native_module_uid, uint32> native_module_uids_to_indices;
	s_static_array<s_native_operator, k_native_operator_count> native_operators;

	bool optimizations_enabled;
	std::vector<s_native_module_optimization_rule> optimization_rules;
};

static e_native_module_registry_state g_native_module_registry_state = k_native_module_registry_state_uninitialized;
static s_native_module_registry_data g_native_module_registry_data;

static bool do_native_modules_conflict(const s_native_module &native_module_a, const s_native_module &native_module_b);

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule);
#else // IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {}
#endif // IS_TRUE(ASSERTS_ENABLED)

void c_native_module_registry::initialize() {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_uninitialized);

	ZERO_STRUCT(&g_native_module_registry_data.native_operators);
	g_native_module_registry_data.optimizations_enabled = false;

	g_native_module_registry_state = k_native_module_registry_state_initialized;
}

void c_native_module_registry::shutdown() {
	wl_assert(g_native_module_registry_state != k_native_module_registry_state_uninitialized);

	// Clear and free all memory
	g_native_module_registry_data.native_module_libraries.clear();
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

bool c_native_module_registry::register_native_module_library(uint32 library_id, const char *library_name) {
	wl_assert(g_native_module_registry_state == k_native_module_registry_state_registering);

	// Make sure there isn't already a library registered with this ID
	auto it = g_native_module_registry_data.native_module_libraries.find(library_id);
	if (it != g_native_module_registry_data.native_module_libraries.end()) {
		return false;
	}

	s_native_module_library library;
	library.library_name.set_verify(library_name);
	g_native_module_registry_data.native_module_libraries.insert(std::make_pair(library_id, library));
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
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		wl_assert(VALID_INDEX(native_module.arguments[arg].qualifier, k_native_module_qualifier_count));
		wl_assert(native_module.arguments[arg].type.is_valid());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Check that the library referenced by the UID is registered
	{
		auto it = g_native_module_registry_data.native_module_libraries.find(native_module.uid.get_library_id());
		if (it == g_native_module_registry_data.native_module_libraries.end()) {
			return false;
		}
	}

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

bool c_native_module_registry::output_registered_native_modules(const char *filename) {
	std::ofstream out(filename);

	if (!out.is_open()) {
		return false;
	}

	struct s_output_item {
		enum e_type {
			k_type_library,
			k_type_native_module,

			k_type_count
		};

		e_type type;
		s_native_module_uid uid; // Doubles as just library ID if this is a library

		bool operator<(const s_output_item &other) const {
			if (uid.get_library_id() != other.uid.get_library_id()) {
				return uid.get_library_id() < other.uid.get_library_id();
			} else {
				if (type == k_type_native_module && other.type == k_type_native_module) {
					return uid.get_module_id() < other.uid.get_module_id();
				} else {
					// The library always comes first
					return (other.type == k_type_native_module);
				}
			}
		}
	};

	std::vector<s_output_item> output_items;
	output_items.reserve(g_native_module_registry_data.native_module_libraries.size() +
		g_native_module_registry_data.native_modules.size());

	for (auto it = g_native_module_registry_data.native_module_libraries.begin();
		 it != g_native_module_registry_data.native_module_libraries.end();
		 it++) {
		s_output_item item;
		item.type = s_output_item::k_type_library;
		item.uid = s_native_module_uid::build(it->first, 0);
		output_items.push_back(item);
	}

	for (size_t index = 0; index < g_native_module_registry_data.native_modules.size(); index++) {
		s_output_item item;
		item.type = s_output_item::k_type_native_module;
		item.uid = g_native_module_registry_data.native_modules[index].uid;
		output_items.push_back(item);
	}

	std::sort(output_items.begin(), output_items.end());

	bool first_output = false;
	for (size_t index = 0; index < output_items.size(); index++) {
		const s_output_item &item = output_items[index];

		switch (item.type) {
		case s_output_item::k_type_library:
		{
			if (!first_output) {
				out << "\n";
			}

			const s_native_module_library &library =
				g_native_module_registry_data.native_module_libraries[item.uid.get_library_id()];
			out << library.library_name.get_string() << ":\n";
			break;
		}

		case s_output_item::k_type_native_module:
		{
			uint32 native_module_index = get_native_module_index(item.uid);
			const s_native_module &native_module = get_native_module(native_module_index);

			out << "\t";

			if (!native_module.first_output_is_return) {
				// Return type of void is a special case
				out << k_native_module_no_return_type_string;
			} else {
				wl_assert(native_module.out_argument_count > 0);
				for (size_t arg = 0; arg < native_module.argument_count; arg++) {
					const s_native_module_argument &argument = native_module.arguments[arg];
					if (argument.qualifier == k_native_module_qualifier_out) {
						out << k_native_module_primitive_type_strings[argument.type.get_primitive_type()];
						if (argument.type.is_array()) {
							out << "[]";
						}
						break;
					}
				}
			}

			out << " " << native_module.name.get_string() << "(";

			bool first_argument = true;
			bool return_type_argument_found = false;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				const s_native_module_argument &argument = native_module.arguments[arg];

				if (native_module.first_output_is_return &&
					!return_type_argument_found &&
					argument.qualifier == k_native_module_qualifier_out) {
					// Skip it - this argument is the return type
					return_type_argument_found = true;
					continue;
				}

				if (!first_argument) {
					out << ", ";
				}

				out << k_native_module_qualifier_strings[argument.qualifier] << " " <<
					k_native_module_primitive_type_strings[argument.type.get_primitive_type()];
				if (argument.type.is_array()) {
					out << "[]";
				}

				if (!native_module.argument_names[arg].is_empty()) {
					out << " " << native_module.argument_names[arg].get_string();
				}

				first_argument = false;
			}

			out << ")\n";

			break;
		}

		default:
			wl_unreachable();
		}

		first_output = false;
	}

	if (out.fail()) {
		return false;
	}

	return true;
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

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {
	// $TODO $PLUGIN validation
}
#endif // IS_TRUE(ASSERTS_ENABLED)