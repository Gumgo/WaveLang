#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"
#include "compiler/ast/node_module_call_argument.h"
#include "compiler/ast/node_module_declaration.h"

#include <memory>
#include <vector>

class c_ast_node_module_call : public c_ast_node_expression {
public:
	AST_NODE_TYPE(c_ast_node_module_call, k_module_call);
	c_ast_node_module_call();

	void add_argument(const c_ast_node_module_call_argument *argument);
	size_t get_argument_count() const;
	c_ast_node_module_call_argument *get_argument(size_t index) const;

	c_ast_node_module_declaration *get_resolved_module_declaration() const;
	void set_resolved_module_declaration(
		c_ast_node_module_declaration *resolved_module_declaration,
		e_ast_data_mutability dependent_constant_output_mutability);

	// These arguments correspond to the module declaration, not the module call site
	c_ast_node_expression *get_resolved_module_argument_expression(size_t argument_index) const;

protected:
	c_ast_node *copy_internal() const override;

private:
	std::vector<std::unique_ptr<c_ast_node_module_call_argument>> m_arguments;
	c_ast_node_module_declaration *m_resolved_module_declaration = nullptr;
};