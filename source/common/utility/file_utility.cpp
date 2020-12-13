#include "common/utility/file_utility.h"

#if IS_TRUE(PLATFORM_WINDOWS)
#include <Shlwapi.h>
#else // IS_TRUE(PLATFORM_WINDOWS)
#include <sys/stat.h>
#include <sys/types.h>
#endif // IS_TRUE(PLATFORM_WINDOWS)

bool are_file_paths_equivalent(const char *path_a, const char *path_b) {
	if (strcmp(path_a, path_b) == 0) {
		return true;
	}

#if IS_TRUE(PLATFORM_WINDOWS)
	{
		bool result = false;
		s_static_array<HANDLE, 2> file_handles;

		for (size_t i = 0; i < 2; i++) {
			file_handles[i] = CreateFileA(
				i == 0 ? path_a : path_b,
				0,
				FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}

		if (file_handles[0] != INVALID_HANDLE_VALUE
			&& file_handles[1] != INVALID_HANDLE_VALUE) {
			BY_HANDLE_FILE_INFORMATION file_a_info;
			BY_HANDLE_FILE_INFORMATION file_b_info;
			if (GetFileInformationByHandle(file_handles[0], &file_a_info)
				&& GetFileInformationByHandle(file_handles[1], &file_b_info)) {
				result = (file_a_info.dwVolumeSerialNumber == file_b_info.dwVolumeSerialNumber)
					&& (file_a_info.nFileIndexHigh == file_b_info.nFileIndexHigh)
					&& (file_a_info.nFileIndexLow == file_b_info.nFileIndexLow);
			}
		}

		for (size_t i = 0; i < 2; i++) {
			if (file_handles[i] != INVALID_HANDLE_VALUE) {
				CloseHandle(file_handles[i]);
			}
		}

		return result;
	}
#else // IS_TRUE(PLATFORM_WINDOWS)
	{
		struct stat stat_buffer_a;
		struct stat stat_buffer_b;
		int32 result_a = stat(path_a, &stat_buffer_a);
		int32 result_b = stat(path_b, &stat_buffer_b);

		if (result_a != 0 || result_b != 0) {
			// In the event of any error, assume the files are different
			return false;
		}

		return stat_buffer_a.st_ino == stat_buffer_b.st_ino;
	}
#endif // IS_TRUE(PLATFORM_WINDOWS)
}

std::string canonicalize_path(const char *path) {
	std::string canonicalized_path;

#if IS_TRUE(PLATFORM_WINDOWS)
	HANDLE file_handle = CreateFileA(
		path,
		0,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (file_handle != INVALID_HANDLE_VALUE) {
		// Note: the final parameter can be either FILE_NAME_NORMALIZED or FILE_NAME_OPENED. I'm not sure what the
		// difference is exactly, but FILE_NAME_NORMALIZED can fail in more cases (when permissions are lacking). Since
		// this function is used for display purposes only, I'm going with FILE_NAME_OPENED to avoid failures.
		char buffer[MAX_PATH];
		DWORD result = GetFinalPathNameByHandleA(file_handle, buffer, sizeof(buffer), FILE_NAME_OPENED);
		CloseHandle(file_handle);

		// On success, the return value is the length of the resulting string, not including the null terminator
		if (result < MAX_PATH) {
			// Remove prefixes
			std::string canonicalized_path(buffer);
			if (canonicalized_path.substr(0, 8).compare("\\\\?\\UNC\\") == 0) {
				// Network path, replace "\\?\UNC\" with just "\\"
				canonicalized_path = "\\" + canonicalized_path.substr(7);
			} else if (canonicalized_path.substr(0, 4).compare("\\\\?\\") == 0) {
				// Local path, remove the leading "\\?\"
				canonicalized_path = canonicalized_path.substr(4);
			}
		}
	}
#else // IS_TRUE(PLATFORM_WINDOWS)
	char buffer[PATH_MAX];
	if (realpath(path, buffer)) { // $TODO test this
		canonicalized_path = buffer;
	}
#endif // IS_TRUE(PLATFORM_WINDOWS)

	return canonicalized_path;
}

std::string get_path_directory(const char *path) {
	// Find the last path separator and remove everything after it... are there any weird edge cases here?
	std::string directory = path;

	size_t separator_index = directory.find_last_of(k_path_separator);
	if constexpr (k_path_separator != k_alt_path_separator) {
		size_t alt_separator_index = directory.find_last_of(k_alt_path_separator);
		if (alt_separator_index != std::string::npos
			&& (separator_index == std::string::npos || alt_separator_index > separator_index)) {
			separator_index = alt_separator_index;
		}
	}

	if (separator_index == std::string::npos) {
		directory.clear();
	} else {
		directory = directory.substr(0, separator_index);
	}

	return directory;
}

bool is_path_relative(const char *path) {
#if IS_TRUE(PLATFORM_WINDOWS)
	return PathIsRelativeA(path) == TRUE;
#else // IS_TRUE(PLATFORM_WINDOWS)
	// Absolute paths start with /, and otherwise it is relative
	return *path != '/';
#endif // IS_TRUE(PLATFORM_WINDOWS)
}

bool get_file_last_modified_timestamp(const char *path, uint64 &timestamp_out) {
	bool result = false;

#if IS_TRUE(PLATFORM_WINDOWS)
	HANDLE file_handle = CreateFileA(
		path,
		0,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (file_handle != INVALID_HANDLE_VALUE) {
		FILETIME last_write_time;
		if (GetFileTime(file_handle, nullptr, nullptr, &last_write_time)) {
			ULARGE_INTEGER conv;
			conv.LowPart = last_write_time.dwLowDateTime;
			conv.HighPart = last_write_time.dwHighDateTime;
			// FILETIME is the number of 100-nanosecond intervals from Jan. 1 1601 UTC
			// Our timestamp uses the number of seconds since Jan. 1 1970 UTC
			timestamp_out = (conv.QuadPart / 1000000000ull) - 11644473600ull;
			result = true;
		}
	}

	if (file_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(file_handle);
	}
#else // IS_TRUE(PLATFORM_WINDOWS)
	struct stat stat_buffer;
	if (stat(path, &stat_buffer) == 0) {
		timestamp_out = stat_buffer.st_mtime;
		result = true;
	}
#endif // IS_TRUE(PLATFORM_WINDOWS)

	return result;
}

void create_directory(const char *path) {
#if IS_TRUE(PLATFORM_WINDOWS)
	CreateDirectoryA(path, nullptr);
#else // IS_TRUE(PLATFORM_WINDOWS)
	mkdir(path, 0777);
#endif // IS_TRUE(PLATFORM_WINDOWS)
}

e_read_full_file_result read_full_file(const char *path, std::vector<char> &file_contents_out) {
	file_contents_out.clear();

	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		return e_read_full_file_result::k_failed_to_open;
	}

	std::streampos full_file_size = file.tellg();
	file.seekg(0);

	if (full_file_size < 0) {
		return e_read_full_file_result::k_failed_to_read;
	}

	// Guard against 32-bit platforms
	uint64 file_size_uint64 = static_cast<uint64>(full_file_size);
	if (file_size_uint64 > static_cast<uint64>(std::numeric_limits<size_t>::max())) {
		return e_read_full_file_result::k_file_too_big;
	}

	size_t file_size = cast_integer_verify<size_t>(file_size_uint64);
	file_contents_out.resize(file_size);
	if (file_size > 0) {
		file.read(file_contents_out.data(), file_size);
	}

	return file.fail()
		? e_read_full_file_result::k_failed_to_read
		: e_read_full_file_result::k_success;
}
