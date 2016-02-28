#include "common/utility/file_utility.h"

#if PREDEFINED(PLATFORM_WINDOWS)
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#else // PREDEFINED(PLATFORM_WINDOWS)
#include <sys/types.h>
#include <sys/stat.h>
#endif // PREDEFINED(PLATFORM_WINDOWS)

bool are_file_paths_equivalent(const char *path_a, const char *path_b) {
	if (strcmp(path_a, path_b) == 0) {
		return true;
	}

#if PREDEFINED(PLATFORM_WINDOWS)
	{
		bool result = false;

		HANDLE file_a_handle = CreateFile(path_a, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		HANDLE file_b_handle = CreateFile(path_b, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file_a_handle != INVALID_HANDLE_VALUE &&
			file_b_handle != INVALID_HANDLE_VALUE) {
			BY_HANDLE_FILE_INFORMATION file_a_info;
			BY_HANDLE_FILE_INFORMATION file_b_info;
			if (GetFileInformationByHandle(file_a_handle, &file_a_info) &&
				GetFileInformationByHandle(file_b_handle, &file_b_info)) {
				result =
					(file_a_info.dwVolumeSerialNumber == file_b_info.dwVolumeSerialNumber) &&
					(file_a_info.nFileIndexHigh == file_b_info.nFileIndexHigh) &&
					(file_a_info.nFileIndexLow == file_b_info.nFileIndexLow);
			}
		}

		if (file_a_handle != INVALID_HANDLE_VALUE) {
			CloseHandle(file_a_handle);
		}

		if (file_b_handle != INVALID_HANDLE_VALUE) {
			CloseHandle(file_b_handle);
		}

		return result;
	}
#else // PREDEFINED(PLATFORM_WINDOWS)
	{
		struct s_stat stat_buffer_a;
		struct s_stat stat_buffer_b;
		int32 result_a = get_stat(path_a, &stat_buffer_a);
		int32 result_b = get_stat(path_b, &stat_buffer_b);

		if (result_a != 0 || result_b != 0) {
			// In the event of any error, assume the files are different
			return false;
		}

		return stat_buffer_a.st_ino == stat_buffer_b.st_ino;
	}
#endif // PREDEFINED(PLATFORM_WINDOWS)
}

bool is_path_relative(const char *path) {
#if PREDEFINED(PLATFORM_WINDOWS)
	return PathIsRelative(path) == TRUE;
#else // PREDEFINED(PLATFORM_WINDOWS)
	// Absolute paths start with /, and otherwise it is relative
	return *path != '/';
#endif // PREDEFINED(PLATFORM_WINDOWS)
}

bool get_file_last_modified_timestamp(const char *path, uint64 &out_timestamp) {
	bool result = false;

#if PREDEFINED(PLATFORM_WINDOWS)
	HANDLE file_handle = CreateFile(path, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file_handle != INVALID_HANDLE_VALUE) {
		FILETIME last_write_time;
		if (GetFileTime(file_handle, nullptr, nullptr, &last_write_time)) {
			ULARGE_INTEGER conv;
			conv.LowPart = last_write_time.dwLowDateTime;
			conv.HighPart = last_write_time.dwHighDateTime;
			// FILETIME is the number of 100-nanosecond intervals from Jan. 1 1601 UTC
			// Our timestamp uses the number of seconds since Jan. 1 1970 UTC
			out_timestamp = (conv.QuadPart / 1000000000ull) - 11644473600ull;
			result = true;
		}
	}

	if (file_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(file_handle);
	}
#else // PREDEFINED(PLATFORM_WINDOWS)
	struct s_stat stat_buffer;
	if (get_stat(path, &stat_buffer) == 0) {
		out_timestamp = stat_buffer.st_mtime;
		result = true;
	}
#endif // PREDEFINED(PLATFORM_WINDOWS)

	return result;
}