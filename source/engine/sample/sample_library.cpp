#include "common/utility/file_utility.h"

#include "engine/math/math_constants.h" // MEGA HACK
#include "engine/sample/sample.h"
#include "engine/sample/sample_library.h"

const uint32 c_sample_library::k_invalid_handle;

c_sample_library::c_sample_library() {
}

c_sample_library::~c_sample_library() {
	// This will clear out all samples
	clear_requested_samples();
	update_loaded_samples();
	wl_assert(m_requested_samples.empty());
	wl_assert(m_previous_requested_samples.empty());
}

void c_sample_library::initialize(const char *root_path) {
	wl_assert(m_root_path.empty());
	m_root_path = root_path;
}

void c_sample_library::clear_requested_samples() {
	wl_vassert(m_previous_requested_samples.empty(), "Call update_loaded_samples() before clearing");
	m_previous_requested_samples.swap(m_requested_samples);
}

uint32 c_sample_library::request_sample(const s_file_sample_parameters &parameters) {
	wl_assert(parameters.filename);
	wl_assert(valid_enum_index(parameters.loop_mode));

	// MEGA HACK: currently, scriptable wavetables don't really work because the compiler is too slow to deal with large
	// arrays. To work around this for now, hardcode a few predefined wavetables here.

	if (strcmp(parameters.filename, "__native_sin") == 0) {
		s_wavetable_sample_parameters wavetable_parameters;
		std::vector<real32> harmonic_weights(1024, 0.0f);
		harmonic_weights[0] = 1.0f;
		wavetable_parameters.harmonic_weights =
			c_wrapped_array<const real32>(&harmonic_weights.front(), harmonic_weights.size());
		wavetable_parameters.sample_count = 2048;
		wavetable_parameters.phase_shift_enabled = parameters.phase_shift_enabled;
		return request_sample(wavetable_parameters);
	} else if (strcmp(parameters.filename, "__native_sawtooth") == 0) {
		s_wavetable_sample_parameters wavetable_parameters;
		std::vector<real32> harmonic_weights(1024, 0.0f);
		for (size_t index = 0; index < harmonic_weights.size(); index++) {
			harmonic_weights[index] = static_cast<real32>(-2.0 / (k_pi * static_cast<real64>(index + 1)));
		}
		wavetable_parameters.harmonic_weights =
			c_wrapped_array<const real32>(&harmonic_weights.front(), harmonic_weights.size());
		wavetable_parameters.sample_count = 2048;
		wavetable_parameters.phase_shift_enabled = parameters.phase_shift_enabled;
		return request_sample(wavetable_parameters);
	} else if (strcmp(parameters.filename, "__native_triangle") == 0) {
		s_wavetable_sample_parameters wavetable_parameters;
		std::vector<real32> harmonic_weights(1024, 0.0f);
		for (size_t index = 0; index < harmonic_weights.size(); index++) {
			if ((index + 1) % 2 != 0) {
				real64 s = (index / 2) % 2 == 0 ? 1.0 : -1.0;
				real64 sqrt_denom = k_pi * static_cast<real64>(index + 1);
				harmonic_weights[index] = static_cast<real32>(s * 8.0 / (sqrt_denom * sqrt_denom));
			}
		}
		wavetable_parameters.harmonic_weights =
			c_wrapped_array<const real32>(&harmonic_weights.front(), harmonic_weights.size());
		wavetable_parameters.sample_count = 2048;
		wavetable_parameters.phase_shift_enabled = parameters.phase_shift_enabled;
		return request_sample(wavetable_parameters);
	}

	// END MEGA HACK

	s_requested_sample requested_sample;
	requested_sample.sample_type = e_sample_type::k_file;
	if (is_path_relative(parameters.filename)) {
		requested_sample.file_path = m_root_path + parameters.filename;
	} else {
		requested_sample.file_path = parameters.filename;
	}
	requested_sample.loop_mode = parameters.loop_mode;
	requested_sample.timestamp = 0;

	requested_sample.sample_count = 0;

	requested_sample.phase_shift_enabled = parameters.phase_shift_enabled;
	if (parameters.loop_mode == e_sample_loop_mode::k_none) {
		// Phase doesn't exist without looping
		requested_sample.phase_shift_enabled = false;
	}
	requested_sample.sample = nullptr;

	return request_sample(requested_sample);
}

uint32 c_sample_library::request_sample(const s_wavetable_sample_parameters &parameters) {
	s_requested_sample requested_sample;
	requested_sample.sample_type = e_sample_type::k_wavetable;

	requested_sample.loop_mode = e_sample_loop_mode::k_loop;
	requested_sample.timestamp = 0;

	for (size_t index = 0; index < parameters.harmonic_weights.get_count(); index++) {
		requested_sample.harmonic_weights.push_back(parameters.harmonic_weights[index]);
	}
	requested_sample.sample_count = parameters.sample_count;

	requested_sample.phase_shift_enabled = parameters.phase_shift_enabled;
	requested_sample.sample = nullptr;

	return request_sample(requested_sample);
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

		if (request.sample_type == e_sample_type::k_file) {
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
				request.sample =
					c_sample::load_file(request.file_path.c_str(), request.loop_mode, request.phase_shift_enabled);
			}
		} else if (request.sample_type == e_sample_type::k_wavetable) {
			if (!request.sample) {
				c_wrapped_array<const real32> harmonic_weights(
					&request.harmonic_weights.front(), request.harmonic_weights.size());
				request.sample =
					c_sample::generate_wavetable(harmonic_weights, request.sample_count, request.phase_shift_enabled);
			}
		}
	}
}

const c_sample *c_sample_library::get_sample(uint32 handle) const {
	if (handle == k_invalid_handle) {
		return nullptr;
	} else {
		return m_requested_samples[handle].sample;
	}
}

bool c_sample_library::are_requested_samples_equal(
	const s_requested_sample &requested_sample_a, const s_requested_sample &requested_sample_b) {
	if (requested_sample_a.sample_type != requested_sample_b.sample_type) {
		return false;
	}

	if (requested_sample_a.sample_type == e_sample_type::k_file) {
		return are_file_paths_equivalent(requested_sample_a.file_path.c_str(), requested_sample_b.file_path.c_str()) &&
			requested_sample_a.loop_mode == requested_sample_b.loop_mode &&
			requested_sample_a.timestamp == requested_sample_b.timestamp &&
			requested_sample_a.phase_shift_enabled == requested_sample_b.phase_shift_enabled;
	} else if (requested_sample_a.sample_type == e_sample_type::k_wavetable) {
		if (requested_sample_a.harmonic_weights.size() != requested_sample_b.harmonic_weights.size()) {
			return false;
		}

		for (size_t index = 0; index < requested_sample_a.harmonic_weights.size(); index++) {
			if (requested_sample_a.harmonic_weights[index] != requested_sample_b.harmonic_weights[index]) {
				return false;
			}
		}

		return requested_sample_a.sample_count == requested_sample_b.sample_count &&
			requested_sample_a.phase_shift_enabled == requested_sample_b.phase_shift_enabled;
	} else {
		wl_unreachable();
	}

	return false;
}

uint32 c_sample_library::request_sample(const s_requested_sample &requested_sample) {
	uint32 result;

	// Check for duplicates
	bool found = false;
	for (uint32 index = 0; !found && index < m_requested_samples.size(); index++) {
		const s_requested_sample &existing_sample = m_requested_samples[index];
		if (are_requested_samples_equal(existing_sample, requested_sample)) {
			result = index;
			found = true;
		}
	}

	for (uint32 index = 0; !found && index < m_previous_requested_samples.size(); index++) {
		s_requested_sample &existing_sample = m_previous_requested_samples[index];
		if (are_requested_samples_equal(existing_sample, requested_sample)) {
			// Move from the previous to the current list
			result = static_cast<uint32>(m_requested_samples.size());
			m_requested_samples.push_back(s_requested_sample());
			std::swap(existing_sample, m_requested_samples.back());
			std::swap(existing_sample, m_previous_requested_samples.back());
			m_previous_requested_samples.pop_back();
			found = true;
		}
	}
	if (!found) {
		result = static_cast<uint32>(m_requested_samples.size());
		m_requested_samples.push_back(requested_sample);
	}

	return result;
}

c_sample_library_accessor::c_sample_library_accessor(const c_sample_library &sample_library)
	: m_sample_library(sample_library) {}

const c_sample *c_sample_library_accessor::get_sample(uint32 handle) const {
	return m_sample_library.get_sample(handle);
}

c_sample_library_requester::c_sample_library_requester(c_sample_library &sample_library)
	: m_sample_library(sample_library) {}

uint32 c_sample_library_requester::request_sample(const s_file_sample_parameters &parameters) {
	return m_sample_library.request_sample(parameters);
}

uint32 c_sample_library_requester::request_sample(const s_wavetable_sample_parameters &parameters) {
	return m_sample_library.request_sample(parameters);
}
