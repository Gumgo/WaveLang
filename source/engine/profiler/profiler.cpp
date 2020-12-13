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
	out << "Total avg,Total min,Total max\n"
		<< nanoseconds_to_milliseconds(report.execution_total_time.average_time) << ","
		<< nanoseconds_to_milliseconds(report.execution_total_time.min_time) << ","
		<< nanoseconds_to_milliseconds(report.execution_total_time.max_time) << "\n";
	out << "Voice avg,Voice min,Voice max\n"
		<< nanoseconds_to_milliseconds(report.voice_time.average_time) << ","
		<< nanoseconds_to_milliseconds(report.voice_time.min_time) << ","
		<< nanoseconds_to_milliseconds(report.voice_time.max_time) << "\n";
	out << "FX avg,FX min,FX max\n"
		<< nanoseconds_to_milliseconds(report.fx_time.average_time) << ","
		<< nanoseconds_to_milliseconds(report.fx_time.min_time) << ","
		<< nanoseconds_to_milliseconds(report.fx_time.max_time) << "\n";
	out << "Overhead avg,Overhead min,Overhead max\n"
		<< nanoseconds_to_milliseconds(report.execution_overhead_time.average_time) << ","
		<< nanoseconds_to_milliseconds(report.execution_overhead_time.min_time) << ","
		<< nanoseconds_to_milliseconds(report.execution_overhead_time.max_time) << "\n";

	out << "\n";

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		out << (instrument_stage == e_instrument_stage::k_voice) ? "Voice tasks\n" : "FX tasks\n";
		out << "Task Index,Task UID,Task name,Total avg,Total min,Total max,Function avg,Function min,Function max,"
			"Overhead avg,Overhead min,Overhead max\n";

		const std::vector<s_profiler_report::s_task> &tasks = report.tasks[enum_index(instrument_stage)];
		for (uint32 task_index = 0; task_index < tasks.size(); task_index++) {
			const s_profiler_report::s_task &task = tasks[task_index];
			if (task.task_total_time.sample_count == 0) {
				// Don't report this task if it never ran
				continue;
			}

			const s_task_function &task_function =
				c_task_function_registry::get_task_function(task.task_function_index);

			std::stringstream uid_stream;
			uid_stream << "0x" << std::setfill('0') << std::setw(2) << std::hex;
			for (size_t b = 0; b < array_count(task_function.uid.data); b++) {
				uid_stream << static_cast<uint32>(task_function.uid.data[b]);
			}

			out << task_index << ","
				<< uid_stream.str() << ","
				<< task_function.name.get_string() << ","
				<< nanoseconds_to_milliseconds(task.task_total_time.average_time) << ","
				<< nanoseconds_to_milliseconds(task.task_total_time.min_time) << ","
				<< nanoseconds_to_milliseconds(task.task_total_time.max_time) << ","
				<< nanoseconds_to_milliseconds(task.task_function_time.average_time) << ","
				<< nanoseconds_to_milliseconds(task.task_function_time.min_time) << ","
				<< nanoseconds_to_milliseconds(task.task_function_time.max_time) << ","
				<< nanoseconds_to_milliseconds(task.task_overhead_time.average_time) << ","
				<< nanoseconds_to_milliseconds(task.task_overhead_time.min_time) << ","
				<< nanoseconds_to_milliseconds(task.task_overhead_time.max_time) << "\n";
		}

		out << "\n";
	}

	return !out.fail();
}

c_profiler::c_profiler() {}

c_profiler::~c_profiler() {}

void c_profiler::initialize(const s_profiler_settings &settings) {
	m_thread_contexts.free();
	m_thread_contexts.allocate(settings.worker_thread_count);
	m_tasks[enum_index(e_instrument_stage::k_voice)].free();
	m_tasks[enum_index(e_instrument_stage::k_fx)].free();
	m_tasks[enum_index(e_instrument_stage::k_voice)].allocate(settings.voice_task_count);
	m_tasks[enum_index(e_instrument_stage::k_fx)].allocate(settings.fx_task_count);

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

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		for (size_t index = 0; index < m_tasks[enum_index(instrument_stage)].get_array().get_count(); index++) {
			s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[index];
			zero_type(&task);
			task.total_time.reset();
			task.function_time.reset();
			task.overhead_time.reset();
			task.did_run = false;
		}
	}
}

void c_profiler::stop() {
	m_execution.total_time.resolve();
	m_execution.overhead_time.resolve();
	m_execution.voice_time.resolve();
	m_execution.fx_time.resolve();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		c_wrapped_array<s_task> tasks = m_tasks[enum_index(instrument_stage)].get_array();

		for (size_t index = 0; index < tasks.get_count(); index++) {
			s_task &task = tasks[index];
			task.total_time.resolve();
			task.function_time.resolve();
			task.overhead_time.resolve();
		}
	}
}

void c_profiler::get_report(s_profiler_report &report_out) const {
	report_out.execution_total_time = m_execution.total_time;
	report_out.execution_overhead_time = m_execution.overhead_time;
	report_out.voice_time = m_execution.voice_time;
	report_out.fx_time = m_execution.fx_time;

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		c_wrapped_array<const s_task> tasks = m_tasks[enum_index(instrument_stage)].get_array();
		std::vector<s_profiler_report::s_task> &report_tasks = report_out.tasks[enum_index(instrument_stage)];
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
	zero_type(&m_execution.query_points);
	m_execution.stopwatch.reset();
}

void c_profiler::begin_voices() {
	m_execution.query_points[enum_index(e_execution_query_point::k_begin_voices)] = m_execution.stopwatch.query();
}

void c_profiler::end_voices() {
	m_execution.query_points[enum_index(e_execution_query_point::k_end_voices)] = m_execution.stopwatch.query();
}

void c_profiler::begin_voice() {
	m_execution.query_points[enum_index(e_execution_query_point::k_begin_voice)] = m_execution.stopwatch.query();
}

void c_profiler::end_voice() {
	m_execution.query_points[enum_index(e_execution_query_point::k_end_voice)] = m_execution.stopwatch.query();

	int64 time =
		m_execution.query_points[enum_index(e_execution_query_point::k_end_voice)] -
		m_execution.query_points[enum_index(e_execution_query_point::k_begin_voice)];
	m_execution.voice_time.update(time);
}

void c_profiler::begin_fx() {
	m_execution.query_points[enum_index(e_execution_query_point::k_begin_fx)] = m_execution.stopwatch.query();
}

void c_profiler::end_fx() {
	m_execution.query_points[enum_index(e_execution_query_point::k_end_fx)] = m_execution.stopwatch.query();

	int64 time =
		m_execution.query_points[enum_index(e_execution_query_point::k_end_fx)] -
		m_execution.query_points[enum_index(e_execution_query_point::k_begin_fx)];
	m_execution.fx_time.update(time);
}

void c_profiler::end_execution(int64 min_total_time_threshold_ns) {
	m_execution.query_points[enum_index(e_execution_query_point::k_end_execution)] = m_execution.stopwatch.query();

	int64 total_time = m_execution.query_points[enum_index(e_execution_query_point::k_end_execution)];
	if (total_time >= min_total_time_threshold_ns) {
		commit();
	} else {
		discard();
	}
}

void c_profiler::begin_task(
	e_instrument_stage instrument_stage,
	uint32 worker_thread,
	uint32 task_index,
	uint32 task_function_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[task_index];
	task.task_function_index = task_function_index;
	thread_context.stopwatch.reset();
}

void c_profiler::begin_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[task_index];
	task.query_points[enum_index(e_task_query_point::k_begin_function)] = thread_context.stopwatch.query();
}

void c_profiler::end_task_function(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[task_index];
	task.query_points[enum_index(e_task_query_point::k_end_function)] = thread_context.stopwatch.query();

}

void c_profiler::end_task(e_instrument_stage instrument_stage, uint32 worker_thread, uint32 task_index) {
	s_thread_context &thread_context = m_thread_contexts.get_array()[worker_thread];
	s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[task_index];
	task.query_points[enum_index(e_task_query_point::k_end_task)] = thread_context.stopwatch.query();
	task.did_run = true;
}

void c_profiler::commit() {
	// Commit each task
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		for (size_t index = 0; index < m_tasks[enum_index(instrument_stage)].get_array().get_count(); index++) {
			s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[index];
			commit_task(task);
		}
	}

	// Commit overall execution
	int64 total_time =
		m_execution.query_points[enum_index(e_execution_query_point::k_end_execution)];
	int64 voice_time =
		m_execution.query_points[enum_index(e_execution_query_point::k_end_voices)] -
		m_execution.query_points[enum_index(e_execution_query_point::k_begin_voices)];
	int64 fx_time =
		m_execution.query_points[enum_index(e_execution_query_point::k_end_fx)] -
		m_execution.query_points[enum_index(e_execution_query_point::k_begin_fx)];
	int64 overhead_time = total_time - voice_time - fx_time;

	m_execution.total_time.update(total_time);
	m_execution.overhead_time.update(overhead_time);
}

void c_profiler::commit_task(s_task &task) {
	if (task.did_run) {
		task.did_run = false;

		int64 total_time =
			task.query_points[enum_index(e_task_query_point::k_end_task)];
		int64 function_time =
			task.query_points[enum_index(e_task_query_point::k_end_function)] -
			task.query_points[enum_index(e_task_query_point::k_begin_function)];
		int64 overhead_time = total_time - function_time;

		task.total_time.update(total_time);
		task.function_time.update(function_time);
		task.overhead_time.update(overhead_time);
	}
}

void c_profiler::discard() {
	// Discard each task
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		for (size_t index = 0; index < m_tasks[enum_index(instrument_stage)].get_array().get_count(); index++) {
			s_task &task = m_tasks[enum_index(instrument_stage)].get_array()[index];
			discard_task(task);
		}
	}
}

void c_profiler::discard_task(s_task &task) {
	task.did_run = false;
}
