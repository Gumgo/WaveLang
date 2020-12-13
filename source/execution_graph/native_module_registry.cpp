#include "execution_graph/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <unordered_map>

// Strings for native module registry output
static constexpr const char *k_native_module_no_return_type_string = "void";

static constexpr const char *k_native_module_argument_direction_strings[] = {
	"in",	// e_native_module_argument_direction::k_in
	"out"	// e_native_module_argument_direction::k_out
};
STATIC_ASSERT(is_enum_fully_mapped<e_native_module_argument_direction>(k_native_module_argument_direction_strings));

// $TODO $PLUGIN error reporting for registration errors. Important when we have plugins - we will want a global error
// reporting system for that. Also, don't halt on error, return false instead

enum class e_native_module_registry_state {
	k_uninitialized,
	k_initialized,
	k_registering,
	k_finalized,

	k_count,
};

struct s_native_module_internal {
	s_native_module native_module;
	e_native_operator native_operator;
};

struct s_native_operator {
	c_static_string<k_max_native_module_name_length> native_module_name;
};

namespace std {
	template<>
	struct hash<s_native_module_uid> {
		size_t operator()(const s_native_module_uid &key) const {
			return std::hash<uint64>()(key.data_uint64);
		}
	};
}

struct s_native_module_registry_data {
	std::vector<s_native_module_library> native_module_libraries;
	std::unordered_map<uint32, uint32> native_module_library_ids_to_indices;

	std::vector<s_native_module_internal> native_modules;
	std::unordered_map<s_native_module_uid, uint32> native_module_uids_to_indices;
	s_static_array<s_native_operator, enum_count<e_native_operator>()> native_operators;

	bool optimizations_enabled;
	std::vector<s_native_module_optimization_rule> optimization_rules;
};

static e_native_module_registry_state g_native_module_registry_state = e_native_module_registry_state::k_uninitialized;
static s_native_module_registry_data g_native_module_registry_data;

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule);
#else // IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {}
#endif // IS_TRUE(ASSERTS_ENABLED)

void c_native_module_registry::initialize() {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_uninitialized);

	zero_type(&g_native_module_registry_data.native_operators);
	g_native_module_registry_data.optimizations_enabled = false;

	g_native_module_registry_state = e_native_module_registry_state::k_initialized;
}

void c_native_module_registry::shutdown() {
	wl_assert(g_native_module_registry_state != e_native_module_registry_state::k_uninitialized);

	// Clear and free all memory
	g_native_module_registry_data.native_module_libraries.clear();
	g_native_module_registry_data.native_module_libraries.shrink_to_fit();
	g_native_module_registry_data.native_module_library_ids_to_indices.clear();
	g_native_module_registry_data.native_modules.clear();
	g_native_module_registry_data.native_modules.shrink_to_fit();
	g_native_module_registry_data.native_module_uids_to_indices.clear();

	g_native_module_registry_state = e_native_module_registry_state::k_uninitialized;
}

void c_native_module_registry::begin_registration(bool optimizations_enabled) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_initialized);
	g_native_module_registry_state = e_native_module_registry_state::k_registering;

	g_native_module_registry_data.optimizations_enabled = optimizations_enabled;
}

bool c_native_module_registry::end_registration() {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	// Make sure all the native operators are set and point to existing native modules. Don't bother checking arguments
	// here though, errors will show up as argument mismatches during compilation.
	for (e_native_operator native_operator : iterate_enum<e_native_operator>()) {
		const s_native_operator &native_operator_data =
			g_native_module_registry_data.native_operators[enum_index(native_operator)];
		if (native_operator_data.native_module_name.is_empty()) {
			return false;
		}

		bool found = false;
		for (const s_native_module_internal &native_module : g_native_module_registry_data.native_modules) {
			if (native_module.native_module.name == native_operator_data.native_module_name) {
				found = true;
				break;
			}
		}

		if (!found) {
			return false;
		}
	}

	g_native_module_registry_state = e_native_module_registry_state::k_finalized;
	return true;
}

bool c_native_module_registry::register_native_module_library(const s_native_module_library &library) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	// Make sure there isn't already a library registered with this ID
	auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(library.id);
	if (it != g_native_module_registry_data.native_module_library_ids_to_indices.end()) {
		return false;
	}

	// $TODO $PLUGIN make sure there isn't a library with the same name

	uint32 index = cast_integer_verify<uint32>(g_native_module_registry_data.native_module_libraries.size());

	g_native_module_registry_data.native_module_libraries.push_back(library);
	g_native_module_registry_data.native_module_library_ids_to_indices.insert(std::make_pair(library.id, index));
	return true;
}

bool c_native_module_registry::is_native_module_library_registered(uint32 library_id) {
	auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(library_id);
	return it != g_native_module_registry_data.native_module_library_ids_to_indices.end();
}

h_native_module_library c_native_module_registry::get_native_module_library_handle(uint32 library_id) {
	wl_assert(is_native_module_library_registered(library_id));
	auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(library_id);
	return h_native_module_library::construct(it->second);
}

uint32 c_native_module_registry::get_native_module_library_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.native_module_libraries.size());
}

c_index_handle_iterator<h_native_module_library> c_native_module_registry::iterate_native_module_libraries() {
	return c_index_handle_iterator<h_native_module_library>(get_native_module_library_count());
}

const s_native_module_library &c_native_module_registry::get_native_module_library(h_native_module_library handle) {
	return g_native_module_registry_data.native_module_libraries[handle.get_data()];
}

bool c_native_module_registry::register_native_module(const s_native_module &native_module) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	// Validate that the native module is set up correctly
	native_module.name.validate();
	wl_assert(!native_module.name.is_empty());
	wl_assert(native_module.argument_count <= k_max_native_module_arguments);
	wl_assert(native_module.argument_count == native_module.in_argument_count + native_module.out_argument_count);
	wl_assert((native_module.return_argument_index == k_invalid_native_module_argument_index)
		|| (native_module.arguments[native_module.return_argument_index].argument_direction ==
			e_native_module_argument_direction::k_out));
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t arg = 0; arg < native_module.argument_count; arg++) {
		wl_assert(valid_enum_index(native_module.arguments[arg].argument_direction));
		wl_assert(native_module.arguments[arg].type.is_valid());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	if (!validate_native_module(native_module)) {
		return false;
	}

	s_native_module_internal native_module_internal;
	native_module_internal.native_module = native_module;

	// Check that the library referenced by the UID is registered
	{
		auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(
			native_module.uid.get_library_id());
		if (it == g_native_module_registry_data.native_module_library_ids_to_indices.end()) {
			return false;
		}
	}

	// Check that the UID isn't already in use
	if (g_native_module_registry_data.native_module_uids_to_indices.find(native_module.uid) !=
		g_native_module_registry_data.native_module_uids_to_indices.end()) {
		return false;
	}

	// Determine whether this native module is a native operator
	native_module_internal.native_operator = e_native_operator::k_invalid;
	for (e_native_operator native_operator : iterate_enum<e_native_operator>()) {
		if (native_module.name ==
			g_native_module_registry_data.native_operators[enum_index(native_operator)].native_module_name) {
			native_module_internal.native_operator = native_operator;
			break;
		}
	}

	uint32 index = cast_integer_verify<uint32>(g_native_module_registry_data.native_modules.size());

	// Append the native module
	g_native_module_registry_data.native_modules.push_back(native_module_internal);
	g_native_module_registry_data.native_module_uids_to_indices.insert(std::make_pair(native_module.uid, index));

	return true;
}

void c_native_module_registry::register_native_operator(
	e_native_operator native_operator,
	const char *native_module_name) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	wl_vassert(g_native_module_registry_data.native_modules.empty(),
		"Native operators must be registered before native modules");

	wl_assert(valid_enum_index(native_operator));
	wl_assert(strlen(native_module_name) <= k_max_native_module_name_length - 1);
	g_native_module_registry_data.native_operators[enum_index(native_operator)].native_module_name.set_verify(
		native_module_name);
}

e_native_operator c_native_module_registry::get_native_module_operator(s_native_module_uid native_module_uid) {
	h_native_module native_module_handle = get_native_module_handle(native_module_uid);
	wl_assert(native_module_handle.is_valid());
	return g_native_module_registry_data.native_modules[native_module_handle.get_data()].native_operator;
}

bool c_native_module_registry::register_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	if (g_native_module_registry_data.optimizations_enabled) {
		validate_optimization_rule(optimization_rule);
		g_native_module_registry_data.optimization_rules.push_back(optimization_rule);
		return true; // $TODO $PLUGIN
	} else {
		// Just ignore it. In the runtime, we don't need to store optimizations.
		return true;
	}
}

uint32 c_native_module_registry::get_native_module_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.native_modules.size());
}

c_index_handle_iterator<h_native_module> c_native_module_registry::iterate_native_modules() {
	return c_index_handle_iterator<h_native_module>(get_native_module_count());
}

h_native_module c_native_module_registry::get_native_module_handle(s_native_module_uid native_module_uid) {
	auto it = g_native_module_registry_data.native_module_uids_to_indices.find(native_module_uid);
	if (it == g_native_module_registry_data.native_module_uids_to_indices.end()) {
		return h_native_module::invalid();
	} else {
		return h_native_module::construct(it->second);
	}
}

const s_native_module &c_native_module_registry::get_native_module(h_native_module handle) {
	wl_assert(valid_index(handle.get_data(), get_native_module_count()));
	return g_native_module_registry_data.native_modules[handle.get_data()].native_module;
}

const char *c_native_module_registry::get_native_module_for_native_operator(e_native_operator native_operator) {
	wl_assert(valid_enum_index(native_operator));
	return g_native_module_registry_data.native_operators[enum_index(native_operator)].native_module_name.get_string();
}

uint32 c_native_module_registry::get_optimization_rule_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.optimization_rules.size());
}

const s_native_module_optimization_rule &c_native_module_registry::get_optimization_rule(uint32 index) {
	wl_assert(valid_index(index, get_optimization_rule_count()));
	return g_native_module_registry_data.optimization_rules[index];
}

bool c_native_module_registry::output_registered_native_modules(const char *filename) {
	std::ofstream out(filename);

	if (!out.is_open()) {
		return false;
	}

	struct s_output_item {
		enum class e_type {
			k_library,
			k_native_module,

			k_count
		};

		e_type type;
		s_native_module_uid uid; // Doubles as just library ID if this is a library

		bool operator<(const s_output_item &other) const {
			if (uid.get_library_id() != other.uid.get_library_id()) {
				return uid.get_library_id() < other.uid.get_library_id();
			} else {
				if (type == e_type::k_native_module && other.type == e_type::k_native_module) {
					return uid.get_module_id() < other.uid.get_module_id();
				} else {
					// The library always comes first
					return (other.type == e_type::k_native_module);
				}
			}
		}
	};

	std::vector<s_output_item> output_items;
	output_items.reserve(g_native_module_registry_data.native_module_libraries.size() +
		g_native_module_registry_data.native_modules.size());

	for (const s_native_module_library &library : g_native_module_registry_data.native_module_libraries) {
		s_output_item item;
		item.type = s_output_item::e_type::k_library;
		item.uid = s_native_module_uid::build(library.id, 0);
		output_items.push_back(item);
	}

	for (size_t index = 0; index < g_native_module_registry_data.native_modules.size(); index++) {
		s_output_item item;
		item.type = s_output_item::e_type::k_native_module;
		item.uid = g_native_module_registry_data.native_modules[index].native_module.uid;
		output_items.push_back(item);
	}

	std::sort(output_items.begin(), output_items.end());

	bool first_output = true;
	for (size_t index = 0; index < output_items.size(); index++) {
		const s_output_item &item = output_items[index];

		switch (item.type) {
		case s_output_item::e_type::k_library:
		{
			if (!first_output) {
				out << "\n";
			}

			const s_native_module_library &library =
				g_native_module_registry_data.native_module_libraries[item.uid.get_library_id()];
			out << library.name.get_string() << ":\n";
			break;
		}

		case s_output_item::e_type::k_native_module:
		{
			h_native_module native_module_handle = get_native_module_handle(item.uid);
			const s_native_module &native_module = get_native_module(native_module_handle);

			out << "\t";

			if (native_module.return_argument_index == k_invalid_native_module_argument_index) {
				// Return type of void is a special case
				out << k_native_module_no_return_type_string;
			} else {
				wl_assert(native_module.out_argument_count > 0);
				const s_native_module_argument &argument = native_module.arguments[native_module.return_argument_index];
				wl_assert(argument.argument_direction == e_native_module_argument_direction::k_out);
				out << argument.type.to_string();
			}

			out << " " << native_module.name.get_string() << "(";

			bool first_argument = true;
			for (size_t arg = 0; arg < native_module.argument_count; arg++) {
				if (arg == native_module.return_argument_index) {
					// Skip it - this argument is the return type
					continue;
				}

				const s_native_module_argument &argument = native_module.arguments[arg];

				if (!first_argument) {
					out << ", ";
				}

				out << k_native_module_argument_direction_strings[enum_index(argument.argument_direction)]
					<< " " << argument.type.to_string();

				if (!native_module.arguments[arg].name.is_empty()) {
					out << " " << native_module.arguments[arg].name.get_string();
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

#if IS_TRUE(ASSERTS_ENABLED)
static void validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule) {
	// $TODO $PLUGIN validation
}
#endif // IS_TRUE(ASSERTS_ENABLED)
