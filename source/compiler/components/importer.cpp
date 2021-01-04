#include "common/utility/file_utility.h"

#include "compiler/ast/nodes.h"
#include "compiler/components/importer.h"

#include "execution_graph/native_module_registry.h"

#include "generated/wavelang_grammar.h"

#include <vector>

static constexpr const char *k_wavelang_source_file_extension = ".wl";

static bool are_imports_identical(
	const s_compiler_source_file_import &import_a,
	const s_compiler_source_file_import &import_b);

static c_ast_node_module_declaration *create_module_declaration_for_native_module(
	const s_native_module &native_module,
	bool is_operator);

static c_ast_qualified_data_type ast_data_type_from_native_module_data_type(
	c_native_module_qualified_data_type native_module_data_type,
	bool is_runtime_only);

class c_import_resolver_visitor : public c_wavelang_lr_parse_tree_visitor {
public:
	c_import_resolver_visitor(
		c_compiler_context &context,
		h_compiler_source_file source_file_handle);

protected:
	void exit_import() override;
	void exit_normal_import_type(const s_token &location_token);
	void exit_native_import_type(const s_token &location_token);
	void exit_import_path_prefix_list() override;
	void exit_import_path_component_list(const s_token &component) override;
	void exit_import_as_local() override;
	void exit_import_as_component_list(const s_token &component) override;

	bool enter_instrument_global(const s_token &command) override { return false; }
	bool enter_global_scope(
		c_temporary_reference<c_ast_node_scope> &rule_head_context,
		c_ast_node_scope *&item_list) override {
		return false;
	}

private:
	c_compiler_context &m_context;
	h_compiler_source_file m_source_file_handle;

	s_token m_location_token;						// Used for error reporting
	bool m_native_import;							// Whether to force this import to be native
	size_t m_import_path_prefix_dot_count;			// Number of .s before the import components
	std::vector<s_token> m_import_path_components;	// Components of the import path
	bool m_import_as_local;							// Whether to import relative to the current source file
	std::vector<s_token> m_import_as_components;	// Components of the import-as section

	void try_resolve_import(const s_token &import_keyword);
};

static bool g_importer_initialized = false;

void c_importer::initialize() {
	wl_assert(!g_importer_initialized);
	g_importer_initialized = true;
}

void c_importer::deinitialize() {
	wl_assert(g_importer_initialized);
	g_importer_initialized = false;
}

void c_importer::resolve_imports(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	wl_assert(g_importer_initialized);
	c_import_resolver_visitor visitor(context, source_file_handle);
	visitor.visit();
}

void c_importer::add_imports_to_global_scope(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	s_compiler_source_file &source_file = context.get_source_file(source_file_handle);

	// First add all operators
	c_ast_node_scope *global_scope = source_file.ast->get_as<c_ast_node_scope>();
	for (h_native_module native_module_handle : c_native_module_registry::iterate_native_modules()) {
		const s_native_module &native_module =
			c_native_module_registry::get_native_module(native_module_handle);
		e_native_operator native_operator =
			c_native_module_registry::get_native_module_operator(native_module.uid);
		if (native_operator == e_native_operator::k_invalid) {
			continue;
		}

		c_ast_node_module_declaration *module_declaration =
			create_module_declaration_for_native_module(native_module, true);
		global_scope->add_imported_scope_item(module_declaration, true);
	}

	std::vector<c_ast_node_declaration *> lookup_buffer;
	for (const s_compiler_source_file_import &import : source_file.imports) {
		// First make sure the import-as scope exists
		c_ast_node_scope *current_scope = source_file.ast->get_as<c_ast_node_scope>();
		for (const std::string &import_as_component : import.import_as_components) {
			// Find a matching namespace or create one if it doesn't yet exist. Our lookup may produce multiple results
			// which is a conflict that will be reported later (through the use of c_tracked_scope).
			current_scope->lookup_declarations_by_name(import_as_component.c_str(), lookup_buffer);
			c_ast_node_namespace_declaration *child_namespace = nullptr;
			for (c_ast_node_declaration *declaration : lookup_buffer) {
				child_namespace = declaration->try_get_as<c_ast_node_namespace_declaration>();
				if (child_namespace) {
					break;
				}
			}

			if (!child_namespace) {
				child_namespace = new c_ast_node_namespace_declaration();
				child_namespace->set_name(import_as_component.c_str());
			}

			current_scope->add_imported_scope_item(child_namespace, true);
			current_scope = child_namespace->get_scope();
		}

		// Pull in all public declarations from the import
		if (import.source_file_handle.is_valid()) {
			// Grab each declaration in the imported source file. Don't take ownership of these because they're
			// already owned by the imported source file's AST.
			const s_compiler_source_file &imported_source_file = context.get_source_file(import.source_file_handle);
			const c_ast_node_scope *imported_scope = imported_source_file.ast->get_as<c_ast_node_scope>();
			for (size_t item_index = 0; item_index < imported_scope->get_scope_item_count(); item_index++) {
				c_ast_node_declaration *declaration =
					imported_scope->get_scope_item(item_index)->try_get_as<c_ast_node_declaration>();
				if (declaration && declaration->get_visibility() == e_ast_visibility::k_public) {
					current_scope->add_imported_scope_item(declaration, false);
				}
			}
		} else {
			wl_assert(import.native_module_library_handle.is_valid());

			// Construct declarations for all the native modules in this library and add them to the current scope.
			// Take ownership of these because we're constructing them on-the-fly here.
			const s_native_module_library &library =
				c_native_module_registry::get_native_module_library(import.native_module_library_handle);
			for (h_native_module native_module_handle : c_native_module_registry::iterate_native_modules()) {
				const s_native_module &native_module =
					c_native_module_registry::get_native_module(native_module_handle);
				if (native_module.uid.get_library_id() != library.id) {
					// Note: this is pretty inefficient, we could change the registry to keep a list of native
					// modules on a per-library basis
					continue;
				}

				c_ast_node_module_declaration *module_declaration =
					create_module_declaration_for_native_module(native_module, false);
				current_scope->add_imported_scope_item(module_declaration, true);
			}
		}
	}
}


static bool are_imports_identical(
	const s_compiler_source_file_import &import_a,
	const s_compiler_source_file_import &import_b) {
	if (import_a.source_file_handle != import_b.source_file_handle
		|| import_a.native_module_library_handle != import_b.native_module_library_handle
		|| import_a.import_as_components.size() != import_b.import_as_components.size()) {
		return false;
	}

	for (size_t index = 0; index < import_a.import_as_components.size(); index++) {
		if (import_a.import_as_components[index] != import_b.import_as_components[index]) {
			return false;
		}
	}

	return true;
}

c_import_resolver_visitor::c_import_resolver_visitor(
	c_compiler_context &context,
	h_compiler_source_file source_file_handle)
	: c_wavelang_lr_parse_tree_visitor(
		context.get_source_file(source_file_handle).parse_tree,
		context.get_source_file(source_file_handle).tokens)
	, m_context(context)
	, m_source_file_handle(source_file_handle) {
	m_native_import = false;
	m_import_path_prefix_dot_count = 0;
	bool m_import_as_local = false;
}

void c_import_resolver_visitor::exit_import() {
	try_resolve_import(m_location_token);
	m_native_import = false;
	m_import_path_prefix_dot_count = 0;
	m_import_path_components.clear();
	m_import_as_local = false;
	m_import_as_components.clear();
}

void c_import_resolver_visitor::exit_normal_import_type(const s_token &location_token) {
	m_location_token = location_token;
	m_native_import = false;
}

void c_import_resolver_visitor::exit_native_import_type(const s_token &location_token) {
	m_location_token = location_token;
	m_native_import = true;
}

void c_import_resolver_visitor::exit_import_path_prefix_list() {
	m_import_path_prefix_dot_count++;
}

void c_import_resolver_visitor::exit_import_path_component_list(const s_token &component) {
	m_import_path_components.push_back(component);
}

void c_import_resolver_visitor::exit_import_as_local() {
	m_import_as_local = true;
}

void c_import_resolver_visitor::exit_import_as_component_list(const s_token &component) {
	m_import_as_components.push_back(component);
}

void c_import_resolver_visitor::try_resolve_import(const s_token &import_keyword) {
	std::vector<std::string> import_directory_attempts;
	bool attempt_native_import = false;

	if (m_native_import) {
		// We will only attempt a native library import, not a source file import
	} else {
		if (m_import_path_prefix_dot_count > 0) {
			// This import is relative to the current file
			const s_compiler_source_file &current_source_file = m_context.get_source_file(m_source_file_handle);
			std::string import_directory = get_path_directory(current_source_file.path.c_str());
			for (size_t dot = 1; dot < m_import_path_prefix_dot_count; dot++) {
				import_directory.push_back(k_path_separator);
				import_directory.append("..");
			}

			import_directory_attempts.push_back(import_directory);
		} else {
			// This import could be relative to the top-level file
			const s_compiler_source_file &top_level_source_file =
				m_context.get_source_file(h_compiler_source_file::construct(0));
			std::string import_directory = get_path_directory(top_level_source_file.path.c_str());
			import_directory_attempts.push_back(import_directory);

			// If it's not found there, try looking in installed libraries
			// $TODO not yet implemented
			//import_directory_attempts.push_back(installed_libraries_directory);

			// Finally, check for native imports
			attempt_native_import = true;
		}
	}

	s_compiler_source_file_import new_import;
	new_import.source_file_handle = h_compiler_source_file::invalid();;
	new_import.native_module_library_handle = h_native_module_library::invalid();

	for (size_t dot = 0; dot < m_import_path_prefix_dot_count; dot++) {
		new_import.import_path_string.push_back('.');
	}

	for (size_t index = 0; index < m_import_path_components.size(); index++) {
		if (index > 0) {
			new_import.import_path_string.push_back('.');
		}

		new_import.import_path_string.append(m_import_path_components[index].token_string);
	}

	if (m_import_as_local) {
		new_import.import_as_string.push_back('.');
	} else {
		for (size_t index = 0; index < m_import_as_components.size(); index++) {
			if (index > 0) {
				new_import.import_as_string.push_back('.');
			}

			new_import.import_as_string.append(m_import_as_components[index].token_string);
		}
	}

	// Store off the import-as components. If our import-as is local (just ".") we'll add the import to the file-level
	// namespace so the import-as will have no components.
	if (!m_import_as_local) {
		if (m_import_as_components.empty()) {
			// No import-as components provided, use the (possibly relative) import path instead
			for (const s_token &token : m_import_path_components) {
				new_import.import_as_components.emplace_back(std::string(token.token_string));
			}
		} else {
			// Use the provided import-as components
			for (const s_token &token : m_import_as_components) {
				new_import.import_as_components.emplace_back(std::string(token.token_string));
			}
		}
	}

	bool success = false;

	s_compiler_source_file &source_file = m_context.get_source_file(m_source_file_handle);
	for (const std::string &import_directory : import_directory_attempts) {
		std::string import_path = import_directory;
		for (const s_token &component : m_import_path_components) {
			import_path.push_back(k_path_separator);
			import_path.append(component.token_string);
		}

		import_path.append(k_wavelang_source_file_extension);

		bool was_added;
		new_import.source_file_handle = m_context.get_or_add_source_file(import_path.c_str(), was_added);
		if (new_import.source_file_handle == m_source_file_handle) {
			m_context.error(
				e_compiler_error::k_self_referential_import,
				import_keyword.source_location,
				"Self-referential import encountered");
			return;
		}

		if (new_import.source_file_handle.is_valid()) {
			// Success! Add this import if it doesn't already exist
			success = true;
			break;
		}
	}

	if (!success && m_import_path_prefix_dot_count == 0 && m_import_path_components.size() == 1) {
		// Check if this is a native module library import
		for (h_native_module_library library_handle : c_native_module_registry::iterate_native_module_libraries()) {
			const s_native_module_library &library =
				c_native_module_registry::get_native_module_library(library_handle);
			if (library.name == m_import_path_components[0].token_string) {
				new_import.native_module_library_handle = library_handle;
				success = true;
				break;
			}
		}
	}

	if (!success) {
		m_context.error(
			e_compiler_error::k_failed_to_resolve_import,
			import_keyword.source_location,
			"Failed to resolve import '%s'",
			new_import.import_path_string.c_str());
		return;
	}

	// Only add the import if an identical one doesn't already exists. Otherwise, silently ignore it.
	bool add_import = true;
	for (const s_compiler_source_file_import &existing_import : source_file.imports) {
		if (are_imports_identical(new_import, existing_import)) {
			add_import = false;
			break;
		}
	}

	if (add_import) {
		source_file.imports.emplace_back(new_import);
	}
}

static c_ast_node_module_declaration *create_module_declaration_for_native_module(
	const s_native_module &native_module,
	bool is_operator) {
	c_ast_node_module_declaration *module_declaration = new c_ast_node_module_declaration();
	module_declaration->set_visibility(e_ast_visibility::k_public);

	if (is_operator) {
		e_native_operator native_operator = c_native_module_registry::get_native_module_operator(native_module.uid);
		wl_assert(native_operator != e_native_operator::k_invalid);
		module_declaration->set_name(get_native_operator_native_module_name(native_operator));
	} else {
		module_declaration->set_name(native_module.name.get_string());
	}

	module_declaration->set_native_module_uid(native_module.uid);

	bool is_runtime_only = !native_module.compile_time_call;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		if (argument_index == native_module.return_argument_index) {
			continue;
		}

		const s_native_module_argument &argument = native_module.arguments[argument_index];
		c_ast_node_module_declaration_argument *module_declaration_argument =
			new c_ast_node_module_declaration_argument();

		e_ast_argument_direction argument_direction = e_ast_argument_direction::k_invalid;
		switch (argument.argument_direction) {
		case e_native_module_argument_direction::k_in:
			argument_direction = e_ast_argument_direction::k_in;
			break;

		case e_native_module_argument_direction::k_out:
			argument_direction = e_ast_argument_direction::k_out;
			break;

		default:
			wl_unreachable();
		}

		module_declaration_argument->set_argument_direction(argument_direction);

		c_ast_node_value_declaration *value_declaration = new c_ast_node_value_declaration();
		value_declaration->set_name(argument.name.get_string());
		value_declaration->set_data_type(ast_data_type_from_native_module_data_type(argument.type, is_runtime_only));
		module_declaration_argument->set_value_declaration(value_declaration);

		module_declaration->add_argument(module_declaration_argument);
	}

	if (native_module.return_argument_index == k_invalid_native_module_argument_index) {
		module_declaration->set_return_type(
			c_ast_qualified_data_type(
				c_ast_data_type(e_ast_primitive_type::k_void),
				e_ast_data_mutability::k_variable));
	} else {
		c_native_module_qualified_data_type return_type =
			native_module.arguments[native_module.return_argument_index].type;
		module_declaration->set_return_type(ast_data_type_from_native_module_data_type(return_type, is_runtime_only));
	}

	return module_declaration;
}

static c_ast_qualified_data_type ast_data_type_from_native_module_data_type(
	c_native_module_qualified_data_type native_module_data_type,
	bool is_runtime_only) {
	e_ast_primitive_type ast_primitive_type = e_ast_primitive_type::k_invalid;
	switch (native_module_data_type.get_primitive_type()) {
	case e_native_module_primitive_type::k_real:
		ast_primitive_type = e_ast_primitive_type::k_real;
		break;

	case e_native_module_primitive_type::k_bool:
		ast_primitive_type = e_ast_primitive_type::k_bool;
		break;

	case e_native_module_primitive_type::k_string:
		ast_primitive_type = e_ast_primitive_type::k_string;
		break;

	default:
		wl_unreachable();
	}

	e_ast_data_mutability ast_data_mutability = e_ast_data_mutability::k_invalid;
	switch (native_module_data_type.get_data_mutability()) {
	case e_native_module_data_mutability::k_variable:
		ast_data_mutability = e_ast_data_mutability::k_variable;
		break;

	case e_native_module_data_mutability::k_constant:
		ast_data_mutability = e_ast_data_mutability::k_constant;
		break;

	case e_native_module_data_mutability::k_dependent_constant:
		ast_data_mutability = e_ast_data_mutability::k_dependent_constant;
		break;

	default:
		wl_unreachable();
	}

	return c_ast_qualified_data_type(
		c_ast_data_type(ast_primitive_type, native_module_data_type.is_array()),
		ast_data_mutability);
}
