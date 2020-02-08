#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

struct s_buffer_handle_identifier {};
using h_buffer = c_handle<s_buffer_handle_identifier, uint32>;
using c_buffer_handle_iterator = c_index_handle_iterator<h_buffer>;
