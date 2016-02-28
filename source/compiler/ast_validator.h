#ifndef WAVELANG_AST_VALIDATOR_H__
#define WAVELANG_AST_VALIDATOR_H__

#include "common/common.h"
#include "compiler/ast.h"
#include "compiler/compiler_utility.h"
#include <vector>

class c_ast_validator {
public:
	static s_compiler_result validate(const s_compiler_context *compiler_context, const c_ast_node *ast,
		std::vector<s_compiler_result> &out_errors);
};

#endif // WAVELANG_AST_VALIDATOR_H__