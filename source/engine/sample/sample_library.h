#pragma once

#include "common/common.h"

#include "engine/sample/sample.h"

//#include <filesystem>
#include <string>
#include <vector>

struct s_file_sample_parameters {
	const char *filename;
	e_sample_loop_mode loop_mode;
	bool phase_shift_enabled;
};

struct s_wavetable_sample_parameters {
	c_wrapped_array<const real32> harmonic_weights;
	uint32 sample_count;
	bool phase_shift_enabled;
};

class c_sample_library {
public:
	static const uint32 k_invalid_handle = static_cast<uint32>(-1);

	c_sample_library();
	~c_sample_library();

	void initialize(const char *root_path);

	// Clears the list of samples that have been requested
	void clear_requested_samples();

	// Requests the sample with the given filename to be loaded and returns its handle
	uint32 request_sample(const s_file_sample_parameters &parameters);

	// Requests the wavesample sample to be generated with the given parameters and returns its handle
	uint32 request_sample(const s_wavetable_sample_parameters &parameters);

	// Loads requested samples and unloads any loaded samples that have not been requested
	void update_loaded_samples();

	// Returns the sample with the given handle, or null if it failed to load
	const c_sample *get_sample(uint32 handle) const;

private:
	enum class e_sample_type {
		k_file,
		k_wavetable,

		k_count
	};

	struct s_requested_sample {
		e_sample_type sample_type;

		// File parameters:
		std::string file_path;
		e_sample_loop_mode loop_mode;
		// Modification timestamp of the loaded sample
		uint64 timestamp;

		// Wavetable parameters:
		std::vector<real32> harmonic_weights;
		uint32 sample_count;

		// Phase shift enabled
		bool phase_shift_enabled;

		// The sample, or null if it failed to load/generate
		c_sample *sample;
	};

	static bool are_requested_samples_equal(
		const s_requested_sample &requested_sample_a, const s_requested_sample &requested_sample_b);
	uint32 request_sample(const s_requested_sample &requested_sample);

	std::string m_root_path;
	std::vector<s_requested_sample> m_requested_samples;
	std::vector<s_requested_sample> m_previous_requested_samples;
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
	uint32 request_sample(const s_file_sample_parameters &parameters);
	uint32 request_sample(const s_wavetable_sample_parameters &parameters);

private:
	c_sample_library &m_sample_library;
};

