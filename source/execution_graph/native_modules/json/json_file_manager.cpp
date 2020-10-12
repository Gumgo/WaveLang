#include "common/utility/file_utility.h"

#include "execution_graph/native_modules/json/json_file_manager.h"

s_json_result c_json_file_manager::load_json_file(const char *filename, const c_json_file **json_file_out) {
	for (const s_load_request &load_request : m_load_requests) {
		if (are_file_paths_equivalent(filename, load_request.filename.c_str())) {
			// Will be null if the request previously failed
			*json_file_out = load_request.json_file.get();
			return load_request.result;
		}
	}

	m_load_requests.emplace_back(s_load_request());
	s_load_request &new_request = m_load_requests.back();
	new_request.filename = filename;

	std::unique_ptr<c_json_file> json_file(new c_json_file());
	new_request.result = json_file->load(filename);
	if (new_request.result.result == e_json_result::k_success) {
		*json_file_out = nullptr;
	} else {
		new_request.json_file.swap(json_file);
		*json_file_out = new_request.json_file.get();
	}

	return new_request.result;
}
