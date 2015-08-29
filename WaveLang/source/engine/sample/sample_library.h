#ifndef WAVELANG_SAMPLE_LIBRARY_H__
#define WAVELANG_SAMPLE_LIBRARY_H__

#include "common/common.h"
#include "engine/sample/sample.h"
#include <filesystem>
#include <string>
#include <vector>

class c_sample_library {
public:
	static const uint32 k_invalid_handle = static_cast<uint32>(-1);

	c_sample_library();
	~c_sample_library();

	void initialize(const char *root_path);

	// Clears the list of samples that have been requested
	void clear_requested_samples();

	// Requests the sample with the given filename to be loaded and returns its handle
	uint32 request_sample(const char *filename, e_sample_loop_mode loop_mode);

	// Loads requested samples and unloads any loaded samples that have not been requested
	void update_loaded_samples();

	// Returns the sample with the given handle, or null if it failed to load
	const c_sample *get_sample(uint32 handle) const;

private:
	struct s_requested_sample {
		// Path to the sample file
		std::string file_path;

		// Loop mode
		e_sample_loop_mode loop_mode;

		// Modification timestamp of the loaded sample
		uint64 timestamp;

		// The sample, or null if it failed to load
		c_sample *sample;
	};

	std::string m_root_path;
	std::vector<s_requested_sample> m_requested_samples;
	std::vector<s_requested_sample> m_previous_requested_samples;
	std::vector<s_requested_sample> m_native_samples;
};

// Additional sub-interfaces
class c_sample_library_accessor {
public:
	c_sample_library_accessor(const c_sample_library &sample_library);
	const c_sample *get_sample(uint32 handle) const;

private:
	const c_sample_library &m_sample_library;
};

class c_sample_library_requester {
public:
	c_sample_library_requester(c_sample_library &sample_library);
	uint32 request_sample(const char *filename, e_sample_loop_mode loop_mode);

private:
	c_sample_library &m_sample_library;
};

#endif // WAVELANG_SAMPLE_LIBRARY_H__
