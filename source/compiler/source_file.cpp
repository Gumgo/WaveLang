#include "compiler/ast/node.h"
#include "compiler/source_file.h"

void s_delete_ast_node::operator()(c_ast_node *ast_node) {
	delete ast_node;
}
