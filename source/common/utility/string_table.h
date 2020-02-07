#pragma once

#include "common/common.h"

#include <vector>

class c_string_table {
public:
	c_string_table();

	void clear();

	// Resizes the table to the given size in preparation to be loaded, and returns a writable pointer to the beginning
	char *initialize_for_load(size_t size);

	// Adds the string to the table and returns the offset
	size_t add_string(const char *str);

	// Returns the string at the given offset
	const char *get_string(size_t offset) const;

	// Returns the total size of the table
	size_t get_table_size() const;

	// Returns a pointer to the beginning of the table
	const char *get_table_pointer() const;

	// Swaps with the data of another string table
	void swap(c_string_table &other);

private:
	std::vector<char> m_table;
};

