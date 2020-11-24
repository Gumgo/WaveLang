#pragma once

#include "common/common.h"

#include "compiler/source_file.h"

enum class e_AST_node_type {
	k_scope,

	k_scope_item,
	k_declaration,
	k_value_declaration,
	k_module_declaration,
	k_module_declaration_argument,
	k_namespace_declaration,

	k_expression,
	k_expression_placeholder,
	k_literal,
	k_array,
	k_declaration_reference,
	k_module_call,
	k_module_call_argument,
	k_convert,
	k_access,

	k_expression_statement,
	k_assignment_statement,
	k_return_statement,
	k_if_statement,
	k_for_loop,

	k_count
};

// Helper class to determine subclass type
class c_ast_node_type {
public:
	c_ast_node_type(e_AST_node_type type) {
		m_type = type;
		m_type_flags = 1u << static_cast<uint32>(type);
	}

	// Keeps the root type but adds to the type flags
	c_ast_node_type operator+(e_AST_node_type type) const {
		c_ast_node_type result(m_type);
		result.m_type_flags |= 1u << static_cast<uint32>(type);
		return result;
	}

	e_AST_node_type get_type() const {
		return m_type;
	}

	bool is_type(e_AST_node_type type) const {
		return (m_type_flags & (1u << static_cast<uint32>(type))) != 0;
	}

private:
	e_AST_node_type m_type;
	uint32 m_type_flags;
};

// $TODO $COMPILER replace _AST_ with _ast_ - this is temporary to avoid conflicts with the old AST types
class c_AST_node {
public:
	UNCOPYABLE(c_AST_node);

	virtual ~c_AST_node() = default;

	virtual const char *describe_type() const = 0;

	e_AST_node_type get_type() const {
		return m_type.get_type();
	}

	bool is_type(e_AST_node_type type) const {
		return m_type.is_type(type);
	}

	template<typename t_ast_node>
	t_ast_node *try_get_as() {
		return is_type(t_ast_node::k_ast_node_type) ? static_cast<t_ast_node *>(this) : nullptr;
	}

	template<typename t_ast_node>
	const t_ast_node *try_get_as() const {
		return is_type(t_ast_node::k_ast_node_type) ? static_cast<const t_ast_node *>(this) : nullptr;
	}

	template<typename t_ast_node>
	t_ast_node *get_as() {
		t_ast_node *result = try_get_as<t_ast_node>();
		wl_assert(result);
		return result;
	}

	template<typename t_ast_node>
	const t_ast_node *get_as() const {
		const t_ast_node *result = try_get_as<t_ast_node>();
		wl_assert(result);
		return result;
	}

	const s_compiler_source_location &get_source_location() const;
	void set_source_location(const s_compiler_source_location &source_location);

protected:
	c_AST_node(c_ast_node_type type);

private:
	c_ast_node_type m_type;
	s_compiler_source_location m_source_location;
};

template<typename t_ast_node>
t_ast_node *try_get_ast_node_as_or_null(c_AST_node *node) {
	return node ? node->try_get_as<t_ast_node>() : nullptr;
}

template<typename t_ast_node>
const t_ast_node *try_get_ast_node_as_or_null(const c_AST_node *node) {
	return node ? node->try_get_as<t_ast_node>() : nullptr;
}

template<typename t_ast_node>
t_ast_node *get_ast_node_as_or_null(c_AST_node *node) {
	return node ? node->get_as<t_ast_node>() : nullptr;
}

template<typename t_ast_node>
const t_ast_node *get_ast_node_as_or_null(const c_AST_node *node) {
	return node ? node->get_as<t_ast_node>() : nullptr;
}

// Place this at the top of all classes
#define AST_NODE_TYPE(ast_node_type) static constexpr e_AST_node_type k_ast_node_type = e_AST_node_type::ast_node_type

// Use this one to specify a user-facing type description string
#define AST_NODE_TYPE_DESCRIPTION(ast_node_type, description)						\
	virtual const char *describe_type() const override { return (description); }	\
	AST_NODE_TYPE(ast_node_type)
