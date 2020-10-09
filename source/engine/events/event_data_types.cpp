#include "engine/events/event_data_types.h"

uint8 s_event_data_type_definition<bool>::id;

bool s_event_data_type_definition<bool>::build_event(c_event_data_stack &event_data_stack, bool value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<bool>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%d", *static_cast<const bool *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(bool);
}

uint8 s_event_data_type_definition<int8>::id;

bool s_event_data_type_definition<int8>::build_event(c_event_data_stack &event_data_stack, int8 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<int8>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%d", *static_cast<const int8 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(int8);
}

uint8 s_event_data_type_definition<uint8>::id;

bool s_event_data_type_definition<uint8>::build_event(c_event_data_stack &event_data_stack, uint8 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<uint8>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%u", *static_cast<const uint8 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(uint8);
}

uint8 s_event_data_type_definition<int16>::id;

bool s_event_data_type_definition<int16>::build_event(c_event_data_stack &event_data_stack, int16 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<int16>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%d", *static_cast<const int16 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(int16);
}

uint8 s_event_data_type_definition<uint16>::id;

bool s_event_data_type_definition<uint16>::build_event(c_event_data_stack &event_data_stack, uint16 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<uint16>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%u", *static_cast<const uint16 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(uint16);
}

uint8 s_event_data_type_definition<int32>::id;

bool s_event_data_type_definition<int32>::build_event(c_event_data_stack &event_data_stack, int32 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<int32>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%d", *static_cast<const int32 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(int32);
}

uint8 s_event_data_type_definition<uint32>::id;

bool s_event_data_type_definition<uint32>::build_event(c_event_data_stack &event_data_stack, uint32 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<uint32>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%u", *static_cast<const uint32 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(uint32);
}

uint8 s_event_data_type_definition<int64>::id;

bool s_event_data_type_definition<int64>::build_event(c_event_data_stack &event_data_stack, int64 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<int64>::build_event_string(c_event_string &event_string, const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%" PRId64, *static_cast<const int64 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(int64);
}

uint8 s_event_data_type_definition<uint64>::id;

bool s_event_data_type_definition<uint64>::build_event(c_event_data_stack &event_data_stack, uint64 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<uint64>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%" PRIu64, *static_cast<const uint64 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(uint64);
}

uint8 s_event_data_type_definition<real32>::id;

bool s_event_data_type_definition<real32>::build_event(c_event_data_stack &event_data_stack, real32 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<real32>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%f", *static_cast<const real32 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(real32);
}

uint8 s_event_data_type_definition<real64>::id;

bool s_event_data_type_definition<real64>::build_event(c_event_data_stack &event_data_stack, real64 value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<real64>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	char buffer[16];
	snprintf(buffer, array_count(buffer), "%f", *static_cast<const real64 *>(data_pointer));
	event_string.append_truncate(buffer);
	return sizeof(real64);
}

uint8 s_event_data_type_definition<const char *>::id;

bool s_event_data_type_definition<const char *>::build_event(c_event_data_stack &event_data_stack, const char *value) {
	return event_data_stack.try_append(value);
}

size_t s_event_data_type_definition<const char *>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	const char *str = *static_cast<const char * const *>(data_pointer);
	event_string.append_truncate(str);
	return sizeof(const char *);
}

uint8 s_event_data_type_definition<c_dstr>::id;

bool s_event_data_type_definition<c_dstr>::build_event(c_event_data_stack &event_data_stack, c_dstr value) {
	return event_data_stack.try_append(strlen(value.m_str) + 1, value.m_str);
}

size_t s_event_data_type_definition<c_dstr>::build_event_string(
	c_event_string &event_string,
	const void *data_pointer) {
	const char *str = static_cast<const char *>(data_pointer);
	event_string.append_truncate(str);
	return strlen(str) + 1;
}

void initialize_event_data_types() {
	register_event_data_type<bool>();
	register_event_data_type<int8>();
	register_event_data_type<uint8>();
	register_event_data_type<int16>();
	register_event_data_type<uint16>();
	register_event_data_type<int32>();
	register_event_data_type<uint32>();
	register_event_data_type<int64>();
	register_event_data_type<uint64>();
	register_event_data_type<real32>();
	register_event_data_type<real64>();
	register_event_data_type<const char *>();
	register_event_data_type<c_dstr>();
}
