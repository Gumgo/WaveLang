#ifndef WAVELANG_AST_BUILDER_H__
#define WAVELANG_AST_BUILDER_H__

#include "common\common.h"
#include "compiler\compiler_utility.h"

struct s_lexer_output;
struct s_parser_output;
class c_ast_node;

class c_ast_builder {
public:
	static c_ast_node *build_ast(const s_lexer_output &lexer_output, const s_parser_output &parser_output);
};

#endif // WAVELANG_AST_BUILDER_H__