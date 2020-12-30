#pragma once

#include "common/common.h"

#include "task_function/task_function_binding.h"

struct s_task_function_registration_entry;

struct s_task_function_library_registration_entry {
	s_task_function_library library{};
	s_task_function_registration_entry *task_functions = nullptr;
	s_task_function_library_registration_entry *next = nullptr;

	static s_task_function_library_registration_entry *&registration_list();

	// The library that task functions are actively being registered to
	static s_task_function_library_registration_entry *&active_library();

	static void end_active_library_task_function_registration();

	template<s_task_function_library_registration_entry *k_entry>
	class c_builder {
	public:
		c_builder(uint32 id, const char *name, uint32 version) {
			wl_assert(!s_task_function_library_registration_entry::active_library());
			s_task_function_library_registration_entry::active_library() = k_entry;

			k_entry->next = s_task_function_library_registration_entry::registration_list();
			s_task_function_library_registration_entry::registration_list() = k_entry;

			k_entry->library.id = id;
			k_entry->library.name.set_verify(name);
			k_entry->library.version = version;
		}

		// Sets a function used to initialize the library for the engine. This function should return a pointer to
		// initialized library context.
		c_builder &set_engine_initializer(f_library_engine_initializer engine_initializer) {
			k_entry->library.engine_initializer = engine_initializer;
			return *this;
		}

		// Setsa function used to deinitialize the library for the engine. This function receives a pointer to the
		// initialized library context.
		c_builder &set_engine_deinitializer(f_library_engine_deinitializer engine_deinitializer) {
			k_entry->library.engine_deinitializer = engine_deinitializer;
			return *this;
		}

		// Sets a function which is called directly before tasks are initialized. This function receives a pointer to
		// the initialized library context.
		c_builder &set_tasks_pre_initializer(f_library_tasks_pre_initializer tasks_pre_initializer) {
			k_entry->library.tasks_pre_initializer = tasks_pre_initializer;
			return *this;
		}

		// Sets a function which is called directly after tasks are initialized. This function receives a pointer to the
		// initialized library context.
		c_builder &set_tasks_post_initializer(f_library_tasks_post_initializer tasks_post_initializer) {
			k_entry->library.tasks_post_initializer = tasks_post_initializer;
			return *this;
		}
	};
};

struct s_task_function_registration_entry {
	s_task_function task_function{};
	bool arguments_initialized = false;

	// Identifier of a native module in the same library
	const char *native_module_identifier; // Points to a static allocation

	task_function_binding::s_task_function_argument_names argument_names;
	task_function_binding::s_argument_index_map memory_query_argument_indices;
	task_function_binding::s_argument_index_map initializer_argument_indices;
	task_function_binding::s_argument_index_map voice_initializer_argument_indices;

	s_task_function_registration_entry *next = nullptr;

	template<s_task_function_registration_entry *k_entry>
	class c_builder {
	public:
		c_builder(uint32 id, const char *identifier) {
			// Grab the current library
			s_task_function_library_registration_entry *active_library =
				s_task_function_library_registration_entry::active_library();
			wl_assertf(active_library, "No task function library is active for registration");

			k_entry->next = active_library->task_functions;
			active_library->task_functions = k_entry;

			k_entry->task_function.uid = s_task_function_uid::build(active_library->library.id, id);
			k_entry->identifier.set_verify(identifier);
		}

		// Sets the function to be called at runtime
		template<auto k_function>
		c_builder &set_function() {
			k_entry->task_function.function =
				task_function_binding::task_function_call_wrapper<k_function>;
			task_function_binding::populate_task_function_arguments<decltype(k_function)>(
				k_entry->task_function,
				k_entry->argument_names);
			c_task_function_registration_utilities::validate_argument_names(k_entry->argument_names);
			k_entry->arguments_initialized = true;
			return *this;
		}

		// Sets the function to query the memory requirements of this task function
		template<auto k_function>
		c_builder &set_memory_query() {
			wl_assert(k_entry->arguments_initialized);
			k_entry->task_function.memory_query =
				task_function_binding::task_function_call_wrapper<k_function, &k_entry->memory_query_argument_indices>;

			s_task_function task_function;
			task_function_binding::s_task_function_argument_names argument_names;
			task_function_binding::populate_task_function_arguments<decltype(k_function)>(
				task_function,
				argument_names);
			c_task_function_registration_utilities::validate_argument_names(argument_names);
			c_task_function_registration_utilities::map_arguments(
				k_entry->task_function,
				k_entry->argument_names,
				task_function, argument_names,
				k_entry->memory_query_argument_indices);

			return *this;
		}

		// Sets the function to initialize this task function
		template<auto k_function>
		c_builder &set_initializer() {
			wl_assert(k_entry->arguments_initialized);
			k_entry->task_function.initializer =
				task_function_binding::task_function_call_wrapper<k_function, &k_entry->initializer_argument_indices>;

			s_task_function task_function;
			task_function_binding::s_task_function_argument_names argument_names;
			task_function_binding::populate_task_function_arguments<decltype(k_function)>(
				task_function,
				argument_names);
			c_task_function_registration_utilities::validate_argument_names(argument_names);
			c_task_function_registration_utilities::map_arguments(
				k_entry->task_function,
				k_entry->argument_names,
				task_function, argument_names,
				k_entry->initializer_argument_indices);

			return *this;
		}

		// Sets the function to initialize an individual voice for this task function
		template<auto k_function>
		c_builder &set_voice_initializer() {
			wl_assert(k_entry->arguments_initialized);
			k_entry->task_function.voice_initializer = task_function_binding::task_function_call_wrapper<
				k_function,
				&k_entry->voice_initializer_argument_indices>;

			s_task_function task_function;
			task_function_binding::s_task_function_argument_names argument_names;
			task_function_binding::populate_task_function_arguments<decltype(k_function)>(
				task_function,
				argument_names);
			c_task_function_registration_utilities::validate_argument_names(argument_names);
			c_task_function_registration_utilities::map_arguments(
				k_entry->task_function,
				k_entry->argument_names,
				task_function, argument_names,
				k_entry->voice_initializer_argument_indices);

			return *this;
		}
	};
};

class c_task_function_registration_utilities {
public:
	static void validate_argument_names(
		size_t argument_count,
		const task_function_binding::s_task_function_argument_names &argument_names);

	static void map_arguments(
		const s_task_function &source_task_function,
		const task_function_binding::s_task_function_argument_names &source_argument_names,
		const s_task_function &mapped_task_function,
		const task_function_binding::s_task_function_argument_names &mapped_argument_names,
		task_function_binding::s_argument_index_map &argument_index_map);
};

#define TF_REG_ENTRY TOKEN_CONCATENATE(s_registration_entry_, __LINE__)
#define TF_REG_ENTRY_BUILDER TOKEN_CONCATENATE(s_registration_entry_builder_, __LINE__)

// Registration API:

// Declares a library and marks it as the active registration library
#define wl_task_function_library(id, name, version)														\
	static s_task_function_library_registration_entry TF_REG_ENTRY;										\
	static s_task_function_library_registration_entry::c_builder<&TF_REG_ENTRY> TF_REG_ENTRY_BUILDER =	\
		s_task_function_library_registration_entry::c_builder<&TF_REG_ENTRY>(id, name, version)

// Ends active registration for the currently active library
#define wl_end_active_library_task_function_registration()												\
	static int32 TF_REG_ENTRY = []() {																	\
		s_task_function_library_registration_entry::end_active_library_task_function_registration();	\
		return 0;																						\
	}()

// Declares a task function in the active registration library
// "identifier" is the identifier of a native module in the same library which should map to this task function
#define wl_task_function(id, identifier)														\
	static s_task_function_registration_entry TF_REG_ENTRY;										\
	static s_task_function_registration_entry::c_builder<&TF_REG_ENTRY> TF_REG_ENTRY_BUILDER =	\
		s_task_function_registration_entry::c_builder<&TF_REG_ENTRY>(id, identifier)

bool register_task_functions();
