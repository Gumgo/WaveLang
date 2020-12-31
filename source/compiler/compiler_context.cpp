#include "common/utility/file_utility.h"

#include "compiler/compiler_context.h"

#include <sstream>

c_compiler_context::c_compiler_context(c_wrapped_array<void *> native_module_library_contexts) {
	m_native_module_library_contexts = native_module_library_contexts;
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
	s_compiler_message message_entry = {
		str_vformat(format, args)
	};

	output_to_stream(std::cout, "Message", -1, location, message_entry.message.c_str());
	m_messages.emplace_back(message_entry);
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
	s_compiler_warning warning_entry = {
		warning,
		str_vformat(format, args)
	};

	output_to_stream(std::cerr, "Warning", enum_index(warning), location, warning_entry.message.c_str());
	m_warnings.emplace_back(warning_entry);
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
	s_compiler_error error_entry = {
		error,
		str_vformat(format, args)
	};

	output_to_stream(std::cerr, "Error", enum_index(error), location, error_entry.message.c_str());
	m_errors.emplace_back(error_entry);
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

size_t c_compiler_context::get_message_count() const {
	return m_messages.size();
}

const s_compiler_message &c_compiler_context::get_message(size_t index) const {
	return m_messages[index];
}

size_t c_compiler_context::get_warning_count() const {
	return m_warnings.size();
}

const s_compiler_warning &c_compiler_context::get_warning(size_t index) const {
	return m_warnings[index];
}

size_t c_compiler_context::get_error_count() const {
	return m_errors.size();
}

const s_compiler_error &c_compiler_context::get_error(size_t index) const {
	return m_errors[index];
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
	const char *message) const {
	stream << prefix;
	if (code >= 0) {
		stream << " (code " << code << ")";
	}
	stream << get_source_location_string(location) << ": " << message << "\n";
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
