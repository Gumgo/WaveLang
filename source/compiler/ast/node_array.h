#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

#include <memory>
#include <vector>

class c_ast_node_array : public c_ast_node_expression {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_array, k_array, "array");
	c_ast_node_array();

	void set_data_type(const c_ast_qualified_data_type &data_type);

	void add_element(c_ast_node_expression *element);
	size_t get_element_count() const;
	c_ast_node_expression *get_element(size_t index) const;

protected:
	c_ast_node *copy_internal() const override;

private:
	std::vector<std::unique_ptr<c_ast_node_expression>> m_elements;
};
