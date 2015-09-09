#include "engine/profiler/profiler.h"
#include <algorithm>
#include <fstream>

static real64 nanoseconds_to_milliseconds(int64 nanoseconds) {
	return static_cast<real64>(k_milliseconds_per_second * nanoseconds) / static_cast<real64>(k_nanoseconds_per_second);
}

bool output_profiler_report(const char *filename, const s_profiler_report &report) {
	std::ofstream out(filename);
	if (!out.is_open()) {
		return false;
	}

	out << "-,Total avg,Total min,Total max,Task avg,Task min,Task max,Overhead avg,Overhead min,Overhead max\n";
	out << "Execution," <<
		nanoseconds_to_milliseconds(report.execution_total_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_total_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_total_time.max_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_task_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_task_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_task_time.max_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.max_time) << "\n";

	out << "Task,Total avg,Total min,Total max,Function avg,Function min,Function max,"
		"Overhead avg,Overhead min,Overhead max\n";
	for (uint32 task_index = 0; task_index < report.tasks.size(); task_index++) {
		const s_profiler_report::s_task &task = report.tasks[task_index];
		out << task_index << "," <<
			nanoseconds_to_milliseconds(task.task_total_time.average_time) << "," <<
			nanoseconds_to_milliseconds(task.task_total_time.min_time) << "," <<
			nanoseconds_to_milliseconds(task.task_total_time.max_time) << "," <<
			nanoseconds_to_milliseconds(task.task_function_time.average_time) << "," <<
			nanoseconds_to_milliseconds(task.task_function_time.min_time) << "," <<
			nanoseconds_to_milliseconds(task.task_function_time.max_time) << "," <<
			nanoseconds_to_milliseconds(task.task_overhead_time.average_time) << "," <<
			nanoseconds_to_milliseconds(task.task_overhead_time.min_time) << "," <<
			nanoseconds_to_milliseconds(task.task_overhead_time.max_time) << "\n";
	}

	return !out.fail();
}

c_profiler::c_profiler() {
}

c_profiler::~c_profiler() {
}

void c_profiler::initialize(const s_profiler_settings &settings) {
	m_thread_contexts.free();
	m_thread_contexts.allocate(settings.worker_thread_count);
	m_tasks.free();
	m_tasks.allocate(settings.task_count);

	m_execution.stopwatch.initialize();
	for (size_t thread = 0; thread < m_thread_contexts.get_array().get_count(); thread++) {
		m_thread_contexts.get_array()[thread].stopwatch.initialize();
	}
}

void c_profiler::start() {
	m_execution.instance_count = 0;
	ZERO_STRUCT(&m_execution.total_time);
	ZERO_STRUCT(&m_execution.task_time);
	ZERO_STRUCT(&m_execution.overhead_time);
	ZERO_STRUCT(&m_execution.query_points);
	m_execution.total_time.min_time = std::numeric_limits<int64>::max();
	m_execution.task_time.min_time = std::numeric_limits<int64>::max();
	m_execution.overhead_time.min_time = std::numeric_limits<int64>::max();
	m_execution.running_task_time = 0;

	for (size_t index = 0; index < m_tasks.get_array().get_count(); index++) {
		s_task &task = m_tasks.get_array()[index];
		ZERO_STRUCT(&task);
		task.total_time.min_time = std::numeric_limits<int64>::max();
		task.function_time.min_time = std::numeric_limits<int64>::max();
		task.overhead_time.min_time = std::numeric_limits<int64>::max();
	}
}

void c_profiler::stop() {
	if (m_execution.instance_count > 0) {
		m_execution.total_time.average_time /= m_execution.instance_count;
		m_execution.task_time.average_time /= m_execution.instance_count;
		m_execution.overhead_time.average_time /= m_execution.instance_count;
	} else {
		m_execution.total_time.min_time = 0;
		m_execution.task_time.min_time = 0;
		m_execution.overhead_time.min_time = 0;
	}

	for (size_t index = 0; index < m_tasks.get_array().get_count(); index++) {
		s_task &task = m_tasks.get_array()[index];
		if (task.instance_count > 0) {
			task.total_time.average_time /= task.instance_count;
			task.function_time.average_time /= task.instance_count;
			task.overhead_time.average_time /= task.instance_count;
		} else {
			task.total_time.min_time = 0;
			task.function_time.min_time = 0;
			task.overhead_time.min_time = 0;
		}
	}
}

void c_profiler::get_report(s_profiler_report &out_report) const {
	out_report.execution_total_time = m_execution.total_time;
	out_report.execution_task_time = m_execution.task_time;
	out_report.execution_overhead_time = m_execution.overhead_time;

	out_report.tasks.resize(m_tasks.get_array().get_count());
	for (size_t index = 0; index < m_tasks.get_array().get_count(); index++) {
		const s_task &task = m_tasks.get_array()[index];
		out_report.tasks[index].task_total_time = task.total_time;
		out_report.tasks[index].task_function_time = task.function_time;
		out_report.tasks[index].task_overhead_time = task.overhead_time;
	}
}

void c_profiler::begin_execution() {
	m_execution.stopwatch.reset();
	m_execution.running_task_time = 0;
}

void c_profiler::begin_execution_tasks() {
	m_execution.query_points[k_execution_query_point_begin_tasks] = m_execution.stopwatch.query();
}

void c_profiler::end_execution_tasks() {
	m_execution.query_points[k_execution_query_point_end_tasks] = m_execution.stopwatch.query();
	m_execution.running_task_time +=
		m_execution.query_points[k_execution_query_point_end_tasks] -
		m_execution.query_points[k_execution_query_point_begin_tasks];
}

void c_profiler::end_execution() {
	m_execution.query_points[k_execution_query_point_end_execution] = m_execution.stopwatch.query();

	int64 total_time =
		m_execution.query_points[k_execution_query_point_end_execution];
	int64 task_time =
		m_execution.running_task_time;
	int64 overhead_time = total_time - task_time;

	m_execution.total_time.average_time += total_time;
	m_execution.total_time.min_time = std::min(m_execution.total_time.min_time, total_time);
	m_execution.total_time.max_time = std::max(m_execution.total_time.max_time, total_time);

	m_execution.task_time.average_time += task_time;
	m_execution.task_time.min_time = std::min(m_execution.task_time.min_time, task_time);
	m_execution.task_time.max_time = std::max(m_execution.task_time.max_time, task_time);

	m_execution.overhead_time.average_time += overhead_time;
	m_execution.overhead_time.min_time = std::min(m_execution.overhead_time.min_time, overhead_time);
	m_execution.overhead_time.max_time = std::max(m_execution.overhead_time.max_time, overhead_time);

	m_execution.instance_count++;
}

void c_profiler::begin_task(uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	thread_context.stopwatch.reset();
}

void c_profiler::begin_task_function(uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks.get_array()[task_index];
	task.query_points[k_task_query_point_begin_function] = thread_context.stopwatch.query();
}

void c_profiler::end_task_function(uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks.get_array()[task_index];
	task.query_points[k_task_query_point_end_function] = thread_context.stopwatch.query();

}

void c_profiler::end_task(uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks.get_array()[task_index];
	task.query_points[k_task_query_point_end_task] = thread_context.stopwatch.query();

	int64 total_time =
		task.query_points[k_task_query_point_end_task];
	int64 function_time =
		task.query_points[k_task_query_point_end_function] -
		task.query_points[k_task_query_point_begin_function];
	int64 overhead_time = total_time - function_time;

	task.total_time.average_time += total_time;
	task.total_time.min_time = std::min(task.total_time.min_time, total_time);
	task.total_time.max_time = std::max(task.total_time.max_time, total_time);

	task.function_time.average_time += function_time;
	task.function_time.min_time = std::min(task.function_time.min_time, function_time);
	task.function_time.max_time = std::max(task.function_time.max_time, function_time);

	task.overhead_time.average_time += overhead_time;
	task.overhead_time.min_time = std::min(task.overhead_time.min_time, overhead_time);
	task.overhead_time.max_time = std::max(task.overhead_time.max_time, overhead_time);

	task.instance_count++;
}