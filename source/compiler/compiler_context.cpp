#include "common/utility/file_utility.h"

#include "compiler/compiler_context.h"

#include <sstream>

// $TODO move this to a more generic location
static std::string str_format(const char *format, ...);
static std::string str_vformat(const char *format, va_list args);

c_compiler_context::c_compiler_context(c_wrapped_array<void *> native_module_library_contexts) {
	m_native_module_library_contexts = native_module_library_contexts;

	m_warning_count = 0;
	m_error_count = 0;
}

void c_compiler_context::message(
	const s_compiler_source_location &location,
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	vmessage(location, format, args);
	va_end(args);
}

void c_compiler_context::vmessage(
	const s_compiler_source_location &location,
	const char *format,
	va_list args) {
	output_to_stream(std::cout, "Message", -1, location, format, args);
}

void c_compiler_context::message(
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	vmessage(format, args);
	va_end(args);
}

void c_compiler_context::vmessage(
	const char *format,
	va_list args) {
	vmessage(s_compiler_source_location(), format, args);
}

void c_compiler_context::warning(
	e_compiler_warning warning,
	const s_compiler_source_location &location,
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	vwarning(warning, location, format, args);
	va_end(args);
}

void c_compiler_context::vwarning(
	e_compiler_warning warning,
	const s_compiler_source_location &location,
	const char *format,
	va_list args) {
	output_to_stream(std::cerr, "Warning", enum_index(warning), location, format, args);
	m_warning_count++;
}

void c_compiler_context::warning(
	e_compiler_warning warning,
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	vwarning(warning, format, args);
	va_end(args);
}

void c_compiler_context::vwarning(
	e_compiler_warning warning,
	const char *format,
	va_list args) {
	vwarning(warning, s_compiler_source_location(), format, args);
}

void c_compiler_context::error(
	e_compiler_error error,
	const s_compiler_source_location &location,
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	verror(error, location, format, args);
	va_end(args);
}

void c_compiler_context::verror(
	e_compiler_error error,
	const s_compiler_source_location &location,
	const char *format,
	va_list args) {
	output_to_stream(std::cerr, "Error", enum_index(error), location, format, args);
	m_error_count++;
}

void c_compiler_context::error(
	e_compiler_error error,
	const char *format,
	...) {
	va_list args;
	va_start(args, format);
	verror(error, format, args);
	va_end(args);
}

void c_compiler_context::verror(
	e_compiler_error error,
	const char *format,
	va_list args) {
	verror(error, s_compiler_source_location(), format, args);
}

size_t c_compiler_context::get_warning_count() const {
	return m_warning_count;
}

size_t c_compiler_context::get_error_count() const {
	return m_error_count;
}

size_t c_compiler_context::get_source_file_count() const {
	return m_source_files.size();
}

s_compiler_source_file &c_compiler_context::get_source_file(h_compiler_source_file handle) {
	return *m_source_files[handle.get_data()].get();
}

const s_compiler_source_file &c_compiler_context::get_source_file(h_compiler_source_file handle) const {
	return *m_source_files[handle.get_data()].get();
}

h_compiler_source_file c_compiler_context::get_or_add_source_file(const char *path, bool &was_added_out) {
	was_added_out = false;

	std::string canonical_path = canonicalize_path(path);
	if (canonical_path.empty()) {
		return h_compiler_source_file::invalid();
	}

	c_index_handle_iterator<h_compiler_source_file> iterator(m_source_files.size());
	for (h_compiler_source_file existing_source_file_handle : iterator) {
		const s_compiler_source_file &existing_source_file = get_source_file(existing_source_file_handle);
		if (are_file_paths_equivalent(canonical_path.c_str(), existing_source_file.path.c_str())) {
			return existing_source_file_handle;
		}
	}

	h_compiler_source_file handle = h_compiler_source_file::construct(m_source_files.size());
	m_source_files.emplace_back(std::make_unique<s_compiler_source_file>());
	m_source_files.back()->path = canonical_path;
	was_added_out = true;
	return handle;
}

void *c_compiler_context::get_native_module_library_context(h_native_module_library native_module_library_handle) {
	return m_native_module_library_contexts[native_module_library_handle.get_data()];
}

void c_compiler_context::output_to_stream(
	std::ostream &stream,
	const char *prefix,
	int32 code,
	const s_compiler_source_location &location,
	const char *format,
	va_list args) const {
	stream << prefix;
	if (code >= 0) {
		stream << " (code " << code << ")";
	}
	stream << get_source_location_string(location) << ": " << str_format(format, args) << "\n";
}

std::string c_compiler_context::get_source_location_string(const s_compiler_source_location &location) const {
	std::ostringstream stream;

	if (location.source_file_handle.is_valid()) {
		std::cout << " in file '" << m_source_files[location.source_file_handle.get_data()]->path << "'";
		if (location.line >= 0) {
			std::cout << " (" << location.line;
			if (location.character >= 0) {
				std::cout << ", " << location.character;
			}
			std::cout << ")";
		}
	}

	return stream.str();
}

static std::string str_format(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string result = str_format(format, args);
	va_end(args);
	return result;
}

static std::string str_vformat(const char *format, va_list args) {
	int32 required_length = std::vsnprintf(nullptr, 0, format, args);

	// A negative value indicates an encoding error
	wl_assert(required_length >= 0);

	std::vector<char> buffer(required_length + 1, 0);

	IF_ASSERTS_ENABLED(int32 length_verify = ) std::vsnprintf(&buffer.front(), buffer.size(), format, args);
	wl_assert(required_length == length_verify);

	return std::string(&buffer.front());
}
