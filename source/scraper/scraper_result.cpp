#include "scraper/scraper_result.h"

c_scraper_result::c_scraper_result() {
	m_success = true;
}

void c_scraper_result::add_library(const s_library_declaration &library) {
	m_library_declarations.push_back(library);
}

void c_scraper_result::add_native_module(const s_native_module_declaration &native_module) {
	m_native_module_declarations.push_back(native_module);
}

void c_scraper_result::add_optimization_rule(const s_optimization_rule_declaration &optimization_rule) {
	m_optimization_rule_declarations.push_back(optimization_rule);
}

void c_scraper_result::add_task_memory_query(const s_task_memory_query_declaration &task_memory_query) {
	m_task_memory_query_declarations.push_back(task_memory_query);
}

void c_scraper_result::add_task_initializer(const s_task_initializer_declaration &task_initializer) {
	m_task_initializer_declarations.push_back(task_initializer);
}

void c_scraper_result::add_task_voice_initializer(const s_task_voice_initializer_declaration &task_voice_initializer) {
	m_task_voice_initializer_declarations.push_back(task_voice_initializer);
}

void c_scraper_result::add_task_function(const s_task_function_declaration &task_function) {
	m_task_function_declarations.push_back(task_function);
}

size_t c_scraper_result::get_library_count() const {
	return m_library_declarations.size();
}

const s_library_declaration &c_scraper_result::get_library(size_t index) const {
	return m_library_declarations[index];
}

size_t c_scraper_result::get_native_module_count() const {
	return m_native_module_declarations.size();
}
const s_native_module_declaration &c_scraper_result::get_native_module(size_t index) const {
	return m_native_module_declarations[index];
}

size_t c_scraper_result::get_optimization_rule_count() const {
	return m_optimization_rule_declarations.size();
}

const s_optimization_rule_declaration &c_scraper_result::get_optimization_rule(size_t index) const {
	return m_optimization_rule_declarations[index];
}

size_t c_scraper_result::get_task_memory_query_count() const {
	return m_task_memory_query_declarations.size();
}

const s_task_memory_query_declaration &c_scraper_result::get_task_memory_query(size_t index) const {
	return m_task_memory_query_declarations[index];
}

size_t c_scraper_result::get_task_initializer_count() const {
	return m_task_initializer_declarations.size();
}

const s_task_initializer_declaration &c_scraper_result::get_task_initializer(size_t index) const {
	return m_task_initializer_declarations[index];
}

size_t c_scraper_result::get_task_voice_initializer_count() const {
	return m_task_voice_initializer_declarations.size();
}

const s_task_voice_initializer_declaration &c_scraper_result::get_task_voice_initializer(size_t index) const {
	return m_task_voice_initializer_declarations[index];
}

size_t c_scraper_result::get_task_function_count() const {
	return m_task_function_declarations.size();
}

const s_task_function_declaration &c_scraper_result::get_task_function(size_t index) const {
	return m_task_function_declarations[index];
}

bool c_scraper_result::get_success() const {
	return m_success;
}

void c_scraper_result::set_success(bool success) {
	m_success = success;
}
