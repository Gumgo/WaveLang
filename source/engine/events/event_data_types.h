#ifndef WAVELANG_EVENT_DATA_TYPES_H__
#define WAVELANG_EVENT_DATA_TYPES_H__

#include "common/common.h"

#include "engine/events/event_interface.h"

#include <cinttypes>
#include <cstdio>

// Event type declarations

template<> struct s_event_data_type_definition<bool> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, bool value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%d", *static_cast<const bool *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(bool);
	}
};

template<> struct s_event_data_type_definition<int8> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, int8 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%d", *static_cast<const int8 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(int8);
	}
};

template<> struct s_event_data_type_definition<uint8> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, uint8 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%u", *static_cast<const uint8 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(uint8);
	}
};

template<> struct s_event_data_type_definition<int16> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, int16 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%d", *static_cast<const int16 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(int16);
	}
};

template<> struct s_event_data_type_definition<uint16> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, uint16 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%u", *static_cast<const uint16 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(uint16);
	}
};

template<> struct s_event_data_type_definition<int32> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, int32 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%d", *static_cast<const int32 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(int32);
	}
};

template<> struct s_event_data_type_definition<uint32> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, uint32 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%u", *static_cast<const uint32 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(uint32);
	}
};

template<> struct s_event_data_type_definition<int64> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, int64 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%" PRId64, *static_cast<const int64 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(int64);
	}
};

template<> struct s_event_data_type_definition<uint64> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, uint64 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%" PRIu64, *static_cast<const uint64 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(uint64);
	}
};

template<> struct s_event_data_type_definition<real32> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, real32 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%f", *static_cast<const real32 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(real32);
	}
};

template<> struct s_event_data_type_definition<real64> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, real64 value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		char buffer[16];
		snprintf(buffer, NUMBEROF(buffer), "%f", *static_cast<const real64 *>(data_pointer));
		event_string.append_truncate(buffer);
		return sizeof(real64);
	}
};

// Static string - only pointer is copied
template<> struct s_event_data_type_definition<const char *> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, const char *value) {
		return event_data_stack.try_append(value);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		const char *str = *static_cast<const char * const *>(data_pointer);
		event_string.append_truncate(str);
		return sizeof(const char *);
	}
};

// Dynamic string - entire string is copied
class c_dstr {
public:
	c_dstr(const char *str) { m_str = str; }
	const char *m_str;
};

template<> struct s_event_data_type_definition<c_dstr> {
	static uint8 id;

	static bool build_event(c_event_data_stack &event_data_stack, c_dstr value) {
		return event_data_stack.try_append(strlen(value.m_str) + 1, value.m_str);
	}

	static size_t build_event_string(c_event_string &event_string, const void *data_pointer) {
		const char *str = static_cast<const char *>(data_pointer);
		event_string.append_truncate(str);
		return strlen(str) + 1;
	}
};

void initialize_event_data_types();

#endif // WAVELANG_EVENT_DATA_TYPES_H__