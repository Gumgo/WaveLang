#include "common/utility/string_table.h"

c_string_table::c_string_table() {}

void c_string_table::clear() {
	m_table.clear();
}

char *c_string_table::initialize_for_load(size_t size) {
	m_table.resize(size);
	return m_table.empty() ? nullptr : m_table.data();
}

size_t c_string_table::add_string(const char *str) {
	size_t start_index = 0;

	while (start_index < m_table.size()) {
		if (strcmp(str, &m_table[start_index]) == 0) {
			break;
		}

		start_index += strlen(&m_table[start_index]) + 1;
	}

	if (start_index == m_table.size()) {
		size_t length = strlen(str) + 1;
		m_table.resize(m_table.size() + length);
		memcpy(&m_table[start_index], str, length);
	}

	return start_index;
}

const char *c_string_table::get_string(size_t offset) const {
	wl_assert(valid_index(offset, m_table.size()));
	return &m_table[offset];
}

size_t c_string_table::get_table_size() const {
	return m_table.size();
}

const char *c_string_table::get_table_pointer() const {
	return m_table.empty() ? nullptr : m_table.data();
}

void c_string_table::swap(c_string_table &other) {
	m_table.swap(other.m_table);
}