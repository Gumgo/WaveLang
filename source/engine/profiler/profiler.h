#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/utility/stopwatch.h"

#include "execution_graph/instrument_stage.h"

#include <vector>

struct s_profiler_settings {
	uint32 worker_thread_count;
	uint32 voice_task_count;
	uint32 fx_task_count;
};

struct s_profiler_record {
	uint32 sample_count;
	int64 average_time;
	int64 min_time;
	int64 max_time;

	void reset();
	void update(int64 time);
	void resolve();
};

struct s_profiler_report {
	struct s_task {
		uint32 task_function_index;
		s_profiler_record task_total_time;
		s_profiler_record task_function_time;
		s_profiler_record task_overhead_time;
	};

	s_profiler_record execution_total_time;
	s_profiler_record execution_overhead_time;

	s_profiler_record single_voice_time;
	s_profiler_record voice_time;
	s_profiler_record fx_time;

	s_static_array<std::vector<s_task>, k_instrument_stage_count> tasks;
};

bool output_profiler_report(const char *filename, const s_profiler_report &report);

// $TODO add more query points. additional data includes channel mixing time, voice/FX setup time versus task time

class c_profiler {
public:
	c_profiler();
	~c_profiler();

	void initialize(const s_profiler_settings &settings);

	void start();
	void stop();
	void get_report(s_profiler_report &out_report) const;

	void begin_execution();
	void begin_voices();
	void end_voices();
	void begin_voice();
	void end_voice();
	void begin_fx();
	void end_fx();
	void end_execution(int64 min_total_time_threshold_ns);

	void begin_task(
		e_instrument_stage instrument_stage,
		uint32 worker_thread,
		uint32 task_index,
		uint32 task_function_index);
	void begin_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index);
	void end_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index);
	void end_task(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index);

private:
	enum e_execution_query_point {
		k_execution_query_point_begin_voices,
		k_execution_query_point_end_voices,
		k_execution_query_point_begin_voice,
		k_execution_query_point_end_voice,
		k_execution_query_point_begin_fx,
		k_execution_query_point_end_fx,
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
		s_profiler_record total_time;
		s_profiler_record overhead_time;
		s_profiler_record voice_time;
		s_profiler_record fx_time;
		s_static_array<int64, k_execution_query_point_count> query_points;
	};

	struct ALIGNAS_LOCK_FREE s_thread_context {
		c_stopwatch stopwatch;
	};

	struct ALIGNAS_LOCK_FREE s_task {
		uint32 task_function_index;
		s_profiler_record total_time;
		s_profiler_record function_time;
		s_profiler_record overhead_time;
		bool did_run;
		s_static_array<int64, k_task_query_point_count> query_points;
	};

	void commit();
	void commit_task(s_task &task);

	void discard();
	void discard_task(s_task &task);

	s_execution m_execution;
	c_lock_free_aligned_allocator<s_thread_context> m_thread_contexts;
	s_static_array<c_lock_free_aligned_allocator<s_task>, k_instrument_stage_count> m_tasks;
};

