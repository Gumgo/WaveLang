#include "scraper/annotation_collection.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/Lex/LiteralSupport.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Lookup.h>
#pragma warning(pop)

c_annotation_collection::c_annotation_collection(const clang::Decl::attr_range &attributes) {
	for (clang::AttrVec::const_iterator iterator = attributes.begin(); iterator != attributes.end(); iterator++) {
		const clang::Attr *attribute = *iterator;
		const clang::AnnotateAttr *annotation_attribute = clang::dyn_cast<clang::AnnotateAttr>(attribute);
		if (!annotation_attribute) {
			// Ignore everything but annotations
			continue;
		}

		llvm::StringRef annotation_content = annotation_attribute->getAnnotation();
		m_annotations.push_back(s_annotation());
		m_annotations.back().annotation_data = annotation_content.str();
		m_annotations.back().source_location = attribute->getLocation();
	}

	// Clang seems to store attributes in reverse order, so flip them
	for (size_t index = 0; index < m_annotations.size() / 2; index++) {
		std::swap(m_annotations[index], m_annotations[m_annotations.size() - index - 1]);
	}
}

bool c_annotation_collection::contains_annotation(const char *annotation) const {
	assert(annotation);

	for (size_t index = 0; index < m_annotations.size(); index++) {
		if (m_annotations[index].annotation_data == annotation) {
			return true;
		}
	}

	return false;
}

const char *c_annotation_collection::contains_annotation_with_prefix(const char *annotation_prefix) const {
	assert(annotation_prefix);

	for (size_t index = 0; index < m_annotations.size(); index++) {
		if (string_starts_with(m_annotations[index].annotation_data.c_str(), annotation_prefix)) {
			return m_annotations[index].annotation_data.c_str();
		}
	}

	return nullptr;
}

c_annotation_iterator::c_annotation_iterator(const c_annotation_collection &annotation_collection)
	: m_annotation_collection(annotation_collection) {
	m_index = 0;
}

bool c_annotation_iterator::is_valid() const {
	return m_index < m_annotation_collection.m_annotations.size();
}

void c_annotation_iterator::next() {
	assert(is_valid());
	m_index++;
}

const char *c_annotation_iterator::get_annotation() const {
	assert(is_valid());
	return m_annotation_collection.m_annotations[m_index].annotation_data.c_str();
}

clang::SourceLocation c_annotation_iterator::get_source_location() const {
	assert(is_valid());
	return m_annotation_collection.m_annotations[m_index].source_location;
}

void c_annotation_specifications::add_existence(const char *annotation, const char *name, bool *storage) {
	add_specification_internal(k_annotation_type_existence, annotation, name, false, storage);
	*storage = false;
}

void c_annotation_specifications::add_string(
	const char *prefix, const char *name, bool required, std::string *storage) {
	add_specification_internal(k_annotation_type_string, prefix, name, required, storage);
	storage->clear();
}
void c_annotation_specifications::add_uint32(
	const char *prefix, const char *name, bool required, uint32 *storage) {
	add_specification_internal(k_annotation_type_uint32, prefix, name, required, storage);
	*storage = 0;
}

bool c_annotation_specifications::execute(
	const c_annotation_collection &annotations,
	const char *context,
	clang::CompilerInstance *compiler_instance,
	clang::Decl *decl,
	c_diagnostic &diag) {
	bool result = true;

	clang::DeclContext *decl_context = decl->getDeclContext();
	clang::ASTContext &ast_context = decl->getASTContext();

	// Iterate through all annotations
	for (c_annotation_iterator it(annotations); it.is_valid(); it.next()) {
		// Try to find matching specifications
		for (size_t index = 0; index < m_annotation_specifications.size(); index++) {
			s_annotation_specification &spec = m_annotation_specifications[index];

			if (!string_starts_with(it.get_annotation(), spec.annotation_or_prefix.c_str())) {
				continue;
			}

			if (spec.found) {
				diag.error(it.get_source_location(), "Duplicate %0 specified on %1") << spec.name << context;
				result = false;
				continue;
			}

			const char *annotation_value = it.get_annotation() + spec.annotation_or_prefix.length();

			if (spec.type == k_annotation_type_existence) {
				*spec.bool_storage = true;
				spec.found = true;
			} else if (spec.type == k_annotation_type_string) {
				*spec.string_storage = annotation_value;
				spec.found = true;
			} else if (spec.type == k_annotation_type_uint32) {
				bool lookup_result = lookup_uint32(
					annotation_value, it.get_source_location(),
					compiler_instance, decl_context, &ast_context,
					*spec.uint32_storage);

				if (!lookup_result) {
					diag.error(it.get_source_location(), "Invalid %0 specified on %1") << spec.name << context;
					result = false;
				}

				spec.found = true;
			} else {
				wl_unreachable();
			}
		}
	}

	// Detect required specifications which weren't found
	for (size_t index = 0; index < m_annotation_specifications.size(); index++) {
		const s_annotation_specification &spec = m_annotation_specifications[index];

		if (spec.required && !spec.found) {
			diag.error(decl, "No %0 specified on %1") << spec.name << context;
			result = false;
		}
	}

	return result;
}

void c_annotation_specifications::add_specification_internal(
	e_annotation_type type, const char *annotation_or_prefix, const char *name, bool required, void *storage) {
	wl_assert(VALID_INDEX(type, k_annotation_type_count));
	wl_assert(annotation_or_prefix);
	wl_assert(name);
	wl_assert(storage);
	s_annotation_specification spec;
	spec.type = type;
	spec.annotation_or_prefix = annotation_or_prefix;
	spec.name = name;
	spec.required = required;
	spec.storage = storage;
	spec.found = false;
	m_annotation_specifications.push_back(spec);
}

clang::NamedDecl *c_annotation_specifications::lookup_decl(
	const char *name, clang::SourceLocation name_source_location,
	clang::CompilerInstance *compiler_instance, clang::DeclContext *decl_context) {
	clang::IdentifierInfo *identifier_info = compiler_instance->getPreprocessor().getIdentifierInfo(name);
	if (!identifier_info) {
		return nullptr;
	}

	clang::DeclarationName declaration_name(identifier_info);
	clang::Scope *scope = compiler_instance->getSema().getScopeForContext(decl_context);

	return compiler_instance->getSema().LookupSingleName(
		scope,
		declaration_name,
		name_source_location,
		clang::Sema::LookupOrdinaryName,
		clang::Sema::NotForRedeclaration);
}

bool c_annotation_specifications::lookup_uint32(const char *name_or_value, clang::SourceLocation name_source_location,
	clang::CompilerInstance *compiler_instance, clang::DeclContext *decl_context, clang::ASTContext *ast_context,
	uint32 &result) {
	wl_assert(name_or_value);

	llvm::APSInt unbounded_integer(64, true);

	// If the string starts with a number, try to parse it as an integer
	// Try to parse as a literal value first
	char first_char = *name_or_value;
	if (first_char >= '0' && first_char <= '9') {
		clang::NumericLiteralParser parser(
			llvm::StringRef(name_or_value),
			name_source_location,
			compiler_instance->getPreprocessor());

		if (parser.hadError || !parser.isIntegerLiteral() || parser.GetIntegerValue(unbounded_integer)) {
			return false;
		}
	} else {
		// Attempt name lookup and try to resolve the init value
		clang::NamedDecl *lookup_result = lookup_decl(
			name_or_value, name_source_location, compiler_instance, decl_context);

		if (!lookup_result) {
			return false;
		}

		clang::VarDecl *variable_decl = llvm::dyn_cast<clang::VarDecl>(lookup_result);

		if (!variable_decl ||
			!variable_decl->hasInit() ||
			!variable_decl->getInit()->EvaluateAsInt(unbounded_integer, *ast_context)) {
			return false;
		}
	}

	if (unbounded_integer.isNegative() || unbounded_integer.ugt(std::numeric_limits<uint32>::max())) {
		return false;
	}

	result = static_cast<uint32>(unbounded_integer.getZExtValue());
	return true;
}
