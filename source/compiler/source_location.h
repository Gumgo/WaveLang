#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

struct s_compiler_source_file_handle_identifier {};
using h_compiler_source_file = c_handle<s_compiler_source_file_handle_identifier, size_t>;

struct s_compiler_source_location {
	h_compiler_source_file source_file_handle;
	int32 line;
	int32 character;

	s_compiler_source_location(
		h_compiler_source_file source_file_handle = h_compiler_source_file::invalid(),
		int32 line = -1,
		int32 character = -1) {
		this->source_file_handle = source_file_handle;
		this->line = line;
		this->character = character;
	}

	// Returns true if at least one component of the source location is valid
	bool is_valid() const {
		return source_file_handle.is_valid();
	}

	void clear() {
		source_file_handle = h_compiler_source_file::invalid();
		line = -1;
		character = -1;
	}
};
