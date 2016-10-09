#ifndef WAVELANG_FILE_UTILITY_H__
#define WAVELANG_FILE_UTILITY_H__

#include "common/common.h"

#include <fstream>

bool are_file_paths_equivalent(const char *path_a, const char *path_b);
bool is_path_relative(const char *path);
bool get_file_last_modified_timestamp(const char *path, uint64 &out_timestamp);

template<typename t_value> void write(std::ofstream &out, t_value value) {
	out.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

template<typename t_value> bool read(std::ifstream &in, t_value &value) {
	in.read(reinterpret_cast<char *>(&value), sizeof(value));
	return !in.fail();
}

#endif // WAVELANG_FILE_UTILITY_H__