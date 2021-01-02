#pragma once

#include "common/common.h"

#include <vector>

class c_lr_symbol {
public:
	c_lr_symbol(); // Epsilon initializer
	c_lr_symbol(bool terminal, uint16 index);

	bool is_epsilon() const;
	bool is_terminal() const;
	uint16 get_index() const;

	bool operator==(const c_lr_symbol &other) const;
	bool operator!=(const c_lr_symbol &other) const;

private:
	// We lay out the bitfield like this so that epsilon is all zeros
	// This allows epsilon to return true for is_terminal
	uint32 m_index			: 16;
	uint32 m_is_nonterminal	: 1;
	uint32 m_is_nonepsilon	: 1;
	uint32 m_padding		: 14;
};

struct s_lr_production {
	c_lr_symbol lhs;
	c_wrapped_array<const c_lr_symbol> rhs;
};

// Used to build up the productions for the parser
class c_lr_production_set {
public:
	c_lr_production_set();
	void initialize(
		uint16 terminal_count,
		uint16 nonterminal_count,
		c_wrapped_array<const s_lr_production> productions);

	size_t get_production_count() const;
	const s_lr_production &get_production(size_t index) const;

	uint16 get_terminal_count() const;
	uint16 get_nonterminal_count() const;

	// Includes epsilon, terminals, and nonterminals
	size_t get_total_symbol_count() const;
	size_t get_symbol_index(const c_lr_symbol &symbol) const;

	size_t get_epsilon_index() const;
	size_t get_first_terminal_index() const;
	size_t get_first_nonterminal_index() const;

private:
	uint16 m_terminal_count;
	uint16 m_nonterminal_count;
	c_wrapped_array<const s_lr_production> m_productions;
};

enum class e_lr_action_type {
	k_invalid,
	k_shift,
	k_reduce,
	k_accept,

	k_count
};

class c_lr_action {
public:
	c_lr_action();
	c_lr_action(e_lr_action_type action_type, uint32 index = 0);

	e_lr_action_type get_action_type() const;
	uint32 get_shift_state_index() const;
	uint32 get_reduce_production_index() const;

	bool operator==(const c_lr_action &other) const;
	bool operator!=(const c_lr_action &other) const;

private:
	uint32 m_state_or_production_index	: 30;
	uint32 m_action_type				: 2;
};

class c_lr_action_goto_table {
public:
	static constexpr uint32 k_invalid_state_index = static_cast<uint32>(-1);

	c_lr_action_goto_table();

	uint32 get_state_count() const;
	c_lr_action get_action(uint32 state_index, uint16 terminal_index) const;
	uint32 get_goto(uint32 state_index, uint16 nonterminal_index) const;

	void initialize(
		uint16 terminal_count,
		uint16 nonterminal_count,
		c_wrapped_array<const c_lr_action> action_table,
		c_wrapped_array<const uint32> goto_table);

private:
	uint16 m_terminal_count;
	uint16 m_nonterminal_count;

	c_wrapped_array<const c_lr_action> m_action_table;
	c_wrapped_array<const uint32> m_goto_table;
};

class c_lr_parse_tree_node {
public:
	c_lr_symbol get_symbol() const;
	size_t get_token_index() const;
	size_t get_production_index() const;
	bool has_sibling() const;
	bool has_child() const;
	size_t get_sibling_index() const;
	size_t get_child_index() const;

private:
	friend class c_lr_parse_tree;
	c_lr_symbol m_symbol;
	size_t m_token_or_production_index;
	size_t m_sibling_index;
	size_t m_child_index;
};

class c_lr_parse_tree {
public:
	static constexpr size_t k_invalid_index = static_cast<size_t>(-1);

	c_lr_parse_tree();

	void set_root_node_index(size_t index);
	size_t get_root_node_index() const;

	size_t add_terminal_node(c_lr_symbol symbol, size_t token_index);
	size_t add_nonterminal_node(c_lr_symbol symbol, size_t production_index);
	void make_first_child_node(size_t parent_index, size_t child_index);
	const c_lr_parse_tree_node &get_node(size_t index) const;
	size_t get_node_count() const;

private:
	size_t add_node(c_lr_symbol symbol, size_t token_or_production_index);

	size_t m_root_node_index;
	std::vector<c_lr_parse_tree_node> m_nodes;
};

class c_lr_parse_tree_visitor {
public:
	c_lr_parse_tree_visitor(const c_lr_parse_tree &parse_tree);
	virtual void visit();

protected:
	virtual bool enter_node(size_t node_index) = 0;
	virtual void exit_node(size_t node_index, bool call_exit) = 0;

	void get_child_node_indices(size_t node_index, std::vector<size_t> &child_node_indices_out) const;

	const c_lr_parse_tree &m_parse_tree;
};

using f_lr_parser_get_next_token = bool (*)(void *context, uint16 &token_out);

class c_lr_parser {
public:
	void initialize(
		uint16 terminal_count,
		uint16 nonterminal_count,
		uint16 end_of_production_terminal_index,
		c_wrapped_array<const s_lr_production> productions,
		c_wrapped_array<const c_lr_action> action_table,
		c_wrapped_array<const uint32> goto_table);

	c_lr_parse_tree parse_token_stream(
		f_lr_parser_get_next_token get_next_token,
		void *context,
		std::vector<size_t> &error_tokens_out) const;

private:
	c_lr_production_set m_production_set;
	uint16 m_end_of_input_terminal_index;

	c_lr_action_goto_table m_action_goto_table;
};
