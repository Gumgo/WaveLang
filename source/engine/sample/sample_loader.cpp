#include "common/utility/file_utility.h"

#include "engine/sample/sample_loader.h"

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

static bool load_wav(const char *filename, s_loaded_sample &out_loaded_sample) {
	std::ifstream file(filename, std::ios::binary);
	if (file.fail()) {
		return false;
	}

	c_binary_file_reader reader(file);

	s_wave_riff_header riff_header;
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

	if (fmt_subchunk.subchunk_1_id != 'fmt '
		|| fmt_subchunk.subchunk_1_size != 16
		|| fmt_subchunk.audio_format != 1 // 1 is PCM, other values indicate compression
		|| fmt_subchunk.num_channels < 1
		|| fmt_subchunk.sample_rate == 0
		|| fmt_subchunk.bits_per_sample % 8 != 0) {
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

	// The additional 20 comes from the remaining fields in the riff header and the ID fields in other chunk headers
	if (riff_header.chunk_size != 20 + fmt_subchunk.subchunk_1_size + data_subchunk.subchunk_2_size) {
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

	out_loaded_sample.sample_rate = fmt_subchunk.sample_rate;
	out_loaded_sample.frame_count = frames;
	out_loaded_sample.channel_count = fmt_subchunk.num_channels;
	// $TODO add loop point data if it exists (make sure to verify that it's valid first)
	out_loaded_sample.loop_start_sample_index = 0;
	out_loaded_sample.loop_end_sample_index = frames;

	// Convert to real format
	// $TODO support more formats, namely float and int24
	out_loaded_sample.samples.resize(frames * fmt_subchunk.num_channels);
	switch (fmt_subchunk.bits_per_sample) {
	case 8:
		for (size_t frame = 0; frame < frames; frame++) {
			for (uint32 channel = 0; channel < fmt_subchunk.num_channels; channel++) {
				size_t index = frame * fmt_subchunk.num_channels + channel;
				// map [0,255] to [-1,1]
				real32 value = (static_cast<real32>(raw_data[index]) - 127.5f) * (1.0f / 127.5f);
				out_loaded_sample.samples[channel * frames + frame] = value;
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
				out_loaded_sample.samples[channel * frames + frame] = value;
			}
		}
		break;

	default:
		return false;
	}

	return true;
}

bool load_sample(const char *filename, s_loaded_sample &out_loaded_sample) {
	// $TODO detect format to determine which loading function to call
	return load_wav(filename, out_loaded_sample);
}
