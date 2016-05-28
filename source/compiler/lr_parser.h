#ifndef WAVELANG_LR_PARSER_H__
#define WAVELANG_LR_PARSER_H__

#include "common/common.h"
#include <vector>

#ifdef _DEBUG
// $TODO the language is complex enough at this point so that it takes a long time to generate the parse tables and it's
// not practical to always enable when iterating in debug mode. Can we do some sort of language rule checksum thing here
// so that we only regenerate when the checksum changes?
#define LR_PARSE_TABLE_GENERATION_ENABLED 0
#else // _DEBUG
#define LR_PARSE_TABLE_GENERATION_ENABLED 0
#endif // _DEBUG

const uint32 k_max_production_rhs_symbol_count = 10;

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

class c_lr_symbol_set {
public:
	c_lr_symbol_set();

	// These functions return true if at least one symbol was added
	bool add_symbol(const c_lr_symbol &symbol);
	bool union_with_set(const c_lr_symbol_set &symbol_set, bool ignore_epsilon = false);

	size_t get_symbol_count() const;
	c_lr_symbol get_symbol(size_t index) const;

private:
	// Could make this more efficient with a hash table or ordered set
	std::vector<c_lr_symbol> m_symbols;
};

struct s_lr_symbol_properties {
	bool is_nullable;
	c_lr_symbol_set first_set;
	c_lr_symbol_set follow_set; // Currently unused
};

struct s_lr_production {
	c_lr_symbol lhs;
	s_static_array<c_lr_symbol, k_max_production_rhs_symbol_count> rhs;

	bool rhs_index_valid(size_t index) const {
		// We stop at the first epsilon symbol
		// However, if all symbols are epsilon, we still include the first symbol
		return (index < k_max_production_rhs_symbol_count) && ((index == 0) || !rhs[index].is_epsilon());
	}
};

// Used to build up the productions for the parser
class c_lr_production_set {
public:
	c_lr_production_set();
	void initialize(uint16 terminal_count, uint16 nonterminal_count);
	void add_production(const s_lr_production &production);
	void add_production(
		c_lr_symbol lhs,
		c_lr_symbol rhs_0 = c_lr_symbol(),
		c_lr_symbol rhs_1 = c_lr_symbol(),
		c_lr_symbol rhs_2 = c_lr_symbol(),
		c_lr_symbol rhs_3 = c_lr_symbol(),
		c_lr_symbol rhs_4 = c_lr_symbol(),
		c_lr_symbol rhs_5 = c_lr_symbol(),
		c_lr_symbol rhs_6 = c_lr_symbol(),
		c_lr_symbol rhs_7 = c_lr_symbol(),
		c_lr_symbol rhs_8 = c_lr_symbol(),
		c_lr_symbol rhs_9 = c_lr_symbol());

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

#if PREDEFINED(ASSERTS_ENABLED)
	void validate_symbol(const c_lr_symbol &symbol, bool can_be_epsilon) const;
	void validate_production(const s_lr_production &production) const;
#else // PREDEFINED(ASSERTS_ENABLED)
	void validate_symbol(const c_lr_symbol &symbol, bool can_be_epsilon) const {}
	void validate_production(const s_lr_production &production) const {}
#endif // PREDEFINED(ASSERTS_ENABLED)

private:
	uint16 m_terminal_count;
	uint16 m_nonterminal_count;
	std::vector<s_lr_production> m_productions;
};

struct s_lr_item {
	size_t production_index;
	size_t pointer_index;
	c_lr_symbol lookahead;

	bool operator==(const s_lr_item &other) const;
	bool operator!=(const s_lr_item &other) const;
};

class c_lr_item_set {
public:
	c_lr_item_set();

	bool add_item(const s_lr_item &item);
	bool union_with_set(const c_lr_item_set &item_set);

	size_t get_item_count() const;
	const s_lr_item &get_item(size_t index) const;

	bool operator==(const c_lr_item_set &other) const;

private:
	// Could make this more efficient with a hash table or ordered set
	std::vector<s_lr_item> m_items;
};

struct s_lr_item_set_transition {
	static const size_t k_invalid_transition = static_cast<size_t>(-1);

	// One item set per symbol
	std::vector<size_t> item_set_indices;
};

enum e_lr_action_type {
	k_lr_action_type_invalid,
	k_lr_action_type_shift,
	k_lr_action_type_reduce,
	k_lr_action_type_accept,

	k_lr_action_type_count
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

enum e_lr_conflict {
	k_lr_conflict_none,
	k_lr_conflict_shift_reduce,
	k_lr_conflict_reduce_reduce,

	k_lr_conflict_count
};

class c_lr_action_goto_table {
public:
	static const uint32 k_invalid_state_index = static_cast<uint32>(-1);

	c_lr_action_goto_table();

	uint32 get_state_count() const;
	c_lr_action get_action(uint32 state_index, uint16 terminal_index) const;
	uint32 get_goto(uint32 state_index, uint16 nonterminal_index) const;

#if PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	void initialize(uint16 terminal_count, uint16 nonterminal_count);
	void add_state();
	e_lr_conflict set_action(uint32 state_index, uint16 terminal_index, c_lr_action action);
	void set_goto(uint32 state_index, uint16 nonterminal_index, uint32 goto_index);

	void output_action_goto_tables() const;
#else // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	void initialize(uint16 terminal_count, uint16 nonterminal_count,
		c_wrapped_array_const<c_lr_action> action_table, c_wrapped_array_const<uint32> goto_table);
#endif // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)

private:
	uint16 m_terminal_count;
	uint16 m_nonterminal_count;

#if PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	std::vector<c_lr_action> m_action_table;
	std::vector<uint32> m_goto_table;
#else // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	c_wrapped_array_const<c_lr_action> m_action_table;
	c_wrapped_array_const<uint32> m_goto_table;
#endif // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
};

class c_lr_parse_tree_node {
public:
	c_lr_symbol get_symbol() const;
	size_t get_token_index() const;
	bool has_sibling() const;
	bool has_child() const;
	size_t get_sibling_index() const;
	size_t get_child_index() const;

private:
	friend class c_lr_parse_tree;
	c_lr_symbol m_symbol;
	size_t m_token_index;
	size_t m_sibling_index;
	size_t m_child_index;
};

class c_lr_parse_tree {
public:
	static const size_t k_invalid_index = static_cast<size_t>(-1);

	c_lr_parse_tree();

	void set_root_node_index(size_t index);
	size_t get_root_node_index() const;

	size_t add_node(c_lr_symbol symbol, size_t token_index = 0);
	void make_first_child_node(size_t parent_index, size_t child_index);
	const c_lr_parse_tree_node &get_node(size_t index) const;

private:
	size_t m_root_node_index;
	std::vector<c_lr_parse_tree_node> m_nodes;
};

class c_lr_parse_tree_iterator {
public:
	c_lr_parse_tree_iterator(const c_lr_parse_tree &parse_tree, size_t node_index);

	bool is_valid() const;
	bool has_child() const;
	bool has_sibling() const;

	void follow_child();
	void follow_sibling();

	size_t get_node_index() const;
	const c_lr_parse_tree_node &get_node() const;

private:
	const c_lr_parse_tree &m_parse_tree;
	size_t m_current_node_index;
};

typedef bool (*t_lr_parser_get_next_token)(void *context, uint16 &out_token);

class c_lr_parser {
public:
#if PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	void initialize(const c_lr_production_set &production_set);
#else // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	void initialize(const c_lr_production_set &production_set,
		c_wrapped_array_const<c_lr_action> action_table, c_wrapped_array_const<uint32> goto_table);
#endif // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)

	c_lr_parse_tree parse_token_stream(t_lr_parser_get_next_token get_next_token, void *context,
		std::vector<size_t> &out_error_tokens) const;

private:
	c_lr_production_set m_production_set;
	uint16 m_end_of_input_terminal_index;

	c_lr_action_goto_table m_action_goto_table;

	void create_augmented_production_set(const c_lr_production_set &production_set);

#if PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
	uint16 m_start_nonterminal_index;
	size_t m_start_production_index;

	std::vector<s_lr_symbol_properties> m_symbol_properties_table;
	std::vector<c_lr_item_set> m_item_sets;

	void compute_symbols_properties();
	void compute_symbols_nullable();
	void compute_symbols_first_set();
	void compute_symbols_follow_set();

	bool is_symbol_string_nullable(const c_lr_symbol *str, size_t count) const;
	c_lr_symbol_set compute_symbol_string_first_set(const c_lr_symbol *str, size_t count) const;

	void compute_item_sets();
	c_lr_item_set compute_closure(const c_lr_item_set &item_set) const;
	c_lr_item_set compute_goto(const c_lr_item_set &item_set, c_lr_symbol symbol) const;
#endif // PREDEFINED(LR_PARSE_TABLE_GENERATION_ENABLED)
};

#endif // WAVELANG_LR_PARSER_H__