#ifndef WAVELANG_SCRAPER_NATIVE_MODULE_REGISTRATION_GENERATOR_H__
#define WAVELANG_SCRAPER_NATIVE_MODULE_REGISTRATION_GENERATOR_H__

#include "common/common.h"

#include <fstream>

class c_scraper_result;

bool generate_native_module_registration(
	const c_scraper_result *result, const char *registration_function_name, std::ofstream &out);

#endif // WAVELANG_SCRAPER_NATIVE_MODULE_REGISTRATION_GENERATOR_H__
