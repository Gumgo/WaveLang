#include "engine/profiler/stopwatch.h"

#if PREDEFINED(PLATFORM_WINDOWS)

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
	wl_vassert(m_frequency > 0, "Not initialized");
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	int64 duration = time.QuadPart - m_start_time;

	return (duration * k_nanoseconds_per_second) / m_frequency;
}

#else // PREDEFINED(PLATFORM_WINDOWS)
#error Unknown platform
#endif // PREDEFINED(PLATFORM_WINDOWS)