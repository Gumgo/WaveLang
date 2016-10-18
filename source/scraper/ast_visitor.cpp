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

class c_ast_visitor : public clang::ASTConsumer, public clang::RecursiveASTVisitor<c_ast_visitor> {
public:
	c_ast_visitor(clang::CompilerInstance &compiler_instance, c_scraper_result *result);

	void HandleTranslationUnit(clang::ASTContext &context);

	// See DeclNodes.td for a list of possible nodes we can visit
	// Or see http://clang.llvm.org/doxygen/classclang_1_1ASTDeclReader.html for a list of Visit functions
	// (http://clang.llvm.org/doxygen/classclang_1_1DeclVisitor.html appears not to be parsed correctly by Doxygen)

	// Scrape native modules and task functions
	bool VisitFunctionDecl(clang::FunctionDecl *decl);

	// Library and optimization rule parsing
	bool VisitRecordDecl(clang::RecordDecl *decl);

private:
	void build_native_module_type_table();
	c_native_module_qualified_data_type get_native_module_qualified_data_type(clang::QualType type) const;

	void parse_standalone_attributes();

	bool parse_optimization_rule(const char *rule, std::vector<std::string> &out_tokens) const;

	// The clang compiler instance
	clang::CompilerInstance &m_compiler_instance;

	// Repository which we will build up as we scrape
	c_scraper_result *m_result;

	// Used to emit scraper errors
	c_diagnostic m_diag;

	// Printing policy for type names
	clang::PrintingPolicy m_printing_policy;

	// Table mapping clang type to native module type
	std::map<std::string, c_native_module_qualified_data_type> m_native_module_type_table;

	// The declaration holding all standalone attributes
	clang::RecordDecl *m_standalone_attributes_decl;
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

	m_printing_policy.Bool = true; // Print "bool" instead of "_Bool"
	m_printing_policy.SuppressTagKeyword = true;

	if (!m_compiler_instance.hasDiagnostics()) {
		m_compiler_instance.createDiagnostics();
	}

	build_native_module_type_table();

	m_standalone_attributes_decl = nullptr;
}

void c_ast_visitor::HandleTranslationUnit(clang::ASTContext &context) {
	if (m_compiler_instance.getDiagnosticClient().getNumErrors() > 0 ||
		m_compiler_instance.getDiagnosticClient().getNumWarnings() > 0) {
		m_diag.error(context.getTranslationUnitDecl()->getSourceRange().getBegin(),
			"Compilation finished with errors or warnings, skipping scraping phase");
	} else {
		TraverseDecl(context.getTranslationUnitDecl());
		parse_standalone_attributes();
	}

	m_result->set_success(
		(m_compiler_instance.getDiagnosticClient().getNumErrors() == 0) &&
		(m_compiler_instance.getDiagnosticClient().getNumWarnings() == 0));
}

bool c_ast_visitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
	c_annotation_collection annotations(decl->attrs());

	if (annotations.contains_annotation(WL_NATIVE_MODULE_DECLARATION)) {
		s_native_module_declaration native_module;
		std::string function_name = decl->getName();
		native_module.compile_time_call = decl->getName();

		if (!decl->getReturnType().getCanonicalType().getTypePtr()->isVoidType()) {
			m_diag.error(decl->getSourceRange().getBegin(),
				"Native module '%0' declaration return type must be void") << function_name;
		}

		for (c_annotation_iterator it(annotations); it.is_valid(); it.next()) {
			if (string_starts_with(it.get_annotation(), WL_UID_PREFIX)) {
				native_module.uid_string = it.get_annotation() + strlen(WL_UID_PREFIX);
			} else if (string_starts_with(it.get_annotation(), WL_NAME_PREFIX)) {
				native_module.identifier = it.get_annotation() + strlen(WL_NAME_PREFIX);
				// For the name, remove anything after the '$'
				native_module.name = native_module.identifier.substr(0, native_module.identifier.find_first_of('$'));
			} else if (string_starts_with(it.get_annotation(), WL_OPERATOR_PREFIX)) {
				native_module.native_operator = it.get_annotation() + strlen(WL_OPERATOR_PREFIX);
			} else if (strcmp(it.get_annotation(), WL_RUNTIME_ONLY) == 0) {
				native_module.compile_time_call.clear();
			}
		}

		bool found_out_return_argument = false;

		for (clang::ParmVarDecl **it = decl->param_begin(); it != decl->param_end(); it++) {
			clang::ParmVarDecl *param_decl = *it;

			c_annotation_collection param_annotations(param_decl->getAttrs());

			s_native_module_argument_declaration argument_declaration;
			argument_declaration.name = param_decl->getName();
			argument_declaration.type = get_native_module_qualified_data_type(param_decl->getType());
			argument_declaration.is_return_value = false;

			if (!argument_declaration.type.is_valid()) {
				m_diag.error(param_decl->getSourceRange().getBegin(),
					"Unsupported type for parameter '%0' of native module '%1'") <<
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
				m_diag.error(param_decl->getSourceRange().getBegin(),
					"Invalid qualifier(s) on parameter '%0' of native module '%1'") <<
					argument_declaration.name << function_name;
			}

			native_module.arguments.push_back(argument_declaration);
		}

		m_result->add_native_module(native_module);
	}

	return true;
}

bool c_ast_visitor::VisitRecordDecl(clang::RecordDecl *decl) {
	std::string name = decl->getNameAsString();
	if (name != "s_attribute_placeholder") {
		return true;
	}

	// There seems to be a weird behavior (bug?) where each time we visit a declaration, all attributes from previous
	// declarations of the same name are also included on the declaration. Therefore, instead of scraping standalone
	// attributes as we encounter them, we find the instance of s_attribute_placeholder with the highest attribute count
	// and scrape it once at the very end.
	size_t attribute_count = decl->getAttrs().size();
	if (!m_standalone_attributes_decl ||
		attribute_count > m_standalone_attributes_decl->getAttrs().size()) {
		m_standalone_attributes_decl = decl;
	}

	return true;
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

void c_ast_visitor::parse_standalone_attributes() {
	if (!m_standalone_attributes_decl) {
		return;
	}

	c_annotation_collection annotations(m_standalone_attributes_decl->attrs());

	// Scrape standalone attributes
	for (c_annotation_iterator it(annotations); it.is_valid(); it.next()) {
		if (string_starts_with(it.get_annotation(), WL_LIBRARY_PREFIX)) {
			std::string library_attribute = it.get_annotation() + strlen(WL_LIBRARY_PREFIX);

			// Read the ID, which ends at the first comma
			size_t comma_pos = library_attribute.find_first_of(',');
			if (comma_pos == std::string::npos) {
				m_diag.error(m_standalone_attributes_decl->getSourceRange().getBegin(), "Invalid library declaration");
			} else {
				s_library_declaration library;
				library.id_string = library_attribute.substr(0, comma_pos);
				library.name = library_attribute.substr(comma_pos + 1);
				m_result->add_library(library);
			}
		} else if (string_starts_with(it.get_annotation(), WL_OPTIMIZATION_RULE_PREFIX)) {
			s_optimization_rule_declaration optimization_rule;

			bool parse_result = parse_optimization_rule(
				it.get_annotation() + strlen(WL_OPTIMIZATION_RULE_PREFIX), optimization_rule.tokens);

			if (parse_result) {
				m_result->add_optimization_rule(optimization_rule);
			} else {
				m_diag.error(m_standalone_attributes_decl->getSourceRange().getBegin(),
					"Failed to parse optimization rule");
			}
		}
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
