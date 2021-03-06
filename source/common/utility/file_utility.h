#pragma once

#include "common/common.h"

#include <fstream>
#include <vector>

#if IS_TRUE(PLATFORM_WINDOWS)
constexpr char k_path_separator = '\\';
constexpr char k_alt_path_separator = '/';
#else // IS_TRUE(PLATFORM_WINDOWS)
constexpr char k_path_separator = '/';
constexpr char k_alt_path_separator = '/'; // No alternative on other platforms
#endif // IS_TRUE(PLATFORM_WINDOWS)

bool are_file_paths_equivalent(const char *path_a, const char *path_b);
std::string canonicalize_path(const char *path);
std::string get_path_directory(const char *path);
bool is_path_relative(const char *path);
bool get_file_last_modified_timestamp(const char *path, uint64 &timestamp_out);
void create_directory(const char *path); // $TODO use std::filesystem instead

enum class e_read_full_file_result {
	k_success,
	k_failed_to_open,
	k_failed_to_read,
	k_file_too_big
};

e_read_full_file_result read_full_file(const char *path, std::vector<char> &file_contents_out);

class c_binary_file_reader {
public:
	c_binary_file_reader(std::ifstream &file)
		: m_file(file) {}

	// Performs endian byte swapping
	template<typename t_value> bool read(t_value &value_out) {
		t_value raw_value;
		m_file.read(reinterpret_cast<char *>(&raw_value), sizeof(raw_value));

		if (m_file.fail()) {
			return false;
		}

		value_out = big_to_native_endian(raw_value);
		return true;
	}

	bool read(bool &value_out) {
		uint8 raw_value;
		m_file.read(reinterpret_cast<char *>(&raw_value), sizeof(raw_value));

		if (m_file.fail() || raw_value > 1) {
			return false;
		}

		value_out = (raw_value != 0);
		return true;
	}

	bool read_raw(void *buffer, size_t count) {
		m_file.read(reinterpret_cast<char *>(buffer), count);
		return !m_file.fail();
	}

private:
	std::ifstream &m_file;
};

class c_binary_file_writer {
public:
	c_binary_file_writer(std::ofstream &file)
		: m_file(file) {}

	// Performs endian byte swapping
	template<typename t_value> void write(t_value value) {
		t_value raw_value = native_to_big_endian(value);
		m_file.write(reinterpret_cast<const char *>(&raw_value), sizeof(raw_value));
	}

	void write(bool value) {
		uint8 raw_value = value ? 1 : 0;
		m_file.write(reinterpret_cast<const char *>(&raw_value), sizeof(raw_value));
	}

	void write_raw(const void *buffer, size_t count) {
		m_file.write(reinterpret_cast<const char *>(buffer), count);
	}

private:
	std::ofstream &m_file;
};

