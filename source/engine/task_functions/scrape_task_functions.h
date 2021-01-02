#pragma once

#include "common/common.h"

// Unfortunately, unused static lifetime variables in a static library get optimized away, so our "auto registration"
// system won't work. See https://www.bfilipek.com/2018/02/static-vars-static-lib.html for details.
void scrape_task_functions();
