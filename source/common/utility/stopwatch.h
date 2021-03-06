#pragma once

#include "common/common.h"

static constexpr int64 k_nanoseconds_per_second = 1000000000l;
static constexpr int64 k_milliseconds_per_second = 1000l;

class c_stopwatch {
public:
	c_stopwatch();
	~c_stopwatch();

	void initialize();
	void reset();

	// Returns time from reset in nanoseconds
	int64 query();

	// Returns time from reset in milliseconds
	int64 query_ms();

private:
	int64 query_internal();

	int64 m_frequency;
	int64 m_start_time;
};

