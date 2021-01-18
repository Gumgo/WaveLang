#pragma once

#include "common/common.h"

#include "instrument/native_modules/json/json_file.h"

#include <memory>
#include <vector>

class c_json_file_manager {
public:
	c_json_file_manager() = default;

	s_json_result load_json_file(const char *filename, const c_json_file **json_file_out);

private:
	struct s_load_request {
		std::string filename;
		s_json_result result;
		std::unique_ptr<c_json_file> json_file; // Null if the file failed to load
	};

	std::vector<s_load_request> m_load_requests;
};
