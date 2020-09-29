#pragma once

#include "common/common.h"

// This should be called every time a thread starts up. It enabled FTZ and DAZ.
void initialize_floating_point_behavior();
