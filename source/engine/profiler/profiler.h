#ifndef WAVELANG_PROFILER_H__
#define WAVELANG_PROFILER_H__

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/utility/stopwatch.h"
#include <vector>

struct s_profiler_settings {
	uint32 worker_thread_count;
	uint32 task_count;
};

struct s_profiler_record {
	int64 average_time;
	int64 min_time;
	int64 max_time;
};

struct s_profiler_report {
	struct s_task {
		uint32 task_function_index;
		s_profiler_record task_total_time;
		s_profiler_record task_function_time;
		s_profiler_record task_overhead_time;
	};

	s_profiler_record execution_total_time;
	s_profiler_record execution_task_time;
	s_profiler_record execution_overhead_time;

	std::vector<s_task> tasks;
};

bool output_profiler_report(const char *filename, const s_profiler_report &report);

class c_profiler {
public:
	c_profiler();
	~c_profiler();

	void initialize(const s_profiler_settings &settings);

	void start();
	void stop();
	void get_report(s_profiler_report &out_report) const;

	void begin_execution();
	void begin_execution_tasks();
	void end_execution_tasks();
	void end_execution();

	void begin_task(uint32 worker_thread, uint32 task_index, uint32 task_function_index);
	void begin_task_function(uint32 worker_thread, uint32 task_index);
	void end_task_function(uint32 worker_thread, uint32 task_index);
	void end_task(uint32 worker_thread, uint32 task_index);

private:
	enum e_execution_query_point {
		k_execution_query_point_begin_tasks,
		k_execution_query_point_end_tasks,
		k_execution_query_point_end_execution,

		k_execution_query_point_count
	};

	enum e_task_query_point {
		k_task_query_point_begin_function,
		k_task_query_point_end_function,
		k_task_query_point_end_task,

		k_task_query_point_count
	};

	struct ALIGNAS_LOCK_FREE s_execution {
		c_stopwatch stopwatch;
		uint32 instance_count;
		s_profiler_record total_time;
		s_profiler_record task_time;
		s_profiler_record overhead_time;
		int64 query_points[k_execution_query_point_count];
		int64 running_task_time;
	};

	struct ALIGNAS_LOCK_FREE s_thread_context {
		c_stopwatch stopwatch;
	};

	struct ALIGNAS_LOCK_FREE s_task {
		uint32 task_function_index;
		uint32 instance_count;
		s_profiler_record total_time;
		s_profiler_record function_time;
		s_profiler_record overhead_time;
		int64 query_points[k_task_query_point_count];
	};

	s_execution m_execution;
	c_lock_free_aligned_allocator<s_thread_context> m_thread_contexts;
	c_lock_free_aligned_allocator<s_task> m_tasks;
};

#endif // WAVELANG_PROFILER_H__