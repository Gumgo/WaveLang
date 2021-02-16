#include "common/utility/reporting.h"

#include "instrument/native_module_registry.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <vector>

// Strings for native module registry output
static constexpr const char *k_native_module_no_return_type_string = "void";

static constexpr const char *k_native_module_argument_direction_strings[] = {
	"in",	// e_native_module_argument_direction::k_in
	"out"	// e_native_module_argument_direction::k_out
};
STATIC_ASSERT(is_enum_fully_mapped<e_native_module_argument_direction>(k_native_module_argument_direction_strings));

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
	s_static_array<s_native_module_uid, enum_count<e_native_module_intrinsic>()> native_module_intrinsics;
	std::vector<s_native_module_optimization_rule> optimization_rules;
};

static e_native_module_registry_state g_native_module_registry_state = e_native_module_registry_state::k_uninitialized;
static s_native_module_registry_data g_native_module_registry_data;

static bool validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule, const char *name);

void c_native_module_registry::initialize() {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_uninitialized);
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

	for (s_native_module_uid &uid : g_native_module_registry_data.native_module_intrinsics)
	{
		uid = s_native_module_uid::invalid();
	}

	g_native_module_registry_data.optimization_rules.clear();
	g_native_module_registry_data.optimization_rules.shrink_to_fit();

	g_native_module_registry_state = e_native_module_registry_state::k_uninitialized;
}

void c_native_module_registry::begin_registration() {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_initialized);
	g_native_module_registry_state = e_native_module_registry_state::k_registering;
}

bool c_native_module_registry::end_registration() {
	for (e_native_module_intrinsic intrinsic : iterate_enum<e_native_module_intrinsic>()) {
		if (!g_native_module_registry_data.native_module_intrinsics[enum_index(intrinsic)].is_valid()) {
			report_error("Native module intrinsic %d has not been registered", intrinsic);
			return false;
		}
	}

	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);
	g_native_module_registry_state = e_native_module_registry_state::k_finalized;
	return true;
}

bool c_native_module_registry::register_native_module_library(const s_native_module_library &library) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	// Make sure there isn't already a library registered with this ID
	auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(library.id);
	if (it != g_native_module_registry_data.native_module_library_ids_to_indices.end()) {
		report_error(
			"Failed to register conflicting native module library 0x%04x ('%s')",
			library.id,
			library.name.get_string());
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
	if (!validate_native_module(native_module)) {
		report_error(
			"Failed to register native module 0x%04x ('%s') because it contains errors",
			native_module.uid.get_module_id(),
			native_module.name.get_string());
		return false;
	}

	s_native_module_internal native_module_internal;
	native_module_internal.native_module = native_module;

	// Check that the library referenced by the UID is registered
	{
		auto it = g_native_module_registry_data.native_module_library_ids_to_indices.find(
			native_module.uid.get_library_id());
		if (it == g_native_module_registry_data.native_module_library_ids_to_indices.end()) {
			report_error(
				"Native module library 0x%04x referenced by native module 0x%04x ('%s') was not registered",
				native_module.uid.get_library_id(),
				native_module.uid.get_module_id(),
				native_module.name.get_string());
			return false;
		}
	}

	// Check that the UID isn't already in use
	if (g_native_module_registry_data.native_module_uids_to_indices.find(native_module.uid) !=
		g_native_module_registry_data.native_module_uids_to_indices.end()) {
		report_error(
			"Failed to register conflicting native module 0x%04x ('%s')",
			native_module.uid.get_module_id(),
			native_module.name.get_string());
		return false;
	}

	// Determine whether this native module is a native operator
	native_module_internal.native_operator = e_native_operator::k_invalid;
	for (e_native_operator native_operator : iterate_enum<e_native_operator>()) {
		if (native_module.name == get_native_operator_native_module_name(native_operator)) {
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

bool c_native_module_registry::register_native_module_intrinsic(
	e_native_module_intrinsic intrinsic,
	const s_native_module_uid &native_module_uid) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);

	auto native_module_iter = g_native_module_registry_data.native_module_uids_to_indices.find(native_module_uid);
	if (native_module_iter == g_native_module_registry_data.native_module_uids_to_indices.end()) {
		report_error(
			"Failed to register native module 0x%04x as intrinsic %d "
			"because the native module was not registered",
			native_module_uid.get_module_id(),
			intrinsic);
		return false;
	}

	if (g_native_module_registry_data.native_module_intrinsics[enum_index(intrinsic)].is_valid()) {
		const s_native_module &native_module =
			g_native_module_registry_data.native_modules[native_module_iter->second].native_module;
		auto existing_iter = g_native_module_registry_data.native_module_uids_to_indices.find(
			g_native_module_registry_data.native_module_intrinsics[enum_index(intrinsic)]);
		const s_native_module &existing_native_module =
			g_native_module_registry_data.native_modules[existing_iter->second].native_module;
		report_error(
			"Failed to register native module 0x%04x ('%s') as intrinsic %d "
			"because native module 0x%04x ('%s') was already registered as that intrinsic",
			native_module_uid.get_module_id(),
			native_module.name.get_string(),
			intrinsic,
			existing_native_module.uid.get_module_id(),
			existing_native_module.name.get_string());
		return false;
	}

	g_native_module_registry_data.native_module_intrinsics[enum_index(intrinsic)] = native_module_uid;
	return true;
}

s_native_module_uid c_native_module_registry::get_native_module_intrinsic(e_native_module_intrinsic intrinsic) {
	return g_native_module_registry_data.native_module_intrinsics[enum_index(intrinsic)];
}

e_native_operator c_native_module_registry::get_native_module_operator(s_native_module_uid native_module_uid) {
	h_native_module native_module_handle = get_native_module_handle(native_module_uid);
	wl_assert(native_module_handle.is_valid());
	return g_native_module_registry_data.native_modules[native_module_handle.get_data()].native_operator;
}

bool c_native_module_registry::register_optimization_rule(
	const s_native_module_optimization_rule &optimization_rule,
	const char *name) {
	wl_assert(g_native_module_registry_state == e_native_module_registry_state::k_registering);
	if (!validate_optimization_rule(optimization_rule, name)) {
		report_error("Failed to register optimization rule '%s' because it contains errors", name);
		return false;
	}

	g_native_module_registry_data.optimization_rules.push_back(optimization_rule);
	return true;
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

uint32 c_native_module_registry::get_optimization_rule_count() {
	return cast_integer_verify<uint32>(g_native_module_registry_data.optimization_rules.size());
}

c_index_handle_iterator<h_native_module_optimization_rule> c_native_module_registry::iterate_optimization_rules() {
	return c_index_handle_iterator<h_native_module_optimization_rule>(get_optimization_rule_count());
}

const s_native_module_optimization_rule &c_native_module_registry::get_optimization_rule(
	h_native_module_optimization_rule handle) {
	wl_assert(valid_index(handle.get_data(), get_optimization_rule_count()));
	return g_native_module_registry_data.optimization_rules[handle.get_data()];
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

static bool validate_optimization_rule(const s_native_module_optimization_rule &optimization_rule, const char *name) {
	// $TODO Can we come up with some better way of identifying optimization rules in errors? The string is gone by this
	// point. We could make a to_string() function.

	class c_validator {
	public:
		c_validator(const s_native_module_optimization_rule &rule, const char *name)
			: m_rule(rule)
			, m_name(name) {}

		bool validate() {
			if (!m_rule.source.symbols[0].is_valid()) {
				report_error("Optimization rule '%s' source is empty", m_name);
				return false;
			}

			if (!m_rule.target.symbols[0].is_valid()) {
				report_error("Optimization rule '%s' target is empty", m_name);
				return false;
			}

			if (m_rule.source.symbols[0].type != e_native_module_optimization_symbol_type::k_native_module) {
				report_error("Optimization rule '%s' source does not begin with a native module", m_name);
				return false;
			}

			m_validating_source = true;
			if (!validate_pattern(m_rule.source)) {
				return false;
			}

			m_validating_source = false;
			if (!validate_pattern(m_rule.target)) {
				return false;
			}

			return true;
		}

	private:
		struct s_native_module_call {
			const s_native_module &native_module;
			uint32 next_argument_index;
			e_native_module_data_mutability dependent_constant_data_mutability;
		};

		static void skip_return_argument(s_native_module_call &call) {
			if (call.next_argument_index == call.native_module.return_argument_index) {
				call.next_argument_index++;
			}
		}

		c_native_module_qualified_data_type get_next_argument_data_type() const {
			const s_native_module_call &call = m_native_module_call_stack.back();
			if (call.next_argument_index >= call.native_module.argument_count) {
				report_error(
					"Optimization rule '%s' references native module 0x%04x ('%s') using too many arguments",
					m_name,
					call.native_module.uid.get_module_id(),
					call.native_module.name.get_string());
				return c_native_module_qualified_data_type::invalid();
			}

			if (call.native_module.arguments[call.next_argument_index].argument_direction !=
				e_native_module_argument_direction::k_in) {
				report_error(
					"Optimization rule '%s' references native module 0x%04x ('%s') "
					"which contains non-return out arguments",
					m_name,
					call.native_module.uid.get_module_id(),
					call.native_module.name.get_string());
				return c_native_module_qualified_data_type::invalid();
			}

			return call.native_module.arguments[call.next_argument_index].type;
		}

		bool assign_return_value(c_native_module_qualified_data_type data_type) {
			wl_assert(data_type.is_valid());
			wl_assert(data_type.get_data_mutability() != e_native_module_data_mutability::k_dependent_constant);

			if (m_native_module_call_stack.empty()) {
				if (m_validating_source) {
					m_source_data_type = data_type;
				} else {
					m_target_data_type = data_type;

					// If we're replacing the source with the target, we need to be able to assign the target value to
					// a value holding a source value
					if (!is_native_module_data_type_assignable(m_target_data_type, m_source_data_type)) {
						report_error(
							"Optimization rule '%s' cannot replace source data type '%s' with target data type '%s",
							m_name,
							m_source_data_type.to_string().c_str(),
							m_target_data_type.to_string().c_str());
						return false;
					}
				}
			} else {
				c_native_module_qualified_data_type argument_data_type = get_next_argument_data_type();
				if (!argument_data_type.is_valid()) {
					return false;
				}

				s_native_module_call &call = m_native_module_call_stack.back();
				if (argument_data_type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant) {
					// Downgrade the dependent-constant data mutability if we're provided with a variable data type
					call.dependent_constant_data_mutability =
						std::min(call.dependent_constant_data_mutability, data_type.get_data_mutability());

					// Change to variable so that is_native_module_data_type_assignable() doesn't fail
					argument_data_type =
						argument_data_type.change_data_mutability(e_native_module_data_mutability::k_variable);
				}

				if (!is_native_module_data_type_assignable(data_type, argument_data_type)) {
					const s_native_module_call &call = m_native_module_call_stack.back();
					report_error(
						"Optimization rule '%s' attempts to assign value of type '%s' "
						"to native module 0x%04x ('%s') argument '%s' of type '%s'",
						m_name,
						data_type.to_string().c_str(),
						call.native_module.uid.get_module_id(),
						call.native_module.name.get_string(),
						call.native_module.arguments[call.next_argument_index].name.get_string(),
						argument_data_type.to_string().c_str());
					return false;
				}

				call.next_argument_index++;
				skip_return_argument(call);
			}

			return true;
		}

		bool validate_symbol(const s_native_module_optimization_symbol &symbol) {
			switch (symbol.type) {
			case e_native_module_optimization_symbol_type::k_native_module:
			{
				h_native_module native_module_handle =
					c_native_module_registry::get_native_module_handle(symbol.data.native_module_uid);
				if (!native_module_handle.is_valid()) {
					report_error(
						"Optimization rule '%s' references invalid native module 0x%04x (library 0x%04x)",
						m_name,
						symbol.data.native_module_uid.get_module_id(),
						symbol.data.native_module_uid.get_library_id());
					return false;
				}

				const s_native_module &native_module =
					c_native_module_registry::get_native_module(native_module_handle);
				if (native_module.return_argument_index == k_invalid_native_module_argument_index) {
					report_error(
						"Optimization rule '%s' uses native module 0x%04x ('%s') which has no return value",
						m_name,
						native_module.uid.get_module_id(),
						native_module.name.get_string());
					return false;
				}

				m_native_module_call_stack.push_back({ native_module, 0, e_native_module_data_mutability::k_constant });
				skip_return_argument(m_native_module_call_stack.back());
				break;
			}

			case e_native_module_optimization_symbol_type::k_native_module_end:
			{
				if (m_native_module_call_stack.empty()) {
					report_error("Optimization rule '%s' contains mismatched native module end symbol", m_name);
					return false;
				}

				const s_native_module_call &call = m_native_module_call_stack.back();
				if (call.next_argument_index != call.native_module.argument_count) {
					report_error(
						"Optimization rule '%s', ended native module 0x%04x ('%s') before all arguments were used",
						m_name,
						call.native_module.uid.get_module_id(),
						call.native_module.name.get_string());
					return false;
				}

				c_native_module_qualified_data_type data_type =
					call.native_module.arguments[call.native_module.return_argument_index].type;
				if (data_type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant) {
					data_type = data_type.change_data_mutability(call.dependent_constant_data_mutability);
				}

				m_native_module_call_stack.pop_back();

				if (!assign_return_value(data_type)) {
					return false;
				}

				break;
			}

			case e_native_module_optimization_symbol_type::k_variable:
			{
				if (!m_validating_source) {
					report_error("Optimization rule '%s' contains 'variable' matches in the target", m_name);
					return false;
				}

				c_native_module_qualified_data_type data_type = get_next_argument_data_type();
				if (!data_type.is_valid()) {
					return false;
				}

				if (data_type.get_data_mutability() == e_native_module_data_mutability::k_constant) {
					report_error("Optimization rule '%s' attempts to match a variable to a constant argument", m_name);
					return false;
				}

				data_type = data_type.change_data_mutability(e_native_module_data_mutability::k_variable);
				m_matched_value_data_types.push_back(data_type);

				IF_ASSERTS_ENABLED(bool result = ) assign_return_value(data_type);
				wl_assert(result);
				break;
			}

			case e_native_module_optimization_symbol_type::k_constant:
			{
				if (!m_validating_source) {
					report_error("Optimization rule '%s' contains 'constant' matches in the target", m_name);
					return false;
				}

				c_native_module_qualified_data_type data_type = get_next_argument_data_type();
				if (!data_type.is_valid()) {
					return false;
				}

				data_type = data_type.change_data_mutability(e_native_module_data_mutability::k_constant);
				m_matched_value_data_types.push_back(data_type);

				IF_ASSERTS_ENABLED(bool result = ) assign_return_value(data_type);
				wl_assert(result);
				break;
			}

			case e_native_module_optimization_symbol_type::k_variable_or_constant:
			{
				if (!m_validating_source) {
					report_error(
						"Optimization rule '%s' contains 'variable or constant' matches in the target",
						m_name);
					return false;
				}

				c_native_module_qualified_data_type data_type = get_next_argument_data_type();
				if (!data_type.is_valid()) {
					return false;
				}

				if (data_type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant) {
					data_type = data_type.change_data_mutability(e_native_module_data_mutability::k_variable);
				}

				m_matched_value_data_types.push_back(data_type);

				IF_ASSERTS_ENABLED(bool result = ) assign_return_value(data_type);
				wl_assert(result);
				break;
			}

			case e_native_module_optimization_symbol_type::k_back_reference:
			{
				if (symbol.data.index >= m_matched_value_data_types.size()) {
					report_error(
						"Optimization rule '%s' with %u references attempted to use a back-reference of index %u",
						m_name,
						cast_integer_verify<uint32>(m_matched_value_data_types.size()),
						symbol.data.index);
					return false;
				}

				c_native_module_qualified_data_type data_type = m_matched_value_data_types[symbol.data.index];
				if (!assign_return_value(data_type)) {
					return false;
				}

				break;
			}

			case e_native_module_optimization_symbol_type::k_real_value:
			{
				c_native_module_qualified_data_type data_type(
					c_native_module_data_type(e_native_module_primitive_type::k_real, false, 1),
					e_native_module_data_mutability::k_constant);
				if (!assign_return_value(data_type)) {
					return false;
				}

				break;
			}

			case e_native_module_optimization_symbol_type::k_bool_value:
			{
				c_native_module_qualified_data_type data_type(
					c_native_module_data_type(e_native_module_primitive_type::k_bool, false, 1),
					e_native_module_data_mutability::k_constant);
				if (!assign_return_value(data_type)) {
					return false;
				}

				break;
			}

			default:
				report_error("Optimization rule '%s' contains invalid symbol type %d", m_name, symbol.type);
				return false;
			}

			return true;
		}

		bool validate_pattern(const s_native_module_optimization_pattern &pattern) {
			wl_assert(m_native_module_call_stack.empty());
			for (const s_native_module_optimization_symbol &symbol : pattern.symbols) {
				if (!symbol.is_valid()) {
					break;
				}

				if (m_validating_source && m_source_data_type.is_valid()
					|| (!m_validating_source && m_target_data_type.is_valid())) {
					report_error(
						"Optimization rule '%s' %s contains extra symbols",
						m_name,
						m_validating_source ? "source" : "target");
					return false;
				}

				if (!validate_symbol(symbol)) {
					return false;
				}
			}

			if (!m_native_module_call_stack.empty()) {
				report_error(
					"Optimization rule '%s' %s terminated without ending all native module calls",
					m_name,
					m_validating_source ? "source" : "target");
				return false;
			}

			return true;
		}

		const s_native_module_optimization_rule &m_rule;
		const char *m_name;

		bool m_validating_source = false;
		c_native_module_qualified_data_type m_source_data_type = c_native_module_qualified_data_type::invalid();
		c_native_module_qualified_data_type m_target_data_type = c_native_module_qualified_data_type::invalid();
		std::vector<s_native_module_call> m_native_module_call_stack;
		std::vector<c_native_module_qualified_data_type> m_matched_value_data_types;
	};

	c_validator validator(optimization_rule, name);
	if (!validator.validate()) {
		return false;
	}

	for (h_native_module_optimization_rule rule_handle : c_native_module_registry::iterate_optimization_rules()) {
		const s_native_module_optimization_rule &existing_rule =
			c_native_module_registry::get_optimization_rule(rule_handle);

		bool identical = true;
		for (uint32 index = 0; identical && index < k_max_native_module_optimization_pattern_length; index++) {
			if (!optimization_rule.source.symbols[index].is_valid()
				&& !existing_rule.source.symbols[index].is_valid()) {
				break;
			}

			if (optimization_rule.source.symbols[index] != existing_rule.source.symbols[index]) {
				identical = false;
			}
		}

		for (uint32 index = 0; identical && index < k_max_native_module_optimization_pattern_length; index++) {
			if (!optimization_rule.target.symbols[index].is_valid()
				&& !existing_rule.target.symbols[index].is_valid()) {
				break;
			}

			if (optimization_rule.target.symbols[index] != existing_rule.target.symbols[index]) {
				identical = false;
			}
		}

		if (identical) {
			report_error("Optimization rule '%s' is identical to a previously registered optimization rule", name);
			return false;
		}
	}

	return true;
}
