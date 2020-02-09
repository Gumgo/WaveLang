#pragma once

#include "common/common.h"

#include "scraper/diagnostic.h"

#pragma warning(push, 0) // Disable warnings for LLVM
#include <clang/AST/Attr.h>
#include <clang/AST/AttrIterator.h>
#pragma warning(pop)

#include <string>
#include <vector>

// A collection of annotation attributes scraped from a declaration
class c_annotation_collection {
public:
	c_annotation_collection(const clang::Decl::attr_range &attributes);
	bool contains_annotation(const char *annotation) const;
	const char *contains_annotation_with_prefix(const char *annotation_prefix) const;

private:
	friend class c_annotation_iterator;

	struct s_annotation {
		std::string annotation_data;
		clang::SourceLocation source_location;
	};

	std::vector<s_annotation> m_annotations;
};

class c_annotation_iterator {
public:
	c_annotation_iterator(const c_annotation_collection &annotation_collection);

	bool is_valid() const;
	void next();
	const char *get_annotation() const;
	clang::SourceLocation get_source_location() const;

private:
	const c_annotation_collection &m_annotation_collection;
	size_t m_index;
};

// Allows you to specify a set of required and optional annotations and verifies that the provided annotation collection
// matches the specification
class c_annotation_specifications {
public:
	void add_existence(const char *annotation, const char *name, bool *storage);
	void add_string(const char *prefix, const char *name, bool required, std::string *storage);
	void add_uint32(const char *prefix, const char *name, bool required, uint32 *storage);

	bool execute(
		const c_annotation_collection &annotations,
		const char *context,
		clang::CompilerInstance *compiler_instance,
		clang::Decl *decl,
		c_scraper_diagnostic &diag);

private:
	enum class e_annotation_type {
		k_existence,
		k_string,
		k_uint32,

		k_count
	};

	struct s_annotation_specification {
		e_annotation_type type;
		std::string annotation_or_prefix;
		std::string name;
		bool required;

		union {
			void *storage;
			bool *bool_storage;
			std::string *string_storage;
			uint32 *uint32_storage;
		};

		bool found;
	};

	void add_specification_internal(
		e_annotation_type type,
		const char *annotation_or_prefix,
		const char *name,
		bool required,
		void *storage);

	clang::NamedDecl *lookup_decl(
		const char *name,
		clang::SourceLocation name_source_location,
		clang::CompilerInstance *compiler_instance,
		clang::DeclContext *decl_context);

	bool lookup_uint32(
		const char *name_or_value,
		clang::SourceLocation name_source_location,
		clang::CompilerInstance *compiler_instance,
		clang::DeclContext *decl_context,
		clang::ASTContext *ast_context,
		uint32 &result);

	std::vector<s_annotation_specification> m_annotation_specifications;
};

