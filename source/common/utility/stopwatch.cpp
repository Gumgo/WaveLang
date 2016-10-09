#include "common/utility/stopwatch.h"

#if IS_TRUE(PLATFORM_WINDOWS)

c_stopwatch::c_stopwatch() {
	m_frequency = 0;
}

c_stopwatch::~c_stopwatch() {
}

void c_stopwatch::initialize() {
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	m_frequency = frequency.QuadPart;
	wl_assert(m_frequency > 0);
}

void c_stopwatch::reset() {
	wl_vassert(m_frequency > 0, "Not initialized");
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	m_start_time = time.QuadPart;
}

int64 c_stopwatch::query() {
	int64 duration = query_internal();
	return (duration * k_nanoseconds_per_second) / m_frequency;
}

int64 c_stopwatch::query_ms() {
	int64 duration = query_internal();
	return (duration * k_milliseconds_per_second) / m_frequency;
}

int64 c_stopwatch::query_internal() {
	wl_vassert(m_frequency > 0, "Not initialized");
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return time.QuadPart - m_start_time;
}

#else // IS_TRUE(PLATFORM_WINDOWS)
#error Unknown platform
#endif // IS_TRUE(PLATFORM_WINDOWS)