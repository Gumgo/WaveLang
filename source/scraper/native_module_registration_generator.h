#pragma once

#include "common/common.h"

#include <fstream>

class c_scraper_result;

bool generate_native_module_registration(
	const c_scraper_result *result, const char *registration_function_name, std::ofstream &out);

