#pragma once

#include "common/common.h"

#include <cstdio>

// $TODO replace std::cout and std::cerr with these. Do we want to add report_warning?
#define report_message(format, ...) fprintf_s(stdout, format "\n", __VA_ARGS__)
#define report_error(format, ...) fprintf_s(stderr, format "\n", __VA_ARGS__)
