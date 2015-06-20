#ifndef WAVELANG_PREPROCESSOR_H__
#define WAVELANG_PREPROCESSOR_H__

#include "common\common.h"
#include "compiler\compiler_utility.h"
#include <vector>

struct c_preprocessor_output {
	std::vector<c_compiler_string> imports;
};

class c_preprocessor {
public:
	static s_compiler_result preprocess(c_compiler_string source, c_preprocessor_output &output);
	static bool is_valid_preprocessor_line(c_compiler_string line);
};

#endif // WAVELANG_PREPROCESSOR_H__