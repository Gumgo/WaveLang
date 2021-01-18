#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

#include "engine/task_functions/sampler/sample.h"

#include <string>
#include <vector>

struct s_file_sample_parameters {
	const char *filename;
	e_sample_loop_mode loop_mode;
	bool phase_shift_enabled;
};

struct s_wavetable_sample_parameters {
	c_wrapped_array<const real32> harmonic_weights;
	bool phase_shift_enabled;
};

struct s_sample_handle_identifier {};
using h_sample = c_handle<s_sample_handle_identifier, uint32>;

class c_sample_library {
public:
	c_sample_library();
	~c_sample_library();

	void initialize(const char *root_path);

	// Clears the list of samples that have been requested
	void clear_requested_samples();

	// Requests the sample with the given filename to be loaded and returns its handle
	h_sample request_sample(const s_file_sample_parameters &parameters);

	// Requests the wavesample sample to be generated with the given parameters and returns its handle
	h_sample request_sample(const s_wavetable_sample_parameters &parameters);

	// Loads requested samples and unloads any loaded samples that have not been requested
	void update_loaded_samples();

	// Returns the sample with the given handle, or null if it failed to load
	const c_sample *get_sample(h_sample handle, uint32 channel_index) const;

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

		// Phase shift enabled
		bool phase_shift_enabled;

		// A sample for each channel, or an empty list on load/generate failure
		std::vector<c_sample *> channel_samples;
	};

	static bool are_requested_samples_equal(
		const s_requested_sample &requested_sample_a,
		const s_requested_sample &requested_sample_b);
	h_sample request_sample(const s_requested_sample &requested_sample);

	std::string m_root_path;
	std::vector<s_requested_sample> m_requested_samples;
	std::vector<s_requested_sample> m_previous_requested_samples;
};
