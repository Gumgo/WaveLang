#include "engine/sample/sample_library.h"
#include "engine/sample/sample.h"
#include "engine/sample/native_samples.h"
#include "common/utility/file_utility.h"

const uint32 c_sample_library::k_invalid_handle;
static const uint32 k_native_sample_handle_mask = 0x80000000;

c_sample_library::c_sample_library() {
}

c_sample_library::~c_sample_library() {
	// This will clear out all samples
	clear_requested_samples();
	update_loaded_samples();
	wl_assert(m_requested_samples.empty());
	wl_assert(m_previous_requested_samples.empty());

	for (size_t index = 0; index < m_native_samples.size(); index++) {
		delete m_native_samples[index].sample;
	}
}

void c_sample_library::initialize(const char *root_path) {
	wl_assert(m_root_path.empty());
	wl_assert(m_native_samples.empty());

	m_root_path = root_path;

	// Add all native samples
	for (uint32 index = 0; index < k_native_sample_count; index++) {
		m_native_samples.push_back(s_requested_sample());
		s_requested_sample &requested_sample = m_native_samples.back();
		requested_sample.file_path = get_native_sample_name(index);;
		requested_sample.timestamp = 0;
		requested_sample.sample = build_native_sample(index);
	}
}

void c_sample_library::clear_requested_samples() {
	wl_vassert(m_previous_requested_samples.empty(), "Call update_loaded_samples() before clearing");
	m_previous_requested_samples.swap(m_requested_samples);
}

uint32 c_sample_library::request_sample(const char *filename, e_sample_loop_mode loop_mode) {
	wl_assert(filename);
	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));

	uint32 result;

	const char *native_prefix = get_native_sample_prefix();
	bool is_native = (strncmp(filename, native_prefix, strlen(native_prefix)) == 0);

	if (is_native) {
		for (result = 0; result < m_native_samples.size(); result++) {
			if (m_native_samples[result].file_path == filename &&
				m_native_samples[result].loop_mode == loop_mode) {
				break;
			}
		}

		if (result < m_native_samples.size()) {
			// Succcess, add the native bit to the handle
			result |= k_native_sample_handle_mask;
		} else {
			result = k_invalid_handle;
		}
	} else {
		std::string full_path;
		if (is_path_relative(filename)) {
			full_path = m_root_path + filename;
		} else {
			full_path = filename;
		}

		// Check for duplicates
		bool found = false;
		for (uint32 index = 0; !found && index < m_requested_samples.size(); index++) {
			if (are_file_paths_equivalent(full_path.c_str(), m_requested_samples[index].file_path.c_str()) &&
				m_requested_samples[index].loop_mode == loop_mode) {
				result = index;
				found = true;
			}
		}

		if (!found) {
			// Search through previously requested sample list
			for (uint32 index = 0; !found && index < m_previous_requested_samples.size(); index++) {
				s_requested_sample &previous_requsted_sample = m_previous_requested_samples[index];
				if (are_file_paths_equivalent(full_path.c_str(), previous_requsted_sample.file_path.c_str()) &&
					previous_requsted_sample.loop_mode == loop_mode) {
					// Move from the previous to the current list
					result = m_requested_samples.size();
					m_requested_samples.push_back(s_requested_sample());
					std::swap(previous_requsted_sample, m_previous_requested_samples.back());
					std::swap(m_requested_samples.back(), m_previous_requested_samples.back());
					m_previous_requested_samples.pop_back();
					found = true;
				}
			}
		}

		if (!found) {
			result = m_requested_samples.size();
			m_requested_samples.push_back(s_requested_sample());
			s_requested_sample &new_request = m_requested_samples.back();
			new_request.file_path = full_path;
			new_request.loop_mode = loop_mode;
			new_request.timestamp = 0;
			new_request.sample = nullptr;
		}
	}

	return result;
}

void c_sample_library::update_loaded_samples() {
	// Unload all previously loaded samples which were not re-referenced
	for (size_t index = 0; index < m_previous_requested_samples.size(); index++) {
		delete m_previous_requested_samples[index].sample;
	}

	m_previous_requested_samples.clear();

	// Load any new or modified samples
	for (size_t index = 0; index < m_requested_samples.size(); index++) {
		s_requested_sample &request = m_requested_samples[index];

		uint64 new_timestamp = 0;
		bool obtained_timestamp = get_file_last_modified_timestamp(request.file_path.c_str(), new_timestamp);

		if (request.sample == nullptr ||
			!obtained_timestamp ||
			new_timestamp > request.timestamp) {
			// Clear out the current sample for reload
			delete request.sample;
			request.sample = nullptr;
		}

		if (!request.sample) {
			request.timestamp = new_timestamp;
			request.sample = c_sample::load(request.file_path.c_str(), request.loop_mode);
		}
	}
}

const c_sample *c_sample_library::get_sample(uint32 handle) const {
	if (handle == k_invalid_handle) {
		return nullptr;
	} else if ((handle & k_native_sample_handle_mask) == 0) {
		return m_requested_samples[handle].sample;
	} else {
		return m_native_samples[handle & ~k_native_sample_handle_mask].sample;
	}
}

c_sample_library_accessor::c_sample_library_accessor(const c_sample_library &sample_library)
	: m_sample_library(sample_library) {}

const c_sample *c_sample_library_accessor::get_sample(uint32 handle) const {
	return m_sample_library.get_sample(handle);
}

c_sample_library_requester::c_sample_library_requester(c_sample_library &sample_library)
	: m_sample_library(sample_library) {}

uint32 c_sample_library_requester::request_sample(const char *filename, e_sample_loop_mode loop_mode) {
	return m_sample_library.request_sample(filename, loop_mode);
}
