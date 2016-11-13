#include "common/scraper_attributes.h"

#include "scraper/annotation_collection.h"
#include "scraper/ast_visitor.h"
#include "scraper/diagnostic.h"
#include "scraper/scraper_result.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecursiveASTVisitor.h> // Can we use DeclVisitor?
#pragma warning(pop)

static const size_t k_invalid_library_index = static_cast<size_t>(-1);

class c_ast_visitor : public clang::ASTConsumer, public clang::RecursiveASTVisitor<c_ast_visitor> {
public:
	c_ast_visitor(clang::CompilerInstance &compiler_instance, c_scraper_result *result);

	void HandleTranslationUnit(clang::ASTContext &context);

	// See DeclNodes.td for a list of possible nodes we can visit
	// Or see http://clang.llvm.org/doxygen/classclang_1_1ASTDeclReader.html for a list of Visit functions
	// (http://clang.llvm.org/doxygen/classclang_1_1DeclVisitor.html appears not to be parsed correctly by Doxygen)

	// Library parsing
	bool VisitNamespaceDecl(clang::NamespaceDecl *decl);

	// Scrape native modules and task functions
	bool VisitFunctionDecl(clang::FunctionDecl *decl);

	// Optimization rule parsing
	bool VisitRecordDecl(clang::RecordDecl *decl);

private:
	struct s_task_argument_type {
		c_task_qualified_data_type data_type;
		bool is_constant;
		bool is_possibly_constant;
	};

	void visit_native_module_declaration(clang::FunctionDecl *decl);
	void visit_task_memory_query_declaration(clang::FunctionDecl *decl);
	void visit_task_initializer_declaration(clang::FunctionDecl *decl);
	void visit_task_voice_initializer_declaration(clang::FunctionDecl *decl);
	void visit_task_function_declaration(clang::FunctionDecl *decl);

	void build_native_module_type_table();
	c_native_module_qualified_data_type get_native_module_qualified_data_type(clang::QualType type) const;

	void build_task_type_table();
	s_task_argument_type get_task_qualified_data_type(clang::QualType type) const;

	bool parse_optimization_rule(const char *rule, std::vector<std::string> &out_tokens) const;

	std::vector<s_task_function_argument_declaration> parse_task_arguments(
		clang::FunctionDecl *decl, const char *function_type, const char *function_type_cap,
		bool allow_only_possible_constants);

	std::string get_function_call(const clang::FunctionDecl *decl) const;

	// The clang compiler instance
	clang::CompilerInstance &m_compiler_instance;

	// Repository which we will build up as we scrape
	c_scraper_result *m_result;

	// The current library
	size_t m_current_library_index;

	// Used to emit scraper errors
	c_diagnostic m_diag;

	// Printing policy for type names
	clang::PrintingPolicy m_printing_policy;

	// Table mapping clang type to native module type
	std::map<std::string, c_native_module_qualified_data_type> m_native_module_type_table;

	// Table mapping clang type to task type
	std::map<std::string, s_task_argument_type> m_task_type_table;
};

clang::ASTConsumer *create_ast_visitor(
	clang::CompilerInstance &compiler_instance, c_scraper_result *result) {
	return new c_ast_visitor(compiler_instance, result);
}

c_ast_visitor::c_ast_visitor(
	clang::CompilerInstance &compiler_instance, c_scraper_result *result)
	: m_compiler_instance(compiler_instance)
	, m_diag(compiler_instance)
	, m_printing_policy(compiler_instance.getLangOpts()) {
	assert(result);
	m_result = result;

	m_current_library_index = k_invalid_library_index;

	m_printing_policy.Bool = true; // Print "bool" instead of "_Bool"
	m_printing_policy.SuppressTagKeyword = true;

	if (!m_compiler_instance.hasDiagnostics()) {
		m_compiler_instance.createDiagnostics();
	}

	build_native_module_type_table();
	build_task_type_table();
}

void c_ast_visitor::HandleTranslationUnit(clang::ASTContext &context) {
	if (m_compiler_instance.getDiagnosticClient().getNumErrors() > 0 ||
		m_compiler_instance.getDiagnosticClient().getNumWarnings() > 0) {
		m_diag.error(context.getTranslationUnitDecl(),
			"Compilation finished with errors or warnings, skipping scraping phase");
	} else {
		TraverseDecl(context.getTranslationUnitDecl());
	}

	m_result->set_success(
		(m_compiler_instance.getDiagnosticClient().getNumErrors() == 0) &&
		(m_compiler_instance.getDiagnosticClient().getNumWarnings() == 0));
}

bool c_ast_visitor::VisitNamespaceDecl(clang::NamespaceDecl *decl) {
	c_annotation_collection annotations(decl->attrs());

	if (annotations.contains_annotation(WL_LIBRARY_DECLARATION)) {
		if (m_current_library_index != k_invalid_library_index) {
			m_diag.error(decl, "Cannot declare library within existing library '%0'") <<
				m_result->get_library(m_current_library_index).name;
			return true;
		}

		std::string namespace_name = decl->getName();

		s_library_declaration library;

		c_annotation_specifications specs;
		specs.add_uint32(WL_ID_PREFIX, "ID", true, &library.id);
		specs.add_string(WL_NAME_PREFIX, "name", true, &library.name);
		specs.add_uint32(WL_VERSION_PREFIX, "version", true, &library.version);
		bool spec_result = specs.execute(
			annotations,
			("library '" + namespace_name + "' declaration").c_str(),
			&m_compiler_instance,
			decl,
			m_diag);

		if (!spec_result) {
			return true;
		}

		// Determine if there is already an identical library
		for (size_t index = 0; index < m_result->get_library_count(); index++) {
			const s_library_declaration &existing_library = m_result->get_library(index);

			if (library.id == existing_library.id ||
				library.name == existing_library.name) {
				// If namespace name, ID, or name match an existing library, everything must match
				if (library.id == existing_library.id &&
					library.name == existing_library.name &&
					library.version == existing_library.version) {
					m_current_library_index = index;
					break;
				} else {
					m_diag.error(decl, "Library declaration '%0' conflicts with existing library '%1'") <<
						library.name << existing_library.name;
				}
			}
		}

		if (m_current_library_index == k_invalid_library_index) {
			// Add a new library if necessary
			m_current_library_index = m_result->get_library_count();
			m_result->add_library(library);
		}

		for (auto it = decl->decls_begin(); it != decl->decls_end(); it++) {
			if (!TraverseDecl(*it)) {
				return false;
			}
		}

		m_current_library_index = k_invalid_library_index;
	}

	return true;
}

bool c_ast_visitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
	if (m_current_library_index == k_invalid_library_index) {
		return true;
	}

	c_annotation_collection annotations(decl->attrs());

	if (annotations.contains_annotation(WL_NATIVE_MODULE_DECLARATION)) {
		visit_native_module_declaration(decl);
	}

	if (annotations.contains_annotation(WL_TASK_MEMORY_QUERY)) {
		visit_task_memory_query_declaration(decl);
	}

	if (annotations.contains_annotation(WL_TASK_INITIALIZER)) {
		visit_task_initializer_declaration(decl);
	}

	if (annotations.contains_annotation(WL_TASK_VOICE_INITIALIZER)) {
		visit_task_voice_initializer_declaration(decl);
	}

	if (annotations.contains_annotation(WL_TASK_FUNCTION_DECLARATION)) {
		visit_task_function_declaration(decl);
	}

	return true;
}

bool c_ast_visitor::VisitRecordDecl(clang::RecordDecl *decl) {
	if (m_current_library_index == k_invalid_library_index) {
		return true;
	}

	c_annotation_collection annotations(decl->attrs());

	// Scrape standalone attributes
	for (c_annotation_iterator it(annotations); it.is_valid(); it.next()) {
		if (string_starts_with(it.get_annotation(), WL_OPTIMIZATION_RULE_PREFIX)) {
			s_optimization_rule_declaration optimization_rule;
			optimization_rule.library_index = m_current_library_index;

			bool parse_result = parse_optimization_rule(
				it.get_annotation() + strlen(WL_OPTIMIZATION_RULE_PREFIX), optimization_rule.tokens);

			if (parse_result) {
				m_result->add_optimization_rule(optimization_rule);
			} else {
				m_diag.error(it.get_source_location(), "Failed to parse optimization rule '%0'") << it.get_annotation();
			}
		}
	}

	return true;
}

void c_ast_visitor::visit_native_module_declaration(clang::FunctionDecl *decl) {
	c_annotation_collection annotations(decl->attrs());

	s_native_module_declaration native_module;
	native_module.library_index = m_current_library_index;
	std::string function_name = decl->getName();
	native_module.first_argument_is_context = false;
	native_module.compile_time_call = function_name;
	native_module.compile_time_function_call = get_function_call(decl);

	if (!decl->getReturnType().getCanonicalType().getTypePtr()->isVoidType()) {
		m_diag.error(decl, "Native module '%0' declaration return type must be void") << function_name;
	}

	c_annotation_specifications specs;
	bool runtime_only;
	specs.add_uint32(WL_ID_PREFIX, "ID", true, &native_module.id);
	specs.add_string(WL_NAME_PREFIX, "name", true, &native_module.identifier);
	specs.add_string(WL_OPERATOR_PREFIX, "operator", false, &native_module.native_operator);
	specs.add_existence(WL_RUNTIME_ONLY, "runtime only", &runtime_only);
	bool spec_result = specs.execute(
		annotations,
		("native module '" + function_name + "' declaration").c_str(),
		&m_compiler_instance,
		decl,
		m_diag);

	if (!spec_result) {
		return;
	}

	// For the name, remove anything after the '$'
	native_module.name = native_module.identifier.substr(0, native_module.identifier.find_first_of('$'));

	if (runtime_only) {
		native_module.compile_time_call.clear();
	}

	bool found_out_return_argument = false;

	for (clang::ParmVarDecl **it = decl->param_begin(); it != decl->param_end(); it++) {
		clang::ParmVarDecl *param_decl = *it;

		if (it == decl->param_begin()) {
			clang::QualType type = param_decl->getType();
			const clang::Type *canonical_type = type.getCanonicalType().getTypePtr();
			std::string type_string = clang::QualType::getAsString(canonical_type, type.getQualifiers());

			if (type_string == "const struct s_native_module_context &") {
				native_module.first_argument_is_context = true;
				continue;
			}
		}

		c_annotation_collection param_annotations(param_decl->getAttrs());

		s_native_module_argument_declaration argument_declaration;
		argument_declaration.name = param_decl->getName();
		argument_declaration.type = get_native_module_qualified_data_type(param_decl->getType());
		argument_declaration.is_return_value = false;

		if (!argument_declaration.type.is_valid()) {
			m_diag.error(param_decl, "Unsupported type for parameter '%0' of native module '%1'") <<
				argument_declaration.name << function_name;
			continue;
		}

		bool in_argument = param_annotations.contains_annotation(WL_IN);
		bool out_argument = param_annotations.contains_annotation(WL_OUT);
		bool in_const_argument = param_annotations.contains_annotation(WL_IN_CONST);
		bool out_return_argument = param_annotations.contains_annotation(WL_OUT_RETURN);

		// Make sure the parameter markup matches the type
		bool param_error = false;
		if ((in_argument + out_argument + in_const_argument + out_return_argument) != 1) {
			param_error = true;
		} else if (in_argument || in_const_argument) {
			if (argument_declaration.type.get_qualifier() != k_native_module_qualifier_in) {
				param_error = true;
			} else if (in_const_argument) {
				// Convert in to constant
				argument_declaration.type = c_native_module_qualified_data_type(
					argument_declaration.type.get_data_type(), k_native_module_qualifier_constant);
			}
		} else if (out_argument || out_return_argument) {
			if ((argument_declaration.type.get_qualifier() != k_native_module_qualifier_out) ||
				(out_return_argument && found_out_return_argument)) {
				param_error = true;
			} else if (out_return_argument) {
				argument_declaration.is_return_value = true;
			}
		}

		if (param_error) {
			m_diag.error(param_decl, "Invalid qualifier(s) on parameter '%0' of native module '%1'") <<
				argument_declaration.name << function_name;
		}

		native_module.arguments.push_back(argument_declaration);
	}

	m_result->add_native_module(native_module);
}

void c_ast_visitor::visit_task_memory_query_declaration(clang::FunctionDecl *decl) {
	s_task_memory_query_declaration task_memory_query;
	std::string function_name = decl->getName();
	task_memory_query.name = function_name;
	task_memory_query.function_call = get_function_call(decl);

	if (!decl->getReturnType().getCanonicalType().getTypePtr()->isUnsignedIntegerType()) {
		m_diag.error(decl, "Task memory query '%0' declaration return type must be unsigned integral type") <<
			function_name;
	}

	task_memory_query.arguments = parse_task_arguments(decl, "task memory query", "Task memory query", true);

	m_result->add_task_memory_query(task_memory_query);
}

void c_ast_visitor::visit_task_initializer_declaration(clang::FunctionDecl *decl) {
	s_task_initializer_declaration task_initializer;
	std::string function_name = decl->getName();
	task_initializer.name = function_name;
	task_initializer.function_call = get_function_call(decl);

	if (!decl->getReturnType().getCanonicalType().getTypePtr()->isVoidType()) {
		m_diag.error(decl, "Task initializer '%0' declaration return type must be void") << function_name;
	}

	task_initializer.arguments = parse_task_arguments(decl, "task initializer", "Task initializer", true);

	m_result->add_task_initializer(task_initializer);
}

void c_ast_visitor::visit_task_voice_initializer_declaration(clang::FunctionDecl *decl) {
	s_task_voice_initializer_declaration task_voice_initializer;
	std::string function_name = decl->getName();
	task_voice_initializer.name = function_name;
	task_voice_initializer.function_call = get_function_call(decl);

	if (!decl->getReturnType().getCanonicalType().getTypePtr()->isVoidType()) {
		m_diag.error(decl, "Task voice initializer '%0' declaration return type must be void") << function_name;
	}

	task_voice_initializer.arguments =
		parse_task_arguments(decl, "task voice initializer", "Task voice initializer", true);

	m_result->add_task_voice_initializer(task_voice_initializer);
}

void c_ast_visitor::visit_task_function_declaration(clang::FunctionDecl *decl) {
	c_annotation_collection annotations(decl->attrs());

	s_task_function_declaration task_function;
	task_function.library_index = m_current_library_index;
	std::string function_name = decl->getName();
	task_function.function = function_name;
	task_function.function_call = get_function_call(decl);

	if (!decl->getReturnType().getCanonicalType().getTypePtr()->isVoidType()) {
		m_diag.error(decl, "Task function '%0' declaration return type must be void") << function_name;
	}

	c_annotation_specifications specs;
	specs.add_uint32(WL_ID_PREFIX, "ID", true, &task_function.id);
	specs.add_string(WL_NAME_PREFIX, "name", true, &task_function.name);
	specs.add_string(WL_SOURCE_PREFIX, "native module source", true, &task_function.native_module_source);
	specs.add_string(WL_TASK_MEMORY_QUERY_FUNCTION_PREFIX, "task memory query", false, &task_function.memory_query);
	specs.add_string(WL_TASK_INITIALIZER_FUNCTION_PREFIX, "task initializer", false, &task_function.initializer);
	specs.add_string(WL_TASK_VOICE_INITIALIZER_FUNCTION_PREFIX, "task voice initializer", false,
		&task_function.voice_initializer);

	bool spec_result = specs.execute(
		annotations,
		("task function '" + function_name + "' declaration").c_str(),
		&m_compiler_instance,
		decl,
		m_diag);

	if (!spec_result) {
		return;
	}

	task_function.arguments = parse_task_arguments(decl, "task function", "Task function", false);

	m_result->add_task_function(task_function);
}

void c_ast_visitor::build_native_module_type_table() {
	wl_assert(m_native_module_type_table.empty());

	// Map type string to type. A better/more robust way to do this would be to apply markup to base types in C++ so we
	// could recognize them by their clang::Type, but this is easier.

	for (uint32 index = 0; index < k_native_module_primitive_type_count; index++) {
		e_native_module_primitive_type primitive_type = static_cast<e_native_module_primitive_type>(index);

		static const char *k_cpp_primitive_type_names[] = {
			"float",
			"_Bool",
			"class c_native_module_string"
		};
		static_assert(NUMBEROF(k_cpp_primitive_type_names) == k_native_module_primitive_type_count,
			"Primitive type name mismatch");

		static const char *k_cpp_primitive_type_array_names[] = {
			"class c_native_module_real_array",
			"class c_native_module_bool_array",
			"class c_native_module_string_array"
		};
		static_assert(NUMBEROF(k_cpp_primitive_type_array_names) == k_native_module_primitive_type_count,
			"Primitive type array name mismatch");

		c_native_module_data_type data_type(primitive_type, false);
		c_native_module_data_type array_data_type(primitive_type, true);

		char buffer[256];

		// For each type, recognize the following syntax (for both primitive values and arrays):
		// type - in
		// const type & - in
		// type & - out

		snprintf(buffer, NUMBEROF(buffer), "%s", k_cpp_primitive_type_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(data_type, k_native_module_qualifier_in)));

		snprintf(buffer, NUMBEROF(buffer), "const %s &", k_cpp_primitive_type_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(data_type, k_native_module_qualifier_in)));

		snprintf(buffer, NUMBEROF(buffer), "%s &", k_cpp_primitive_type_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(data_type, k_native_module_qualifier_out)));

		snprintf(buffer, NUMBEROF(buffer), "%s", k_cpp_primitive_type_array_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(array_data_type, k_native_module_qualifier_in)));

		snprintf(buffer, NUMBEROF(buffer), "const %s &", k_cpp_primitive_type_array_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(array_data_type, k_native_module_qualifier_in)));

		snprintf(buffer, NUMBEROF(buffer), "%s &", k_cpp_primitive_type_array_names[primitive_type]);
		m_native_module_type_table.insert(std::make_pair(
			buffer, c_native_module_qualified_data_type(array_data_type, k_native_module_qualifier_out)));
	}
}

c_native_module_qualified_data_type c_ast_visitor::get_native_module_qualified_data_type(clang::QualType type) const {
	const clang::Type *canonical_type = type.getCanonicalType().getTypePtr();
	std::string type_string = clang::QualType::getAsString(canonical_type, type.getQualifiers());

	auto it = m_native_module_type_table.find(type_string);
	if (it == m_native_module_type_table.end()) {
		return c_native_module_qualified_data_type::invalid();
	} else {
		return it->second;
	}
}

void c_ast_visitor::build_task_type_table() {
	wl_assert(m_task_type_table.empty());

	// Map type string to type. A better/more robust way to do this would be to apply markup to base types in C++ so we
	// could recognize them by their clang::Type, but this is easier.

	s_task_argument_type type;

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_real), k_task_qualifier_in);
	type.is_constant = false;
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair( // "class c_real_const_buffer_or_constant"
		"class c_buffer_or_constant_base<const class c_buffer, float, struct s_constant_accessor_real>", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_real), k_task_qualifier_out);
	type.is_constant = false;
	type.is_possibly_constant = false;
	m_task_type_table.insert(std::make_pair("class c_real_buffer *", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_real), k_task_qualifier_in);
	type.is_constant = true;
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair("float", type));

	type.data_type =
		c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_real, true), k_task_qualifier_in);
	type.is_constant = true; // The array itself is always constant
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair( // "class c_real_array"
		"class c_wrapped_array_const<struct s_real_array_element>", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_bool), k_task_qualifier_in);
	type.is_constant = false;
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair( // "class c_bool_const_buffer_or_constant"
		"class c_buffer_or_constant_base<const class c_buffer, _Bool, struct s_constant_accessor_bool>", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_bool), k_task_qualifier_out);
	type.is_constant = false;
	type.is_possibly_constant = false;
	m_task_type_table.insert(std::make_pair("class c_bool_buffer *", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_bool), k_task_qualifier_in);
	type.is_constant = true;
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair("_Bool", type));

	type.data_type =
		c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_bool, true), k_task_qualifier_in);
	type.is_constant = true; // The array itself is always constant
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair( // "class c_bool_array"
		"class c_wrapped_array_const<struct s_bool_array_element>", type));

	type.data_type = c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_string), k_task_qualifier_in);
	type.is_constant = true;
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair("const char *", type));

	type.data_type =
		c_task_qualified_data_type(c_task_data_type(k_task_primitive_type_string, true), k_task_qualifier_in);
	type.is_constant = true; // The array itself is always constant
	type.is_possibly_constant = true;
	m_task_type_table.insert(std::make_pair( // "class c_string_array",
		"class c_wrapped_array_const<struct s_string_array_element>", type));
}

c_ast_visitor::s_task_argument_type c_ast_visitor::get_task_qualified_data_type(clang::QualType type) const {
	const clang::Type *canonical_type = type.getCanonicalType().getTypePtr();
	std::string type_string = clang::QualType::getAsString(canonical_type, type.getQualifiers());

	auto it = m_task_type_table.find(type_string);
	if (it == m_task_type_table.end()) {
		s_task_argument_type result;
		result.data_type = c_task_qualified_data_type::invalid();
		result.is_constant = false;
		return result;
	} else {
		return it->second;
	}
}

bool c_ast_visitor::parse_optimization_rule(const char *rule, std::vector<std::string> &out_tokens) const {
	// Simple parser - recognizes { } [ ] , -> and identifiers
	std::string next_token;

	const char *next_char = rule;
	while (true) {
		char ch = *next_char;
		next_char++;

		if (ch == '\0') {
			if (!next_token.empty()) {
				out_tokens.push_back(next_token);
			}

			// Add an extra empty token to simplify token lookahead
			out_tokens.push_back(std::string());
			break;
		} else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
			// Whitespace terminates tokens
			if (!next_token.empty()) {
				out_tokens.push_back(next_token);
				next_token.clear();
			}
		} else if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == ',') {
			// Terminate the current token
			if (!next_token.empty()) {
				out_tokens.push_back(next_token);
				next_token.clear();
			}

			// Add this token immediately - it is only a single character
			next_token = ch;
			out_tokens.push_back(next_token);
			next_token.clear();
		} else if (ch == '-') {
			// Terminate the current token
			if (!next_token.empty()) {
				out_tokens.push_back(next_token);
				next_token.clear();
			}

			next_token = ch;
		} else if (ch >= 32 && ch <= 126) {
			// This covers all other valid characters
			next_token.push_back(ch);

			if (ch == '>' && next_token == "->") {
				// Terminate the current token
				out_tokens.push_back(next_token);
				next_token.clear();
			}
		} else {
			// Invalid character
			return false;
		}
	}

	return true;
}

std::vector<s_task_function_argument_declaration> c_ast_visitor::parse_task_arguments(
	clang::FunctionDecl *decl, const char *function_type, const char *function_type_cap,
	bool allow_only_possible_constants) {
	std::vector<s_task_function_argument_declaration> result;

	std::string function_name = decl->getName();

	if (decl->param_empty()) {
		m_diag.error(decl, "%0 '%1' must take task function context as its first parameter") <<
			function_type_cap << function_name;
	}

	for (clang::ParmVarDecl **it = decl->param_begin(); it != decl->param_end(); it++) {
		clang::ParmVarDecl *param_decl = *it;

		if (it == decl->param_begin()) {
			clang::QualType type = param_decl->getType();
			const clang::Type *canonical_type = type.getCanonicalType().getTypePtr();
			std::string type_string = clang::QualType::getAsString(canonical_type, type.getQualifiers());

			if (type_string != "const struct s_task_function_context &") {
				m_diag.error(decl, "%0 '%1' must take task function context as its first parameter") <<
					function_type_cap << function_name;
			}

			continue;
		}

		c_annotation_collection param_annotations(param_decl->getAttrs());

		s_task_function_argument_declaration argument_declaration;
		argument_declaration.name = param_decl->getName();

		s_task_argument_type type = get_task_qualified_data_type(param_decl->getType());
		argument_declaration.type = type.data_type;
		argument_declaration.is_constant = type.is_constant;
		argument_declaration.is_possibly_constant = type.is_possibly_constant;

		if (!argument_declaration.type.is_valid()) {
			m_diag.error(param_decl, "Unsupported type for parameter '%0' of %1 '%2'") <<
				argument_declaration.name << function_type << function_name;
			continue;
		}

		if (allow_only_possible_constants && !argument_declaration.is_possibly_constant) {
			m_diag.error(param_decl, "Parameter '%0' of %1 '%2' cannot ever be constant") <<
				argument_declaration.name << function_type << function_name;
			continue;
		}

		const char *in_argument = param_annotations.contains_annotation_with_prefix(WL_IN_SOURCE_PREFIX);
		const char *out_argument = param_annotations.contains_annotation_with_prefix(WL_OUT_SOURCE_PREFIX);

		// Make sure the parameter markup matches the type
		bool param_error = false;
		if (in_argument && out_argument) {
			if (argument_declaration.type.get_qualifier() == k_task_qualifier_out) {
				// Convert out to inout
				argument_declaration.type = c_task_qualified_data_type(argument_declaration.type.get_data_type(),
					k_task_qualifier_inout);
				argument_declaration.in_source = in_argument + strlen(WL_IN_SOURCE_PREFIX);
				argument_declaration.out_source = out_argument + strlen(WL_OUT_SOURCE_PREFIX);
			} else {
				param_error = true;
			}
		} else if (in_argument) {
			if (argument_declaration.type.get_qualifier() != k_task_qualifier_in) {
				param_error = true;
			} else {
				argument_declaration.in_source = in_argument + strlen(WL_IN_SOURCE_PREFIX);
			}
		} else if (out_argument) {
			if (argument_declaration.type.get_qualifier() != k_task_qualifier_out) {
				param_error = true;
			} else {
				argument_declaration.out_source = out_argument + strlen(WL_OUT_SOURCE_PREFIX);
			}
		} else {
			param_error = true;
		}

		if (param_error) {
			m_diag.error(param_decl, "Invalid qualifier(s) on parameter '%0' of %1 '%2'") <<
				argument_declaration.name << function_type << function_name;
		}

		result.push_back(argument_declaration);
	}

	return result;
}

std::string c_ast_visitor::get_function_call(const clang::FunctionDecl *decl) const {
	std::string result;
	decl->printQualifiedName(llvm::raw_string_ostream(result), m_printing_policy);
	return result;
}
