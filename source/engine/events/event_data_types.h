#pragma once

#include "common/common.h"

#include "engine/events/event_interface.h"

#include <cinttypes>
#include <cstdio>

// Event type declarations

template<> struct s_event_data_type_definition<bool> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, bool value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<int8> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, int8 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<uint8> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, uint8 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<int16> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, int16 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<uint16> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, uint16 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<int32> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, int32 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<uint32> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, uint32 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<int64> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, int64 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<uint64> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, uint64 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<real32> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, real32 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

template<> struct s_event_data_type_definition<real64> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, real64 value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

// Static string - only pointer is copied
template<> struct s_event_data_type_definition<const char *> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, const char *value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

// Dynamic string - entire string is copied
class c_dstr {
public:
	c_dstr(const char *str) { m_str = str; }
	const char *m_str;
};

template<> struct s_event_data_type_definition<c_dstr> {
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, c_dstr value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

void initialize_event_data_types();

