#ifndef WAVELANG_SCRAPER_ANNOTATION_COLLECTION_H__
#define WAVELANG_SCRAPER_ANNOTATION_COLLECTION_H__

#include "common/common.h"

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
	bool contains_annotation_with_prefix(const char *annotation_prefix) const;

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

#endif // WAVELANG_SCRAPER_ANNOTATION_COLLECTION_H__
