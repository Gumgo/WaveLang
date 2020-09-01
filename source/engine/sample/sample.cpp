#include "common/utility/file_utility.h"
#include "common/utility/sha1/SHA1.h"

#include "engine/math/math_constants.h"
#include "engine/math/simd.h"
#include "engine/sample/sample.h"

#include <fstream>
#include <iostream>

static const uint32 k_min_mipmap_sample_count = 4;

static const char *k_wavetable_cache_folder = "cache";
static const char *k_wavetable_cache_extension = ".wtc";
static const char k_wavetable_cache_identifier[] = { 'w', 't', 'c', 'h' };
static const uint32 k_wavetable_cache_version = 0;

struct s_wave_riff_header {
	uint32 chunk_id;
	uint32 chunk_size;
	uint32 format;
};

struct s_wave_fmt_subchunk {
	uint32 subchunk_1_id;
	uint32 subchunk_1_size;
	uint16 audio_format;
	uint16 num_channels;
	uint32 sample_rate;
	uint32 byte_rate;
	uint16 block_align;
	uint16 bits_per_sample;
};

struct s_wave_data_subchunk {
	uint32 subchunk_2_id;
	uint32 subchunk_2_size;
};

// $TODO add cue-points to wav files: http://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c

static std::string hash_wavetable_parameters(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled);

static bool read_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled,
	c_wrapped_array<real32> out_sample_data);

static bool write_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled,
	c_wrapped_array<const real32> sample_data);

c_sample *c_sample::load_file(const char *filename, e_sample_loop_mode loop_mode, bool phase_shift_enabled) {
	c_sample *sample = new c_sample();

	std::ifstream file(filename);
	if (file.fail()) {
		delete sample;
		return nullptr;
	}

	// $TODO detect format to determine which loading function to call
	if (!load_wave(file, loop_mode, phase_shift_enabled, sample)) {
		delete sample;
		return nullptr;
	}

	return sample;
}

c_sample *c_sample::generate_wavetable(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled) {
	if (harmonic_weights.get_count() == 0) {
		// If no harmonics are provided, that's an error
		return nullptr;
	}

	if (sample_count < 4) {
		return nullptr;
	}

	{
		uint32 sample_count_pow2_check = 1;
		while (sample_count_pow2_check < sample_count) {
			sample_count_pow2_check *= 2;
		}

		if (sample_count_pow2_check != sample_count) {
			return nullptr;
		}
	}

	enum class e_pass {
		k_compute_memory,
		k_generate_data,

		k_count
	};

	c_sample *sample = new c_sample();
	std::vector<real32> entry_sample_data;
	bool loaded_from_cache = false;
	for (e_pass pass : iterate_enum<e_pass>()) {
		uint32 wavetable_entry_count = 0;
		uint32 total_sample_count = 0;
		uint32 previous_entry_sample_count = 0;
		for (uint32 max_frequency = sample_count / 2 - 1; max_frequency > 0; max_frequency--) {
			uint32 wavetable_entry_index = wavetable_entry_count;
			wavetable_entry_count++;

			uint32 entry_sample_count = sample_count;
			// The entry sample count must be more than twice the max frequency
			// entry_sample_count / 2 > max_frequency * 2
			while (entry_sample_count > max_frequency * 4) {
				entry_sample_count /= 2;
			}

			wl_assert(entry_sample_count > 2);

			// Check to see if we actually need new sample data. If the entry sample count is the same as the previous
			// entry and the previous highest harmonic weight is 0 (that is, the single harmonic weight differing
			// between this entry and the previous entry), then the sample data for this entry will be identical to the
			// previous entry's sample data, so we don't need to reserve more space.

			// harmonic_weight for frequency freq == harmonic_weights[freq - 1]
			// first_unused_harmonic_index == (max_frequency - 1) + 1 == max_frequency
			uint32 first_unused_harmonic_index = max_frequency;
			real32 first_unused_harmonic_weight = first_unused_harmonic_index < harmonic_weights.get_count() ?
				harmonic_weights[first_unused_harmonic_index] : 0.0f;

			if (previous_entry_sample_count == entry_sample_count &&
				first_unused_harmonic_weight == 0.0f) {
				// This sample is identical to the previous one, just reuse the data
				wl_assert(wavetable_entry_index > 0);

				if (pass == e_pass::k_generate_data) {
					c_sample &entry_sample = sample->m_wavetable[wavetable_entry_index];
					const c_sample &previous_entry_sample = sample->m_wavetable[wavetable_entry_index - 1];
					entry_sample = previous_entry_sample;
				}
			} else {
				uint32 padded_sample_count = entry_sample_count + k_max_sample_padding * 2;
				if (phase_shift_enabled) {
					// The loop is doubled in size
					padded_sample_count += entry_sample_count;
				}

				if (pass == e_pass::k_generate_data) {
					// Grab a block of samples
					c_wrapped_array<real32> sample_data_destination =
						sample->m_sample_data_allocator.get_array().get_range(total_sample_count, padded_sample_count);

					c_sample &entry_sample = sample->m_wavetable[wavetable_entry_index];
					entry_sample.m_type = e_type::k_single_sample;
					entry_sample.m_sample_rate = entry_sample_count;
					entry_sample.m_base_sample_rate_ratio =
						static_cast<real64>(entry_sample_count) / static_cast<real64>(sample_count);
					entry_sample.m_loop_mode = e_sample_loop_mode::k_loop;
					entry_sample.m_loop_start = 0;
					entry_sample.m_loop_end = entry_sample_count;
					entry_sample.m_phase_shift_enabled = phase_shift_enabled;

					if (!loaded_from_cache) {
						entry_sample_data.resize(entry_sample_count);
						// Add harmonics up to the nyquist limit
						for (uint32 sample_index = 0; sample_index < entry_sample_count; sample_index++) {
							real64 sample_value = 0.0;
							real64 sample_offset_multiplier = 2.0 * k_pi *
								static_cast<real64>(sample_index) / static_cast<real64>(entry_sample_count);

							uint32 harmonic_end_index = std::min(
								cast_integer_verify<uint32>(harmonic_weights.get_count()), first_unused_harmonic_index);
							for (uint32 harmonic_index = 0; harmonic_index < harmonic_end_index; harmonic_index++) {
								real64 period_multiplier = static_cast<real64>(harmonic_index + 1);
								sample_value +=
									harmonic_weights[harmonic_index] *
									std::sin(sample_offset_multiplier * period_multiplier);
							}

							entry_sample_data[sample_index] = static_cast<real32>(sample_value);
						}
					}

					const real32 *entry_sample_data_pointer =
						entry_sample_data.empty() ? nullptr : &entry_sample_data.front();
					sample->m_wavetable[wavetable_entry_index].initialize_data_with_padding(
						1,
						entry_sample_count,
						c_wrapped_array<const real32>(entry_sample_data_pointer, entry_sample_data.size()),
						&sample_data_destination,
						loaded_from_cache);
				}

				total_sample_count += align_size(padded_sample_count, static_cast<uint32>(k_simd_block_elements));
			}

			previous_entry_sample_count = entry_sample_count;
		}

		wl_assert(wavetable_entry_count > 0);

		if (pass == e_pass::k_compute_memory) {
			// Allocate memory to hold all wavetable entries
			sample->m_sample_data_allocator.allocate(total_sample_count);
			sample->m_wavetable.resize(wavetable_entry_count);

			c_wrapped_array<real32> sample_data_array = sample->m_sample_data_allocator.get_array();
			loaded_from_cache = read_wavetable_cache(
				harmonic_weights, sample_count, phase_shift_enabled, sample_data_array);
			if (!loaded_from_cache) {
				std::cout << "Generating wavetable\n";
			}
		} else {
			wl_assert(pass == e_pass::k_generate_data);
			wl_assert(total_sample_count == sample->m_sample_data_allocator.get_array().get_count());

			if (!loaded_from_cache) {
				c_wrapped_array<real32> sample_data_array = sample->m_sample_data_allocator.get_array();
				write_wavetable_cache(harmonic_weights, sample_count, phase_shift_enabled, sample_data_array);
			}
		}
	}

	sample->initialize_wavetable();
	return sample;
}

c_sample::c_sample() {
	m_type = e_type::k_none;
	m_sample_rate = 0;
	m_base_sample_rate_ratio = 0.0;
	m_channel_count = 0;

	m_first_sampling_frame = 0;
	m_sampling_frame_count = 0;
	m_total_frame_count = 0;

	m_loop_mode = e_sample_loop_mode::k_none;
	m_loop_start = 0;
	m_loop_end = 0;
	m_phase_shift_enabled = false;
}

bool c_sample::is_wavetable() const {
	return m_type == e_type::k_wavetable;
}

uint32 c_sample::get_sample_rate() const {
	return m_sample_rate;
}

real64 c_sample::get_base_sample_rate_ratio() const {
	return m_base_sample_rate_ratio;
}

uint32 c_sample::get_channel_count() const {
	return m_channel_count;
}

uint32 c_sample::get_first_sampling_frame() const {
	return m_first_sampling_frame;
}

uint32 c_sample::get_sampling_frame_count() const {
	return m_sampling_frame_count;
}

uint32 c_sample::get_total_frame_count() const {
	return m_total_frame_count;
}

e_sample_loop_mode c_sample::get_loop_mode() const {
	return m_loop_mode;
}

uint32 c_sample::get_loop_start() const {
	return m_loop_start;
}

uint32 c_sample::get_loop_end() const {
	return m_loop_end;
}

bool c_sample::is_phase_shift_enabled() const {
	return m_phase_shift_enabled;
}

c_wrapped_array<const real32> c_sample::get_channel_sample_data(uint32 channel) const {
	wl_assert(VALID_INDEX(channel, m_channel_count));
	return m_sample_data.get_range(m_total_frame_count * channel, m_total_frame_count);
}

uint32 c_sample::get_wavetable_entry_count() const {
	wl_assert(m_type == e_type::k_wavetable);
	return cast_integer_verify<uint32>(m_wavetable.size());
}

const c_sample *c_sample::get_wavetable_entry(uint32 index) const {
	wl_assert(m_type == e_type::k_wavetable);
	return &m_wavetable[index];
}

bool c_sample::load_wave(
	std::ifstream &file, e_sample_loop_mode loop_mode, bool phase_shift_enabled, c_sample *out_sample) {
	s_wave_riff_header riff_header;

	c_binary_file_reader reader(file);

	if (!reader.read_raw(&riff_header, sizeof(riff_header))) {
		return false;
	}

	bool little_endian = true;
	riff_header.chunk_id = big_to_native_endian(riff_header.chunk_id);
	if (riff_header.chunk_id == 'RIFF') {
		// Stay in little endian mode
	} else if (riff_header.chunk_id == 'RIFX') {
		little_endian = false;
	} else {
		return false;
	}

	if (little_endian) {
		riff_header.chunk_size = little_to_native_endian(riff_header.chunk_size);
		riff_header.format = big_to_native_endian(riff_header.format);
	} else {
		riff_header.chunk_size = little_to_native_endian(riff_header.chunk_size);
		riff_header.format = little_to_native_endian(riff_header.format);
	}

	s_wave_fmt_subchunk fmt_subchunk;
	if (!reader.read_raw(&fmt_subchunk, sizeof(fmt_subchunk))) {
		return false;
	}

	if (little_endian) {
		fmt_subchunk.subchunk_1_id = big_to_native_endian(fmt_subchunk.subchunk_1_id);
		fmt_subchunk.subchunk_1_size = little_to_native_endian(fmt_subchunk.subchunk_1_size);
		fmt_subchunk.audio_format = little_to_native_endian(fmt_subchunk.audio_format);
		fmt_subchunk.num_channels = little_to_native_endian(fmt_subchunk.num_channels);
		fmt_subchunk.sample_rate = little_to_native_endian(fmt_subchunk.sample_rate);
		fmt_subchunk.byte_rate = little_to_native_endian(fmt_subchunk.byte_rate);
		fmt_subchunk.block_align = little_to_native_endian(fmt_subchunk.block_align);
		fmt_subchunk.bits_per_sample = little_to_native_endian(fmt_subchunk.bits_per_sample);
	} else {
		fmt_subchunk.subchunk_1_id = big_to_native_endian(fmt_subchunk.subchunk_1_id);
		fmt_subchunk.subchunk_1_size = big_to_native_endian(fmt_subchunk.subchunk_1_size);
		fmt_subchunk.audio_format = big_to_native_endian(fmt_subchunk.audio_format);
		fmt_subchunk.num_channels = big_to_native_endian(fmt_subchunk.num_channels);
		fmt_subchunk.sample_rate = big_to_native_endian(fmt_subchunk.sample_rate);
		fmt_subchunk.byte_rate = big_to_native_endian(fmt_subchunk.byte_rate);
		fmt_subchunk.block_align = big_to_native_endian(fmt_subchunk.block_align);
		fmt_subchunk.bits_per_sample = big_to_native_endian(fmt_subchunk.bits_per_sample);
	}

	if (fmt_subchunk.subchunk_1_id != 'fmt\0' ||
		fmt_subchunk.subchunk_1_size != 16 ||
		fmt_subchunk.audio_format != 1 || // 1 is PCM, other values indicate compression
		fmt_subchunk.num_channels < 1 ||
		fmt_subchunk.sample_rate == 0 ||
		fmt_subchunk.bits_per_sample % 8 != 0) {
		return false;
	}

	uint32 expected_byte_rate = fmt_subchunk.sample_rate * fmt_subchunk.num_channels * fmt_subchunk.bits_per_sample / 8;
	if (fmt_subchunk.byte_rate != expected_byte_rate) {
		return false;
	}

	uint32 expected_block_align = fmt_subchunk.num_channels * fmt_subchunk.bits_per_sample / 8;
	if (fmt_subchunk.block_align != expected_block_align) {
		return false;
	}

	s_wave_data_subchunk data_subchunk;
	if (!reader.read_raw(&data_subchunk, sizeof(data_subchunk))) {
		return false;
	}

	if (little_endian) {
		data_subchunk.subchunk_2_id = big_to_native_endian(data_subchunk.subchunk_2_id);
		data_subchunk.subchunk_2_size = little_to_native_endian(data_subchunk.subchunk_2_size);
	} else {
		data_subchunk.subchunk_2_id = big_to_native_endian(data_subchunk.subchunk_2_id);
		data_subchunk.subchunk_2_size = big_to_native_endian(data_subchunk.subchunk_2_size);
	}

	if (data_subchunk.subchunk_2_id != 'data') {
		return false;
	}

	if (riff_header.chunk_size != 36 + fmt_subchunk.subchunk_1_size + data_subchunk.subchunk_2_size) {
		return false;
	}

	if (data_subchunk.subchunk_2_size % fmt_subchunk.block_align != 0) {
		return false;
	}

	uint32 frames = data_subchunk.subchunk_2_size / fmt_subchunk.block_align;
	if (frames == 0) {
		return false;
	}

	std::vector<uint8> raw_data(data_subchunk.subchunk_2_size);
	file.read(reinterpret_cast<char *>(&raw_data.front()), raw_data.size());
	if (file.fail()) {
		return false;
	}

	// Convert to real format
	std::vector<real32> sample_data(frames * fmt_subchunk.num_channels);
	switch (fmt_subchunk.bits_per_sample) {
	case 8:
		for (size_t frame = 0; frame < frames; frame++) {
			for (uint32 channel = 0; channel < fmt_subchunk.num_channels; channel++) {
				size_t index = frame * fmt_subchunk.num_channels + channel;
				// map [0,255] to [-1,1]
				real32 value = (static_cast<real32>(raw_data[index]) - 127.5f) * (1.0f / 127.5f);
				sample_data[channel * frames + frame] = value;
			}
		}
		break;

	case 16:
		for (size_t frame = 0; frame < frames; frame++) {
			for (uint32 channel = 0; channel < fmt_subchunk.num_channels; channel++) {
				size_t index = frame * fmt_subchunk.num_channels + channel;
				// map [-32768,32767] to [-1,1]
				int16 integer_value = reinterpret_cast<const int16 *>(&raw_data.front())[index];
				real32 value = (static_cast<real32>(integer_value) + 0.5f) * (1.0f / 32767.5f);
				sample_data[channel * frames + frame] = value;
			}
		}
		break;

	default:
		return false;
	}

	// $TODO add loop point data if it exists (make sure to verify that it's valid first)
	out_sample->initialize(
		fmt_subchunk.sample_rate,
		fmt_subchunk.num_channels,
		frames,
		loop_mode,
		0,
		frames,
		phase_shift_enabled,
		c_wrapped_array<const real32>(sample_data.empty() ? nullptr : &sample_data.front(), sample_data.size()));
	return true;
}

void c_sample::initialize(
	uint32 sample_rate,
	uint32 channel_count,
	uint32 frame_count,
	e_sample_loop_mode loop_mode,
	uint32 loop_start,
	uint32 loop_end,
	bool phase_shift_enabled,
	c_wrapped_array<const real32> sample_data) {
	wl_assert(!m_sample_data_allocator.get_array().get_pointer());
	wl_assert(!m_sample_data.get_pointer());
	wl_assert(m_wavetable.empty());

	wl_assert(sample_data.get_count() > 0);

	wl_assert(m_type == e_type::k_none);
	m_type = e_type::k_single_sample;

	wl_assert(sample_rate > 0);
	m_sample_rate = sample_rate;
	m_base_sample_rate_ratio = 1.0;

	wl_assert(valid_enum_index(loop_mode));
	m_loop_mode = loop_mode;
	if (loop_mode == e_sample_loop_mode::k_none) {
		wl_assert(!phase_shift_enabled);
		m_loop_start = 0;
		m_loop_end = 0;
	} else {
		wl_assert(loop_start < loop_end);
		wl_assert(loop_end <= frame_count);
		m_loop_start = loop_start;
		m_loop_end = loop_end;
	}
	m_phase_shift_enabled = phase_shift_enabled;

	initialize_data_with_padding(channel_count, frame_count, sample_data, nullptr, false);
}

void c_sample::initialize_wavetable() {
	wl_assert(!m_sample_data.get_pointer());
	wl_assert(!m_wavetable.empty());

	wl_assert(m_type == e_type::k_none);
	m_type = e_type::k_wavetable;

#if IS_TRUE(ASSERTS_ENABLED)
	for (uint32 index = 1; index < m_wavetable.size(); index++) {
		// Verify that the mipmap is valid
		const c_sample &sample_a = m_wavetable[index - 1];
		const c_sample &sample_b = m_wavetable[index];

		wl_assert(sample_a.m_type == e_type::k_single_sample);
		wl_assert(sample_b.m_type == e_type::k_single_sample);
		wl_assert(sample_a.m_sample_rate == sample_b.m_sample_rate * 2 ||
			sample_a.m_sample_rate == sample_b.m_sample_rate);
		uint32 sample_rate_ratio = sample_a.m_sample_rate / sample_b.m_sample_rate;
		wl_assert(sample_a.m_channel_count == sample_b.m_channel_count);
		wl_assert(sample_a.m_sampling_frame_count == sample_b.m_sampling_frame_count * sample_rate_ratio);
		wl_assert(sample_a.m_loop_mode == sample_b.m_loop_mode);
		wl_assert(sample_a.m_loop_start - sample_a.m_first_sampling_frame ==
			(sample_b.m_loop_start - sample_b.m_first_sampling_frame) * sample_rate_ratio);
		wl_assert(sample_a.m_loop_end - sample_a.m_first_sampling_frame ==
			(sample_b.m_loop_end - sample_b.m_first_sampling_frame) * sample_rate_ratio);
		wl_assert(sample_a.m_phase_shift_enabled == sample_b.m_phase_shift_enabled);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	const c_sample &first_sample = m_wavetable.front();
	m_sample_rate = first_sample.m_sample_rate;
	m_base_sample_rate_ratio = first_sample.m_base_sample_rate_ratio;
	m_channel_count = first_sample.m_channel_count;

	m_first_sampling_frame = first_sample.m_first_sampling_frame;
	m_sampling_frame_count = first_sample.m_sampling_frame_count;
	m_total_frame_count = first_sample.m_total_frame_count;

	m_loop_mode = first_sample.m_loop_mode;
	m_loop_start = first_sample.m_loop_start;
	m_loop_end = first_sample.m_loop_end;
	m_phase_shift_enabled = first_sample.m_phase_shift_enabled;
}

void c_sample::initialize_data_with_padding(
	uint32 channel_count,
	uint32 frame_count,
	c_wrapped_array<const real32> sample_data,
	c_wrapped_array<real32> *destination,
	bool initialize_metadata_only) {
	// Add zero padding to the beginning of the sound
	m_first_sampling_frame = k_max_sample_padding;
	uint32 loop_frame_count = 0;

	switch (m_loop_mode) {
	case e_sample_loop_mode::k_none:
		// With no looping, the sampling frame count is the same as the provided frame count
		// Key: D = sample data, 0 = zero padding
		// Orig:      [DDDDDDDD]
		// Final: [00][DDDDDDDD][00]
		m_sampling_frame_count = frame_count;
		break;

	case e_sample_loop_mode::k_loop:
		// With regular looping, we sample up to the end of the loop, and then we duplicate some samples for padding. We
		// do this because if we're sampling using a window size of N, then the first time we loop, we actually want our
		// window to touch the pre-loop samples. However, once we've looped enough (usually just once if the loop is
		// long enough) we want our window to "wrap" at the edges. In other words, we want both endpoints of our loop to
		// appear to be identical for the maximum window size, so we shift the loop points forward in time.
		// Key: I = intro samples, L = loop samples, T = intro-to-loop-transition samples (beginning of the loop),
		// P = loop padding, E = edge padding
		// Orig:      [IIII][LLLLLLLLLLLLLLLL]
		// Final: [00][IIII][TT][LLLLLLLLLLLL][PP][EE]

		// If the loop start point is 0 (i.e. there is no "intro"), then the above doesn't apply and we don't need to
		// apply any loop padding. This is because with no intro, there is no "intro boundary" to sample differently
		// from the first time we enter the loop region.
		// Orig:      [LLLLLLLL]
		// Final: [PP][LLLLLLLL][PP]

		m_sampling_frame_count = m_loop_end;
		if (m_loop_start > 0) {
			m_sampling_frame_count += k_max_sample_padding;
		}
		loop_frame_count = m_loop_end - m_loop_start;
		break;

	case e_sample_loop_mode::k_bidi_loop:
		// Same logic as above, but the actual looping portion of the sound is duplicated and reversed:
		// [01234567] -> [01234567654321]
		m_sampling_frame_count = m_loop_end;
		if (m_loop_start > 0) {
			m_sampling_frame_count += k_max_sample_padding;
		}
		loop_frame_count = m_loop_end - m_loop_start;
		if (m_loop_end - m_loop_start > 2) {
			// Don't duplicate the first and last samples
			uint32 reverse_frame_count = m_loop_end - m_loop_start - 2;
			m_sampling_frame_count += reverse_frame_count;
			loop_frame_count += reverse_frame_count;
		}
		break;

	default:
		m_sampling_frame_count = 0;
		wl_unreachable();
	}

	// Add padding to the beginning and end
	// Round up to account for SSE padding
	m_total_frame_count = align_size(
		m_sampling_frame_count + (2 * k_max_sample_padding),
		static_cast<uint32>(k_simd_block_elements));
	if (m_phase_shift_enabled) {
		m_total_frame_count += loop_frame_count;
	}

	wl_assert(initialize_metadata_only || channel_count * frame_count == sample_data.get_count());
	m_channel_count = channel_count;

	c_wrapped_array<real32> sample_data_array;
	if (!destination) {
		m_sample_data_allocator.allocate(m_total_frame_count * m_channel_count);
		sample_data_array = m_sample_data_allocator.get_array();
	} else {
		sample_data_array = *destination;
		wl_assert(sample_data_array.get_count() == m_total_frame_count * m_channel_count);
	}

	m_sample_data = sample_data_array;

	if (!initialize_metadata_only) {
		for (uint32 channel = 0; channel < m_channel_count; channel++) {
			uint32 src_offset = frame_count * channel;
			uint32 dst_offset = m_total_frame_count * channel;

			switch (m_loop_mode) {
			case e_sample_loop_mode::k_none:
				// Pad beginning with 0
				for (uint32 index = 0; index < k_max_sample_padding; index++) {
					sample_data_array[dst_offset] = 0.0f;
					dst_offset++;
				}

				// Copy samples directly
				for (uint32 index = 0; index < frame_count; index++) {
					sample_data_array[dst_offset] = sample_data[src_offset + index];
					dst_offset++;
				}

				// Pad end with 0
				for (uint32 index = 0; index < k_max_sample_padding; index++) {
					sample_data_array[dst_offset] = 0.0f;
					dst_offset++;
				}
				break;

			case e_sample_loop_mode::k_loop:
			case e_sample_loop_mode::k_bidi_loop:
			{
				// Pad beginning with 0 (if our loop start point is 0, we will later overwrite this with loop samples)
				for (uint32 index = 0; index < k_max_sample_padding; index++) {
					sample_data_array[dst_offset] = 0.0f;
					dst_offset++;
				}

				// Copy samples directly
				for (uint32 index = 0; index < m_loop_end; index++) {
					sample_data_array[dst_offset] = sample_data[src_offset + index];
					dst_offset++;
				}

				uint32 loop_mod = m_loop_end - m_loop_start;
				if (m_loop_mode == e_sample_loop_mode::k_bidi_loop) {
					// Copy reversed loop
					uint32 loop_offset = dst_offset - loop_mod;
					for (uint32 index = 1; index + 1 < loop_mod; index++) {
						uint32 reverse_index = loop_mod - index - 1;
						sample_data_array[dst_offset] = sample_data_array[loop_offset + reverse_index];
						dst_offset++;
					}

					if (loop_mod > 2) {
						loop_mod += (loop_mod - 2);
					}
				}

				wl_assert(loop_mod == loop_frame_count);

				if (m_loop_start == 0) {
					// No loop padding necessary, just apply beginning/end padding by copying from the loop

					// Pad the beginning by copying loop samples
					for (uint32 index = 0; index < k_max_sample_padding; index++) {
						uint32 rev_index = k_max_sample_padding - index - 1;
						sample_data_array[rev_index] = sample_data_array[rev_index + loop_mod];
					}

					// Pad the end by copying loop samples
					for (uint32 index = 0; index < k_max_sample_padding; index++) {
						sample_data_array[dst_offset] = sample_data_array[dst_offset - loop_mod];
						dst_offset++;
					}
				} else {
					// Extend the loop by both loop padding and edge padding
					uint32 loop_extend = k_max_sample_padding * 2;
					for (uint32 index = 0; index < loop_extend; index++) {
						sample_data_array[dst_offset] = sample_data_array[dst_offset - loop_mod];
						dst_offset++;
					}

					// Finally, adjust loop points forward by the loop padding
					m_loop_start += k_max_sample_padding;
					m_loop_end += k_max_sample_padding;
				}

				if (m_phase_shift_enabled) {
					// Double the loop so we can phase-shift without overrunning bounds
					for (uint32 index = 0; index < loop_mod; index++) {
						sample_data_array[dst_offset] = sample_data_array[dst_offset - loop_mod];
						dst_offset++;
					}
				}

				break;
			}

			default:
				wl_unreachable();
			}

			// Pad with 0 for SSE alignment
			uint32 simd_padding_count = align_size(dst_offset, static_cast<uint32>(k_simd_block_elements)) - dst_offset;
			for (uint32 index = 0; index < simd_padding_count; index++) {
				sample_data_array[dst_offset] = 0.0f;
				dst_offset++;
			}

			wl_assert(dst_offset == (channel + 1) * m_total_frame_count);
		}
	}

	// Offset the loop points by the initial padding
	m_loop_start += m_first_sampling_frame;
	m_loop_end += m_first_sampling_frame;
}

static std::string hash_wavetable_parameters(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled) {
	CSHA1 hash;
	uint32 harmonic_weights_count = cast_integer_verify<uint32>(harmonic_weights.get_count());
	hash.Update(
		reinterpret_cast<const uint8 *>(&harmonic_weights_count),
		sizeof(harmonic_weights_count));
	hash.Update(
		reinterpret_cast<const uint8 *>(harmonic_weights.get_pointer()),
		cast_integer_verify<uint32>(sizeof(harmonic_weights[0]) * harmonic_weights.get_count()));
	hash.Update(
		reinterpret_cast<const uint8 *>(&phase_shift_enabled),
		sizeof(phase_shift_enabled));
	hash.Final();

	std::string hash_string;
	hash.ReportHashStl(hash_string, CSHA1::REPORT_HEX_SHORT);
	return hash_string;
}

static bool read_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled,
	c_wrapped_array<real32> out_sample_data) {
	std::string hash_string = hash_wavetable_parameters(harmonic_weights, sample_count, phase_shift_enabled);
	std::string wavetable_cache_filename = k_wavetable_cache_folder;
	wavetable_cache_filename += '/';
	wavetable_cache_filename += hash_string;
	wavetable_cache_filename += k_wavetable_cache_extension;

	bool result = true;

	std::ifstream file(wavetable_cache_filename, std::ios::binary);
	if (!file.is_open()) {
		result = false;
	}

	if (result) {
		uint32 file_identifier;
		file.read(reinterpret_cast<char *>(&file_identifier), sizeof(file_identifier));
		if (file.fail() || memcmp(&file_identifier, k_wavetable_cache_identifier, sizeof(file_identifier)) != 0) {
			result = false;
		}
	}

	if (result) {
		uint32 file_version;
		file.read(reinterpret_cast<char *>(&file_version), sizeof(file_version));
		if (file.fail() || file_version != k_wavetable_cache_version) {
			result = false;
		}
	}

	uint32 file_harmonic_weights_count;
	uint32 file_sample_count;
	uint32 file_phase_shift_enabled;

	if (result) {
		file.read(reinterpret_cast<char *>(&file_harmonic_weights_count), sizeof(file_harmonic_weights_count));
		if (file.fail() || file_harmonic_weights_count != harmonic_weights.get_count()) {
			result = false;
		}
	}

	if (result) {
		if (file_harmonic_weights_count > 0) {
			std::vector<real32> file_harmonic_weights(file_harmonic_weights_count);
			size_t harmonic_weights_size = sizeof(file_harmonic_weights[0]) * file_harmonic_weights.size();
			file.read(reinterpret_cast<char *>(&file_harmonic_weights.front()), harmonic_weights_size);
			if (file.fail() ||
				memcmp(&file_harmonic_weights.front(), harmonic_weights.get_pointer(), harmonic_weights_size) != 0) {
				result = false;
			}
		}
	}

	if (result) {
		file.read(reinterpret_cast<char *>(&file_sample_count), sizeof(file_sample_count));
		if (file.fail() || file_sample_count != sample_count) {
			result = false;
		}
	}

	if (result) {
		file.read(reinterpret_cast<char *>(&file_phase_shift_enabled), sizeof(file_phase_shift_enabled));
		if (file.fail() || file_phase_shift_enabled != static_cast<uint32>(phase_shift_enabled)) {
			result = false;
		}
	}

	if (result) {
		// Read sample data if everything has matched so far
		file.read(
			reinterpret_cast<char *>(out_sample_data.get_pointer()),
			sizeof(out_sample_data[0]) * out_sample_data.get_count());
		if (file.fail()) {
			result = false;
		}
	}

	if (result) {
		std::cout << "Loaded wavetable '" << wavetable_cache_filename << "'\n";
	} else {
		std::cout << "Failed to load wavetable '" << wavetable_cache_filename << "'\n";
	}
	return result;
}

static bool write_wavetable_cache(
	c_wrapped_array<const real32> harmonic_weights,
	uint32 sample_count,
	bool phase_shift_enabled,
	c_wrapped_array<const real32> sample_data) {
	std::string hash_string = hash_wavetable_parameters(harmonic_weights, sample_count, phase_shift_enabled);
	std::string wavetable_cache_filename = k_wavetable_cache_folder;
	wavetable_cache_filename += '/';
	wavetable_cache_filename += hash_string;
	wavetable_cache_filename += k_wavetable_cache_extension;

	create_directory(k_wavetable_cache_folder);

	std::ofstream file(wavetable_cache_filename, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	file.write(reinterpret_cast<const char *>(k_wavetable_cache_identifier), sizeof(k_wavetable_cache_identifier));
	file.write(reinterpret_cast<const char *>(&k_wavetable_cache_version), sizeof(k_wavetable_cache_version));

	uint32 file_harmonic_weights_count = cast_integer_verify<uint32>(harmonic_weights.get_count());
	uint32 file_phase_shift_enabled = phase_shift_enabled ? 1 : 0;

	file.write(reinterpret_cast<const char *>(&file_harmonic_weights_count), sizeof(file_harmonic_weights_count));

	size_t harmonic_weights_size = sizeof(harmonic_weights[0]) * harmonic_weights.get_count();
	file.write(reinterpret_cast<const char *>(harmonic_weights.get_pointer()), harmonic_weights_size);
	file.write(reinterpret_cast<const char *>(&sample_count), sizeof(sample_count));
	file.write(reinterpret_cast<const char *>(&file_phase_shift_enabled), sizeof(file_phase_shift_enabled));

	// Write sample data
	file.write(
		reinterpret_cast<const char *>(sample_data.get_pointer()),
		sizeof(sample_data[0]) * sample_data.get_count());

	if (file.fail()) {
		std::cout << "Failed to save wavetable '" << wavetable_cache_filename << "'\n";
	} else {
		std::cout << "Saved wavetable '" << wavetable_cache_filename << "'\n";
	}

	return !file.fail();
}
