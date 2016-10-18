#include "scraper/annotation_collection.h"

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

bool c_annotation_collection::contains_annotation_with_prefix(const char *annotation_prefix) const {
	assert(annotation_prefix);

	for (size_t index = 0; index < m_annotations.size(); index++) {
		if (string_starts_with(m_annotations[index].annotation_data.c_str(), annotation_prefix)) {
			return true;
		}
	}

	return false;
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
