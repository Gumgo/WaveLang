#include "engine/profiler/profiler.h"
#include "engine/task_function_registry.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

static real64 nanoseconds_to_milliseconds(int64 nanoseconds) {
	return static_cast<real64>(k_milliseconds_per_second * nanoseconds) / static_cast<real64>(k_nanoseconds_per_second);
}

void s_profiler_record::reset() {
	sample_count = 0;
	average_time = 0;
	min_time = std::numeric_limits<int64>::max();
	max_time = std::numeric_limits<int64>::min();
}

void s_profiler_record::update(int64 time) {
	sample_count++;
	average_time += time;
	min_time = std::min(min_time, time);
	max_time = std::max(max_time, time);
}

void s_profiler_record::resolve() {
	if (sample_count == 0) {
		average_time = 0;
		min_time = 0;
		max_time = 0;
	} else {
		average_time /= sample_count;
	}
}

bool output_profiler_report(const char *filename, const s_profiler_report &report) {
	std::ofstream out(filename);
	if (!out.is_open()) {
		return false;
	}

	out << "Execution\n";
	out << "Total avg,Total min,Total max\n" <<
		nanoseconds_to_milliseconds(report.execution_total_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_total_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_total_time.max_time) << "\n";
	out << "Voice avg,Voice min,Voice max\n" <<
		nanoseconds_to_milliseconds(report.voice_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.voice_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.voice_time.max_time) << "\n";
	out << "FX avg,FX min,FX max\n" <<
		nanoseconds_to_milliseconds(report.fx_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.fx_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.fx_time.max_time) << "\n";
	out << "Overhead avg,Overhead min,Overhead max\n" <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.average_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.min_time) << "," <<
		nanoseconds_to_milliseconds(report.execution_overhead_time.max_time) << "\n";

	out << "\n";

	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		out << ((instrument_stage == k_instrument_stage_voice) ? "Voice tasks\n" : "FX tasks\n");
		out << "Task Index,Task UID,Task name,Total avg,Total min,Total max,Function avg,Function min,Function max,"
			"Overhead avg,Overhead min,Overhead max\n";

		const std::vector<s_profiler_report::s_task> &tasks = report.tasks[instrument_stage];
		for (uint32 task_index = 0; task_index < tasks.size(); task_index++) {
			const s_profiler_report::s_task &task = tasks[task_index];
			const s_task_function &task_function =
				c_task_function_registry::get_task_function(task.task_function_index);

			std::stringstream uid_stream;
			uid_stream << "0x" << std::setfill('0') << std::setw(2) << std::hex;
			for (size_t b = 0; b < NUMBEROF(task_function.uid.data); b++) {
				uid_stream << static_cast<uint32>(task_function.uid.data[b]);
			}

			out << task_index << "," <<
				uid_stream.str() << "," <<
				task_function.name.get_string() << "," <<
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

		out << "\n";
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
	m_tasks[k_instrument_stage_voice].free();
	m_tasks[k_instrument_stage_fx].free();
	m_tasks[k_instrument_stage_voice].allocate(settings.voice_task_count);
	m_tasks[k_instrument_stage_fx].allocate(settings.fx_task_count);

	m_execution.stopwatch.initialize();
	for (size_t thread = 0; thread < m_thread_contexts.get_array().get_count(); thread++) {
		m_thread_contexts.get_array()[thread].stopwatch.initialize();
	}
}

void c_profiler::start() {
	m_execution.total_time.reset();
	m_execution.overhead_time.reset();
	m_execution.voice_time.reset();
	m_execution.fx_time.reset();

	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		for (size_t index = 0; index < m_tasks[instrument_stage].get_array().get_count(); index++) {
			s_task &task = m_tasks[instrument_stage].get_array()[index];
			ZERO_STRUCT(&task);
			task.total_time.reset();
			task.function_time.reset();
			task.overhead_time.reset();
		}
	}
}

void c_profiler::stop() {
	m_execution.total_time.resolve();
	m_execution.overhead_time.resolve();
	m_execution.voice_time.resolve();
	m_execution.fx_time.resolve();

	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		c_wrapped_array<s_task> tasks = m_tasks[instrument_stage].get_array();

		for (size_t index = 0; index < tasks.get_count(); index++) {
			s_task &task = tasks[index];
			task.total_time.resolve();
			task.function_time.resolve();
			task.overhead_time.resolve();
		}
	}
}

void c_profiler::get_report(s_profiler_report &out_report) const {
	out_report.execution_total_time = m_execution.total_time;
	out_report.execution_overhead_time = m_execution.overhead_time;
	out_report.voice_time = m_execution.voice_time;
	out_report.fx_time = m_execution.fx_time;

	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		c_wrapped_array_const<s_task> tasks = m_tasks[instrument_stage].get_array();
		std::vector<s_profiler_report::s_task> &report_tasks = out_report.tasks[instrument_stage];
		report_tasks.resize(tasks.get_count());

		for (size_t index = 0; index < tasks.get_count(); index++) {
			const s_task &task = tasks[index];
			report_tasks[index].task_function_index = task.task_function_index;
			report_tasks[index].task_total_time = task.total_time;
			report_tasks[index].task_function_time = task.function_time;
			report_tasks[index].task_overhead_time = task.overhead_time;
		}
	}
}

void c_profiler::begin_execution() {
	m_execution.stopwatch.reset();
}

void c_profiler::begin_voices() {
	m_execution.query_points[k_execution_query_point_begin_voices] = m_execution.stopwatch.query();
}

void c_profiler::end_voices() {
	m_execution.query_points[k_execution_query_point_end_voices] = m_execution.stopwatch.query();
}

void c_profiler::begin_voice() {
	m_execution.query_points[k_execution_query_point_begin_voice] = m_execution.stopwatch.query();
}

void c_profiler::end_voice() {
	m_execution.query_points[k_execution_query_point_end_voice] = m_execution.stopwatch.query();

	int64 time =
		m_execution.query_points[k_execution_query_point_end_voice] -
		m_execution.query_points[k_execution_query_point_begin_voice];
	m_execution.voice_time.update(time);
}

void c_profiler::begin_fx() {
	m_execution.query_points[k_execution_query_point_begin_fx] = m_execution.stopwatch.query();
}

void c_profiler::end_fx() {
	m_execution.query_points[k_execution_query_point_end_fx] = m_execution.stopwatch.query();

	int64 time =
		m_execution.query_points[k_execution_query_point_end_fx] -
		m_execution.query_points[k_execution_query_point_begin_fx];
	m_execution.fx_time.update(time);
}

void c_profiler::end_execution() {
	m_execution.query_points[k_execution_query_point_end_execution] = m_execution.stopwatch.query();

	int64 total_time =
		m_execution.query_points[k_execution_query_point_end_execution];
	int64 voice_time =
		m_execution.query_points[k_execution_query_point_begin_voices] -
		m_execution.query_points[k_execution_query_point_end_voices];
	int64 fx_time =
		m_execution.query_points[k_execution_query_point_begin_fx] -
		m_execution.query_points[k_execution_query_point_end_fx];
	int64 overhead_time = total_time - voice_time - fx_time;

	m_execution.total_time.update(total_time);
	m_execution.overhead_time.update(overhead_time);
}

void c_profiler::begin_task(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index,
	uint32 task_function_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[instrument_stage].get_array()[task_index];
	task.task_function_index = task_function_index;
	thread_context.stopwatch.reset();
}

void c_profiler::begin_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[instrument_stage].get_array()[task_index];
	task.query_points[k_task_query_point_begin_function] = thread_context.stopwatch.query();
}

void c_profiler::end_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[instrument_stage].get_array()[task_index];
	task.query_points[k_task_query_point_end_function] = thread_context.stopwatch.query();

}

void c_profiler::end_task(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[instrument_stage].get_array()[task_index];
	task.query_points[k_task_query_point_end_task] = thread_context.stopwatch.query();

	int64 total_time =
		task.query_points[k_task_query_point_end_task];
	int64 function_time =
		task.query_points[k_task_query_point_end_function] -
		task.query_points[k_task_query_point_begin_function];
	int64 overhead_time = total_time - function_time;

	task.total_time.update(total_time);
	task.function_time.update(function_time);
	task.overhead_time.update(overhead_time);
}
