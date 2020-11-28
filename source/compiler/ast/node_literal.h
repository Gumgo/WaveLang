#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

class c_AST_node_literal : public c_AST_node_expression {
public:
	AST_NODE_TYPE(c_AST_node_literal, k_literal);
	c_AST_node_literal();

	real32 get_real_value() const;
	void set_real_value(real32 value);

	bool get_bool_value() const;
	void set_bool_value(bool value);

	const char *get_string_value() const;
	void set_string_value(const char *value);

protected:
	c_AST_node *copy_internal() const override;

private:
	union {
		real32 m_real_value;
		bool m_bool_value;
	};

	std::string m_string_value;
};
