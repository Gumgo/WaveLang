#pragma once

#include "common/common.h"

#include "parser_generator/grammar.h"

#include <fstream>
#include <vector>

// $TODO $COMPILER a lot of this is duplicated with lr_parser.h

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
	uint32 m_index : 16;
	uint32 m_is_nonterminal : 1;
	uint32 m_is_nonepsilon : 1;
	uint32 m_padding : 14;
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
	std::vector<c_lr_symbol> rhs;
	int32 precedence;

	c_lr_symbol get_rhs_symbol_or_epsilon(size_t index) const {
		return index < rhs.size() ? rhs[index] : c_lr_symbol();
	}

	bool has_precedence() const {
		return precedence >= 0;
	}
};

// Used to build up the productions for the parser
class c_lr_production_set {
public:
	enum class e_associativity {
		k_none,
		k_left,
		k_right,
		k_nonassoc,

		k_count
	};

	c_lr_production_set();
	void initialize(uint16 terminal_count, uint16 nonterminal_count);
	void add_production(const s_lr_production &production);
	void set_terminal_associativity(uint16 terminal_index, e_associativity associativity);

	size_t get_production_count() const;
	const s_lr_production &get_production(size_t index) const;
	e_associativity get_terminal_associativity(uint16 terminal_index) const;

	uint16 get_terminal_count() const;
	uint16 get_nonterminal_count() const;

	// Includes epsilon, terminals, and nonterminals
	size_t get_total_symbol_count() const;
	size_t get_symbol_index(const c_lr_symbol &symbol) const;

	size_t get_epsilon_index() const;
	size_t get_first_terminal_index() const;
	size_t get_first_nonterminal_index() const;

#if IS_TRUE(ASSERTS_ENABLED)
	void validate_symbol(const c_lr_symbol &symbol, bool can_be_epsilon) const;
	void validate_production(const s_lr_production &production) const;
#else // IS_TRUE(ASSERTS_ENABLED)
	void validate_symbol(const c_lr_symbol &symbol, bool can_be_epsilon) const {}
	void validate_production(const s_lr_production &production) const {}
#endif // IS_TRUE(ASSERTS_ENABLED)

	void output_productions(std::ofstream &file) const;

private:
	uint16 m_terminal_count;
	uint16 m_nonterminal_count;
	std::vector<s_lr_production> m_productions;
	std::vector<e_associativity> m_terminal_associativity;
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
	uint32 m_state_or_production_index : 30;
	uint32 m_action_type : 2;
};

enum class e_lr_conflict {
	k_none,
	k_shift_reduce,
	k_reduce_reduce,

	k_count
};

class c_lr_action_goto_table {
public:
	static constexpr uint32 k_invalid_state_index = static_cast<uint32>(-1);

	c_lr_action_goto_table();

	uint32 get_state_count() const;
	c_lr_action get_action(uint32 state_index, uint16 terminal_index) const;
	uint32 get_goto(uint32 state_index, uint16 nonterminal_index) const;

	void initialize(const c_lr_production_set *production_set);
	void add_state();
	e_lr_conflict set_action(uint32 state_index, uint16 terminal_index, c_lr_action action);
	void set_goto(uint32 state_index, uint16 nonterminal_index, uint32 goto_index);

	void output_action_goto_tables(std::ofstream &file) const;

private:
	struct s_action
	{
		s_action() = default;
		s_action(c_lr_action a, uint16 t) {
			action = a;
			terminal_index = t;
		}

		c_lr_action action;
		uint16 terminal_index; // Terminal index associated with shift actions, used to resolve conflicts
	};

	s_action try_resolve_shift_reduce_conflict(
		c_lr_action shift_action,
		uint16 shift_terminal_index,
		c_lr_action reduce_action);
	s_action try_resolve_reduce_reduce_conflict(
		c_lr_action reduce_action_a,
		c_lr_action reduce_action_b);

	const c_lr_production_set *m_production_set;
	std::vector<s_action> m_action_table;
	std::vector<uint32> m_goto_table;
};

class c_lr_parser_generator {
public:
	e_lr_conflict generate_lr_parser(
		const s_grammar &grammar,
		const char *output_filename_h,
		const char *output_filename_inl);

private:
	c_lr_production_set m_production_set;
	uint16 m_end_of_input_terminal_index;
	uint16 m_start_nonterminal_index;
	size_t m_start_production_index;

	c_lr_action_goto_table m_action_goto_table;

	std::vector<s_lr_symbol_properties> m_symbol_properties_table;
	std::vector<c_lr_item_set> m_item_sets;

	void create_production_set(const s_grammar &grammar);
	void compute_symbols_properties();
	void compute_symbols_nullable();
	void compute_symbols_first_set();
	void compute_symbols_follow_set();

	bool is_symbol_string_nullable(const c_lr_symbol *str, size_t count) const;
	c_lr_symbol_set compute_symbol_string_first_set(const c_lr_symbol *str, size_t count) const;

	e_lr_conflict compute_item_sets();
	c_lr_item_set compute_closure(const c_lr_item_set &item_set) const;
	c_lr_item_set compute_goto(const c_lr_item_set &item_set, c_lr_symbol symbol) const;
};
