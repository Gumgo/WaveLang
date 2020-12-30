#pragma once

#include "common/common.h"

#include "native_module/native_module_binding.h"

#include <unordered_map>

class c_native_module_registration_utilities {
public:
	static void validate_argument_names(const s_native_module &native_module);

	static void map_arguments(
		const s_native_module &source_native_module,
		const s_native_module &mapped_native_module,
		native_module_binding::s_argument_index_map &argument_index_map);

	static void get_name_from_identifier(
		const char *identifier,
		c_static_string<k_max_native_module_name_length> &name_out);

	static std::unordered_map<std::string_view, s_native_module_uid> build_native_module_identifier_map(
		uint32 library_id,
		uint32 library_version);
};

struct s_native_module_registration_entry;
struct s_native_module_optimization_rule_registration_entry;

struct s_native_module_library_registration_entry {
	s_native_module_library library{};
	s_native_module_registration_entry *native_modules = nullptr;
	s_native_module_optimization_rule_registration_entry *optimization_rules = nullptr;
	s_native_module_library_registration_entry *next = nullptr;

	static s_native_module_library_registration_entry *&registration_list();

	// The library that native modules are actively being registered to
	static s_native_module_library_registration_entry *&active_library();

	static void end_active_library_native_module_registration();

	template<s_native_module_library_registration_entry *k_entry>
	class c_builder {
	public:
		c_builder(uint32 id, const char *name, uint32 version) {
			wl_assert(!s_native_module_library_registration_entry::active_library());
			s_native_module_library_registration_entry::active_library() = k_entry;

			k_entry->next = s_native_module_library_registration_entry::registration_list();
			s_native_module_library_registration_entry::registration_list() = k_entry;

			k_entry->library.id = id;
			k_entry->library.name.set_verify(name);
			k_entry->library.version = version;
		}

		// Sets a function used to initialize the library for the compiler. This function should return a pointer to the
		// initialized library context.
		c_builder &set_compiler_initializer(f_library_compiler_initializer compiler_initializer) {
			k_entry->library.compiler_initializer = compiler_initializer;
			return *this;
		}

		// Sets a function used to deinitialize the library for the compiler. This function receives a pointer to the
		// initialized library context.
		c_builder &set_compiler_deinitializer(f_library_compiler_deinitializer compiler_deinitializer) {
			k_entry->library.compiler_deinitializer = compiler_deinitializer;
			return *this;
		}
	};
};

struct s_native_module_registration_entry {
	s_native_module native_module{};
	bool arguments_initialized = false;

	// A native module identifier is used to distinguish between overloaded native modules. For example, if there are
	// two overloaded modules named "my_module", the identifiers "my_module$real" and "my_module$bool" could be used.
	const char *identifier; // Points to a static allocation

	s_native_module_registration_entry *next = nullptr;

	// $TODO $LATENCY add native_module_binding::s_argument_index_map k_get_latency_argument_index_map template param -
	// see task_function_registration.h for details
	template<s_native_module_registration_entry *k_entry>
	class c_builder {
	public:
		c_builder(uint32 id, const char *identifier) {
			// Grab the current library
			s_native_module_library_registration_entry *active_library =
				s_native_module_library_registration_entry::active_library();
			wl_assertf(active_library, "No native module library is active for registration");

			k_entry->next = active_library->native_modules;
			active_library->native_modules = k_entry;

			k_entry->native_module.uid = s_native_module_uid::build(active_library->library.id, id);
			c_native_module_registration_utilities::get_name_from_identifier(identifier, k_entry->native_module.name);
			k_entry->identifier = identifier;
		}

		// Associates an operator with the native module
		c_builder &set_native_operator(e_native_operator native_operator) {
			wl_assert(native_operator != e_native_operator::k_invalid);
			k_entry->native_module.name.set_verify(get_native_operator_native_module_name(native_operator));
			return *this;
		}

		// Sets the function to be called at compile-time
		template<auto k_function>
		c_builder &set_compile_time_call() {
			k_entry->native_module.compile_time_call =
				native_module_binding::native_module_call_wrapper<k_function, void>;
			native_module_binding::populate_native_module_arguments<decltype(k_function), void>(k_entry->native_module);
			c_native_module_registration_utilities::validate_argument_names(k_entry->native_module);
			k_entry->arguments_initialized = true;
			return *this;
		}

		// Sets the call signature of the native module
		template<typename t_function>
		c_builder &set_call_signature() {
			native_module_binding::populate_native_module_arguments<t_function, void>(k_entry->native_module);
			return *this;
		}

		// $TODO $LATENCY
		// Sets the function to be called at compile-time to query the latency of this native module
		//template<auto k_function>
		//c_builder &set_get_latency() {
		//	wl_assert(k_entry->arguments_initialized);
		//	k_entry->get_latency =
		//		native_module_binding::native_module_call_wrapper<
		//			k_function,
		//			uint32,
		//			k_get_latency_argument_index_map>;
		//
		//	s_native_module native_module;
		//	native_module_binding::populate_native_module_arguments<decltype(k_function), uint32>(native_module);
		//	c_native_module_registration_utilities::validate_argument_names(native_module);
		//	c_native_module_registration_utilities::map_arguments(
		//		k_entry->native_module,
		//		native_module,
		//		*k_get_latency_argument_index_map);
		//
		//	return *this;
		//}
	};
};

struct s_native_module_optimization_rule_registration_entry {
	const char *optimization_rule_string = nullptr;
	s_native_module_optimization_rule_registration_entry *next = nullptr;

	template<s_native_module_optimization_rule_registration_entry *k_entry>
	class c_builder {
	public:
		c_builder(const char *optimization_rule_string) {
			// Grab the current library
			s_native_module_library_registration_entry *active_library =
				s_native_module_library_registration_entry::active_library();
			wl_assertf(active_library, "No native module library is active for registration");

			k_entry->next = active_library->optimization_rules;
			active_library->optimization_rules = k_entry;

			k_entry->optimization_rule_string = optimization_rule_string;
		}
	};
};

#define NM_REG_ENTRY TOKEN_CONCATENATE(s_registration_entry_, __LINE__)
#define NM_REG_ENTRY_BUILDER TOKEN_CONCATENATE(s_registration_entry_builder_, __LINE__)

// Registration API:

// Declares a library and marks it as the active registration library
#define wl_native_module_library(id, name, version)														\
	static s_native_module_library_registration_entry NM_REG_ENTRY;										\
	static s_native_module_library_registration_entry::c_builder<&NM_REG_ENTRY> NM_REG_ENTRY_BUILDER =	\
		s_native_module_library_registration_entry::c_builder<&NM_REG_ENTRY>(id, name, version)

// Ends active registration for the currently active library
#define wl_end_active_library_native_module_registration()												\
	static int32 NM_REG_ENTRY = []() {																	\
		s_native_module_library_registration_entry::end_active_library_native_module_registration();	\
		return 0;																						\
	}()

// Declares a native module in the active registration library
// "identifier" is the script-visible name optionally followed by the character $ and an additional string used to
// uniquely identify this native module from overloads. The identifier is used in optimization rule strings.
#define wl_native_module(id, identifier)														\
	static s_native_module_registration_entry NM_REG_ENTRY;										\
	static s_native_module_registration_entry::c_builder<&NM_REG_ENTRY> NM_REG_ENTRY_BUILDER =	\
		s_native_module_registration_entry::c_builder<&NM_REG_ENTRY>(id, identifier)

// Declares an optimization rule in the active registration library
// Defines an optimization rule using the following syntax:
// x -> y
// meaning that if the expression "x" is identified, it will be replaced with "y" during optimization. Expressions can
// be built using the following pieces:
// - module_name$overload_identifier(x, y, z, ...) - Represents a native module call, where x, y, z, ... are the
//   arguments. Example: addition$real(x, y)
// - identifier - Used to represent a variable to be matched between the pre- and post-optimization expression.
// - const identifier - Same as above, but the variable is only matched if it is compile-time constant.
// - <real value> - matches with a real value.
// - true, false - matches with a boolean value.
#define wl_optimization_rule(optimization_rule)																		\
	static s_native_module_optimization_rule_registration_entry NM_REG_ENTRY;										\
	static s_native_module_optimization_rule_registration_entry::c_builder<&NM_REG_ENTRY> NM_REG_ENTRY_BUILDER =	\
		s_native_module_optimization_rule_registration_entry::c_builder<&NM_REG_ENTRY>(#optimization_rule)

bool register_native_modules();
