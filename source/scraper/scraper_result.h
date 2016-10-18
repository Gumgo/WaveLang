#ifndef WAVELANG_SCRAPER_SCRAPER_RESULT_H__
#define WAVELANG_SCRAPER_SCRAPER_RESULT_H__

#include "common/common.h"

#include "engine/task_function.h"

#include "execution_graph/native_module.h"

#include <string>
#include <vector>

struct s_library_declaration {
	std::string id_string;
	std::string name;
};

struct s_native_module_argument_declaration {
	std::string name;
	c_native_module_qualified_data_type type;
	bool is_return_value;
};

struct s_native_module_declaration {
	std::string uid_string;
	std::string identifier; // This takes the form "module_name$overload_identifier", e.g. "addition$real"
	std::string name;
	std::string native_operator;
	std::vector<s_native_module_argument_declaration> arguments;
	std::string compile_time_call;
};

struct s_optimization_rule_declaration {
	std::vector<std::string> tokens;
};

struct s_task_function_declaration {
	std::string name;
};

// Collects results from the scraper - native modules and task functions
class c_scraper_result {
public:
	c_scraper_result();

	void add_library(const s_library_declaration &library);
	void add_native_module(const s_native_module_declaration &native_module);
	void add_optimization_rule(const s_optimization_rule_declaration &optimization_rule);
	void add_task_function(const s_task_function_declaration &task_function);

	size_t get_library_count() const;
	const s_library_declaration &get_library(size_t index) const;

	size_t get_native_module_count() const;
	const s_native_module_declaration &get_native_module(size_t index) const;

	size_t get_optimization_rule_count() const;
	const s_optimization_rule_declaration &get_optimization_rule(size_t index) const;

	size_t get_task_function_count() const;
	const s_task_function_declaration &get_task_function(size_t index) const;

	bool get_success() const;
	void set_success(bool success);

private:
	std::vector<s_library_declaration> m_library_declarations;
	std::vector<s_native_module_declaration> m_native_module_declarations;
	std::vector<s_optimization_rule_declaration> m_optimization_rule_declarations;
	std::vector<s_task_function_declaration> m_task_function_declarations;
	bool m_success;
};

#endif // WAVELANG_SCRAPER_SCRAPER_RESULT_H__
