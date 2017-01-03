#include "common/utility/file_utility.h"

#include "engine/math/simd.h"
#include "engine/sample/sample.h"

#include <fstream>

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

static bool load_wave(std::ifstream &file, e_sample_loop_mode loop_mode, bool phase_shift_enabled,
	c_sample *out_sample);

c_sample *c_sample::load(const char *filename, e_sample_loop_mode loop_mode, bool phase_shift_enabled) {
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

static bool load_wave(std::ifstream &file, e_sample_loop_mode loop_mode, bool phase_shift_enabled,
	c_sample *out_sample) {
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
	out_sample->initialize(fmt_subchunk.sample_rate, fmt_subchunk.num_channels, frames,
		loop_mode, 0, frames, phase_shift_enabled,
		c_wrapped_array_const<real32>(sample_data.empty() ? nullptr : &sample_data.front(), sample_data.size()));
	return true;
}

c_sample::c_sample() {
	m_type = k_type_none;
	m_sample_rate = 0;
	m_channel_count = 0;

	m_first_sampling_frame = 0;
	m_sampling_frame_count = 0;
	m_total_frame_count = 0;

	m_loop_mode = k_sample_loop_mode_none;
	m_loop_start = 0;
	m_loop_end = 0;
}

c_sample::~c_sample() {
	if (m_type == k_type_mipmap) {
		for (uint32 index = 0; index < m_mipmap.size(); index++) {
			delete m_mipmap[index];
		}
	}
}

void c_sample::initialize(uint32 sample_rate, uint32 channel_count, uint32 frame_count,
	e_sample_loop_mode loop_mode, uint32 loop_start, uint32 loop_end, bool phase_shift_enabled,
	c_wrapped_array_const<real32> sample_data) {
	wl_assert(!m_sample_data.get_array().get_pointer());
	wl_assert(m_mipmap.empty());

	wl_assert(sample_data.get_count() > 0);

	wl_assert(m_type == k_type_none);
	m_type = k_type_single_sample;

	wl_assert(sample_rate > 0);
	m_sample_rate = sample_rate;

	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));
	m_loop_mode = loop_mode;
	if (loop_mode == k_sample_loop_mode_none) {
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

	initialize_data_with_padding(channel_count, frame_count, sample_data, k_max_sample_padding, k_max_sample_padding);
}

void c_sample::initialize_for_mipmap(uint32 sample_rate, uint32 channel_count, uint32 frame_count,
	e_sample_loop_mode loop_mode, uint32 loop_start, uint32 loop_end, bool phase_shift_enabled,
	c_wrapped_array_const<real32> sample_data) {
	wl_assert(!m_sample_data.get_array().get_pointer());
	wl_assert(m_mipmap.empty());

	wl_assert(sample_data.get_count() > 0);

	wl_assert(m_type == k_type_none);
	m_type = k_type_uninitialized_mipmap_sample;

	wl_assert(sample_rate > 0);
	wl_assert(channel_count * frame_count == sample_data.get_count());
	m_sample_rate = sample_rate;
	m_channel_count = channel_count;

	m_first_sampling_frame = 0;
	m_sampling_frame_count = frame_count;
	m_total_frame_count = frame_count;

	wl_assert(VALID_INDEX(loop_mode, k_sample_loop_mode_count));
	m_loop_mode = loop_mode;
	if (loop_mode == k_sample_loop_mode_none) {
		wl_assert(!phase_shift_enabled);
		m_loop_start = 0;
		m_loop_end = 0;
	}
	else {
		wl_assert(loop_start < loop_end);
		wl_assert(loop_end <= frame_count);
		m_loop_start = loop_start;
		m_loop_end = loop_end;
	}
	m_phase_shift_enabled = phase_shift_enabled;

	// Directly copy the data - do not modify it yet
	m_sample_data.allocate(sample_data.get_count());
	memcpy(m_sample_data.get_array().get_pointer(), sample_data.get_pointer(),
		sample_data.get_count() * sizeof(real32));
}

void c_sample::initialize(c_wrapped_array_const<c_sample *> mipmap) {
	wl_assert(!m_sample_data.get_array().get_pointer());
	wl_assert(m_mipmap.empty());
	wl_assert(m_mipmap.size() <= k_max_sample_mipmap_levels);

	wl_assert(m_type == k_type_none);
	m_type = k_type_mipmap;

	wl_assert(mipmap.get_count() > 0);

#if IS_TRUE(ASSERTS_ENABLED)
	for (uint32 index = 1; index < mipmap.get_count(); index++) {
		// Verify that the mipmap is valid
		c_sample *sample_a = mipmap[index - 1];
		c_sample *sample_b = mipmap[index];

		wl_assert(sample_a->m_type == k_type_uninitialized_mipmap_sample);
		wl_assert(sample_b->m_type == k_type_uninitialized_mipmap_sample);
		wl_assert(sample_a->m_sample_rate == sample_b->m_sample_rate * 2);
		wl_assert(sample_a->m_channel_count == sample_b->m_channel_count);
		wl_assert(sample_a->m_sampling_frame_count == sample_b->m_sampling_frame_count * 2);
		wl_assert(sample_a->m_loop_start == sample_b->m_loop_start * 2);
		wl_assert(sample_a->m_loop_end == sample_b->m_loop_end * 2);
		wl_assert(sample_a->m_phase_shift_enabled == sample_b->m_phase_shift_enabled);
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	m_sample_rate = 0;
	m_channel_count = 0;

	m_first_sampling_frame = 0;
	m_sampling_frame_count = 0;
	m_total_frame_count = 0;

	m_loop_mode = mipmap[0]->m_loop_mode;
	m_loop_start = 0;
	m_loop_end = 0;
	m_phase_shift_enabled = mipmap[0]->m_phase_shift_enabled;

	uint32 loop_padding = k_max_sample_padding;
	m_mipmap.resize(mipmap.get_count());
	for (size_t index = 0; index < mipmap.get_count(); index++) {
		size_t reverse_index = mipmap.get_count() - index - 1;
		c_sample *mip_sample = mipmap[reverse_index];
		m_mipmap[reverse_index] = mip_sample;

		// Swap out the current data with an empty vector
		c_aligned_allocator<real32, k_simd_alignment> m_raw_sample_data;
		m_raw_sample_data.swap(mip_sample->m_sample_data);

		c_wrapped_array_const<real32> sample_data = m_raw_sample_data.get_array();
		mip_sample->initialize_data_with_padding(
			mip_sample->m_channel_count, mip_sample->m_total_frame_count,
			sample_data, k_max_sample_padding, loop_padding);

		// This sample has been initialized
		mip_sample->m_type = k_type_single_sample;

		// In order to line up, each level must use twice the padding as the level higher. The edge padding can stay the
		// same though.
		loop_padding *= 2;
	}
}

bool c_sample::is_mipmap() const {
	return m_type == k_type_mipmap;
}

uint32 c_sample::get_sample_rate() const {
	wl_assert(m_type == k_type_single_sample);
	return m_sample_rate;
}

uint32 c_sample::get_channel_count() const {
	wl_assert(m_type == k_type_single_sample);
	return m_channel_count;
}

uint32 c_sample::get_first_sampling_frame() const {
	wl_assert(m_type == k_type_single_sample);
	return m_first_sampling_frame;
}

uint32 c_sample::get_sampling_frame_count() const {
	wl_assert(m_type == k_type_single_sample);
	return m_sampling_frame_count;
}

uint32 c_sample::get_total_frame_count() const {
	wl_assert(m_type == k_type_single_sample);
	return m_total_frame_count;
}

e_sample_loop_mode c_sample::get_loop_mode() const {
	return m_loop_mode;
}

uint32 c_sample::get_loop_start() const {
	wl_assert(m_type == k_type_single_sample);
	return m_loop_start;
}

uint32 c_sample::get_loop_end() const {
	wl_assert(m_type == k_type_single_sample);
	return m_loop_end;
}

bool c_sample::is_phase_shift_enabled() const {
	return m_phase_shift_enabled;
}

c_wrapped_array_const<real32> c_sample::get_channel_sample_data(uint32 channel) const {
	wl_assert(VALID_INDEX(channel, m_channel_count));
	return c_wrapped_array_const<real32>(
		&m_sample_data.get_array()[m_total_frame_count * channel], m_total_frame_count);
}

uint32 c_sample::get_mipmap_count() const {
	wl_assert(m_type == k_type_mipmap);
	return cast_integer_verify<uint32>(m_mipmap.size());
}

const c_sample *c_sample::get_mipmap_sample(uint32 index) const {
	wl_assert(m_type == k_type_mipmap);
	return m_mipmap[index];
}

void c_sample::initialize_data_with_padding(uint32 channel_count, uint32 frame_count,
	c_wrapped_array_const<real32> sample_data, uint32 edge_padding, uint32 loop_padding) {
	// Add zero padding to the beginning of the sound
	m_first_sampling_frame = edge_padding;
	uint32 loop_frame_count = 0;

	switch (m_loop_mode) {
	case k_sample_loop_mode_none:
		// With no looping, the sampling frame count is the same as the provided frame count
		m_sampling_frame_count = frame_count;
		break;

	case k_sample_loop_mode_loop:
		// With regular looping, we sample up to the end of the loop, and then we duplicate some samples for padding. We
		// do this because if we're sampling using a window size of N, then the first time we loop, we actually want our
		// window to touch the pre-loop samples. However, once we've looped enough (usually just once if the loop is
		// long enough) we want our window to "wrap" at the edges. In other words, we want both endpoints of our loop to
		// appear to be identical for the maximum window size.
		m_sampling_frame_count = m_loop_end + loop_padding;
		loop_frame_count = m_loop_end - m_loop_start;
		break;

	case k_sample_loop_mode_bidi_loop:
		// Same logic as above, but the actual looping portion of the sound is duplicated and reversed
		m_sampling_frame_count = m_loop_end + loop_padding;
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
	m_total_frame_count = align_size(m_sampling_frame_count + (2 * edge_padding),
		static_cast<uint32>(k_simd_block_elements));
	if (m_phase_shift_enabled) {
		m_total_frame_count += loop_frame_count;
	}

	wl_assert(channel_count * frame_count == sample_data.get_count());
	m_channel_count = channel_count;

	m_sample_data.allocate(m_total_frame_count * m_channel_count);
	c_wrapped_array<real32> sample_data_array = m_sample_data.get_array();

	for (uint32 channel = 0; channel < m_channel_count; channel++) {
		uint32 src_offset = frame_count * channel;
		uint32 dst_offset = m_total_frame_count * channel;

		// Pad beginning with 0
		for (uint32 index = 0; index < edge_padding; index++) {
			sample_data_array[dst_offset] = 0.0f;
			dst_offset++;
		}

		switch (m_loop_mode) {
		case k_sample_loop_mode_none:
			// Copy samples directly
			for (uint32 index = 0; index < frame_count; index++) {
				sample_data_array[dst_offset] = sample_data[src_offset + index];
				dst_offset++;
			}

			// Pad end with 0
			for (uint32 index = 0; index < edge_padding; index++) {
				sample_data_array[dst_offset] = 0.0f;
				dst_offset++;
			}
			break;

		case k_sample_loop_mode_loop:
		case k_sample_loop_mode_bidi_loop:
		{
			// Copy samples directly
			for (uint32 index = 0; index < m_loop_end; index++) {
				sample_data_array[dst_offset] = sample_data[src_offset + index];
				dst_offset++;
			}

			uint32 loop_mod = m_loop_end - m_loop_start;
			if (m_loop_mode == k_sample_loop_mode_bidi_loop) {
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

			// Extend the loop by both loop padding and edge padding
			uint32 loop_extend = loop_padding + edge_padding;
			if (m_phase_shift_enabled) {
				// Double the loop so we can phase-shift without overrunning bounds
				loop_extend += loop_mod;
			}

			for (uint32 index = 0; index < loop_extend; index++) {
				sample_data_array[dst_offset] = sample_data_array[dst_offset - loop_mod];
				dst_offset++;
			}


			// Finally, adjust loop points
			m_loop_start += loop_padding;
			m_loop_end += loop_padding;
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

	// Offset the loop points by the initial padding
	m_loop_start += m_first_sampling_frame;
	m_loop_end += m_first_sampling_frame;
}
