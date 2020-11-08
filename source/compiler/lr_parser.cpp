#include "compiler/lr_parser.h"

#include <limits>
#include <stack>

c_lr_symbol::c_lr_symbol() {
	m_index = 0;
	m_is_nonterminal = 0;
	m_is_nonepsilon = 0;
	m_padding = 0;
}

c_lr_symbol::c_lr_symbol(bool terminal, uint16 index) {
	m_index = index;
	m_is_nonterminal = !terminal;
	m_is_nonepsilon = 1;
	m_padding = 0;
}

bool c_lr_symbol::is_epsilon() const {
	return !m_is_nonepsilon;
}

bool c_lr_symbol::is_terminal() const {
	return !m_is_nonterminal;
}

uint16 c_lr_symbol::get_index() const {
	return cast_integer_verify<uint16>(m_index);
}

bool c_lr_symbol::operator==(const c_lr_symbol &other) const {
	return m_index == other.m_index
		&& m_is_nonterminal == other.m_is_nonterminal
		&& m_is_nonepsilon == other.m_is_nonepsilon;
}

bool c_lr_symbol::operator!=(const c_lr_symbol &other) const {
	return !(*this == other);
}

c_lr_production_set::c_lr_production_set() {
	m_terminal_count = 0;
	m_nonterminal_count = 0;
}

void c_lr_production_set::initialize(
	uint16 terminal_count,
	uint16 nonterminal_count,
	c_wrapped_array<const s_lr_production> productions) {
	m_terminal_count = terminal_count;
	m_nonterminal_count = nonterminal_count;
	m_productions = productions;
}

size_t c_lr_production_set::get_production_count() const {
	return m_productions.get_count();
}

const s_lr_production &c_lr_production_set::get_production(size_t index) const {
	return m_productions[index];
}

uint16 c_lr_production_set::get_terminal_count() const {
	return m_terminal_count;
}

uint16 c_lr_production_set::get_nonterminal_count() const {
	return m_nonterminal_count;
}

size_t c_lr_production_set::get_total_symbol_count() const {
	return 1 + m_terminal_count + m_nonterminal_count;
}

size_t c_lr_production_set::get_symbol_index(const c_lr_symbol &symbol) const {
	if (symbol.is_epsilon()) {
		return 0;
	} else if (symbol.is_terminal()) {
		return 1 + symbol.get_index();
	} else {
		return 1 + m_terminal_count + symbol.get_index();
	}
}

size_t c_lr_production_set::get_epsilon_index() const {
	return 0;
}

size_t c_lr_production_set::get_first_terminal_index() const {
	return 1;
}

size_t c_lr_production_set::get_first_nonterminal_index() const {
	return 1 + m_terminal_count;
}

c_lr_action::c_lr_action() {
	m_state_or_production_index = 0;
	m_action_type = static_cast<uint32>(e_lr_action_type::k_invalid);
}

c_lr_action::c_lr_action(e_lr_action_type action_type, uint32 index) {
	wl_assert(valid_enum_index(action_type));
	if (action_type == e_lr_action_type::k_invalid
		|| action_type == e_lr_action_type::k_accept) {
		// These actions don't have an index
		wl_assert(index == 0);
	}

	// Index must fit in 30 bits
	wl_assert(index < (2u << 30));

	m_state_or_production_index = index;
	m_action_type = static_cast<uint32>(action_type);
}

e_lr_action_type c_lr_action::get_action_type() const {
	return static_cast<e_lr_action_type>(m_action_type);
}

uint32 c_lr_action::get_shift_state_index() const {
	wl_assert(get_action_type() == e_lr_action_type::k_shift);
	return m_state_or_production_index;
}

uint32 c_lr_action::get_reduce_production_index() const {
	wl_assert(get_action_type() == e_lr_action_type::k_reduce);
	return m_state_or_production_index;
}

bool c_lr_action::operator==(const c_lr_action &other) const {
	return m_state_or_production_index == other.m_state_or_production_index
		&& m_action_type == other.m_action_type;
}

bool c_lr_action::operator!=(const c_lr_action &other) const {
	return !(*this == other);
}

c_lr_action_goto_table::c_lr_action_goto_table() {
	m_terminal_count = 0;
	m_nonterminal_count = 0;
}

uint32 c_lr_action_goto_table::get_state_count() const {
	return static_cast<uint32>(m_action_table.get_count() / m_terminal_count);
}


c_lr_action c_lr_action_goto_table::get_action(uint32 state_index, uint16 terminal_index) const {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(terminal_index, m_terminal_count));
	size_t index = state_index * m_terminal_count + terminal_index;
	return m_action_table[index];
}

uint32 c_lr_action_goto_table::get_goto(uint32 state_index, uint16 nonterminal_index) const {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(nonterminal_index, m_nonterminal_count));
	size_t index = state_index * m_nonterminal_count + nonterminal_index;
	return m_goto_table[index];
}

void c_lr_action_goto_table::initialize(
	uint16 terminal_count,
	uint16 nonterminal_count,
	c_wrapped_array<const c_lr_action> action_table,
	c_wrapped_array<const uint32> goto_table) {
	m_terminal_count = terminal_count;
	m_nonterminal_count = nonterminal_count;

	m_action_table = action_table;
	m_goto_table = goto_table;
}

c_lr_symbol c_lr_parse_tree_node::get_symbol() const {
	return m_symbol;
}

size_t c_lr_parse_tree_node::get_token_index() const {
	wl_assert(m_symbol.is_terminal());
	return m_token_or_production_index;
}

size_t c_lr_parse_tree_node::get_production_index() const {
	wl_assert(!m_symbol.is_terminal());
	return m_token_or_production_index;
}

bool c_lr_parse_tree_node::has_sibling() const {
	return m_sibling_index != c_lr_parse_tree::k_invalid_index;
}

bool c_lr_parse_tree_node::has_child() const {
	return m_child_index != c_lr_parse_tree::k_invalid_index;
}

size_t c_lr_parse_tree_node::get_sibling_index() const {
	return m_sibling_index;
}

size_t c_lr_parse_tree_node::get_child_index() const {
	return m_child_index;
}

c_lr_parse_tree::c_lr_parse_tree() {
	m_root_node_index = k_invalid_index;
}

void c_lr_parse_tree::set_root_node_index(size_t index) {
	wl_assert(valid_index(index, m_nodes.size()));
	m_root_node_index = index;
}

size_t c_lr_parse_tree::get_root_node_index() const {
	return m_root_node_index;
}

size_t c_lr_parse_tree::add_terminal_node(c_lr_symbol symbol, size_t token_index) {
	wl_assert(symbol.is_terminal());
	return add_node(symbol, token_index);
}

size_t c_lr_parse_tree::add_nonterminal_node(c_lr_symbol symbol, size_t production_index) {
	wl_assert(!symbol.is_terminal());
	return add_node(symbol, production_index);
}

void c_lr_parse_tree::make_first_child_node(size_t parent_index, size_t child_index) {
	wl_assert(m_nodes[child_index].m_sibling_index == k_invalid_index);

	// Move the current first child to be the next sibling
	m_nodes[child_index].m_sibling_index = m_nodes[parent_index].m_child_index;
	m_nodes[parent_index].m_child_index = child_index;
}

const c_lr_parse_tree_node &c_lr_parse_tree::get_node(size_t index) const {
	return m_nodes[index];
}

size_t c_lr_parse_tree::get_node_count() const {
	return m_nodes.size();
}

size_t c_lr_parse_tree::add_node(c_lr_symbol symbol, size_t token_or_production_index) {
	c_lr_parse_tree_node node;
	node.m_symbol = symbol;
	node.m_token_or_production_index = token_or_production_index;
	node.m_sibling_index = k_invalid_index;
	node.m_child_index = k_invalid_index;
	size_t index = m_nodes.size();
	m_nodes.push_back(node);
	return index;
}

c_lr_parse_tree_visitor::c_lr_parse_tree_visitor(const c_lr_parse_tree &parse_tree)
	: m_parse_tree(parse_tree) {
}

void c_lr_parse_tree_visitor::visit() {
	struct s_node_state {
		size_t node_index;
		bool entered;
	};
	std::vector<s_node_state> node_stack;
	node_stack.push_back({ m_parse_tree.get_root_node_index(), false });
	while (!node_stack.empty()) {
		s_node_state &node_state = node_stack.back();
		if (!node_state.entered) {
			bool enter_child_nodes = enter_node(node_state.node_index);
			node_state.entered = true;
			if (enter_child_nodes && m_parse_tree.get_node(node_state.node_index).has_child()) {
				node_stack.push_back({ m_parse_tree.get_node(node_state.node_index).get_child_index() });
			}
		} else {
			exit_node(node_state.node_index);
			if (m_parse_tree.get_node(node_state.node_index).has_sibling()) {
				node_state = { m_parse_tree.get_node(node_state.node_index).get_sibling_index(), false };
			} else {
				node_stack.pop_back();
			}
		}
	}
}

void c_lr_parse_tree_visitor::get_child_node_indices(
	size_t node_index,
	std::vector<size_t> &child_node_indices_out) const {
	child_node_indices_out.clear();

	size_t next_child_node_index = m_parse_tree.get_node(node_index).get_child_index();
	while (next_child_node_index != c_lr_parse_tree::k_invalid_index) {
		child_node_indices_out.push_back(next_child_node_index);
		next_child_node_index = m_parse_tree.get_node(next_child_node_index).get_sibling_index();
	}
}

c_lr_parse_tree_iterator::c_lr_parse_tree_iterator(const c_lr_parse_tree &parse_tree, size_t node_index)
	: m_parse_tree(parse_tree)
	, m_current_node_index(node_index) {
}

bool c_lr_parse_tree_iterator::is_valid() const {
	return m_current_node_index != c_lr_parse_tree::k_invalid_index;
}

bool c_lr_parse_tree_iterator::has_child() const {
	return get_node().get_child_index() != c_lr_parse_tree::k_invalid_index;
}

bool c_lr_parse_tree_iterator::has_sibling() const {
	return get_node().get_sibling_index() != c_lr_parse_tree::k_invalid_index;
}

void c_lr_parse_tree_iterator::follow_child() {
	m_current_node_index = get_node().get_child_index();
}

void c_lr_parse_tree_iterator::follow_sibling() {
	m_current_node_index = get_node().get_sibling_index();
}

size_t c_lr_parse_tree_iterator::get_node_index() const {
	return m_current_node_index;
}

const c_lr_parse_tree_node &c_lr_parse_tree_iterator::get_node() const {
	wl_assert(is_valid());
	return m_parse_tree.get_node(m_current_node_index);
}

void c_lr_parser::initialize(
	uint16 terminal_count,
	uint16 nonterminal_count,
	uint16 end_of_production_terminal_index,
	c_wrapped_array<const s_lr_production> productions,
	c_wrapped_array<const c_lr_action> action_table,
	c_wrapped_array<const uint32> goto_table) {
	m_end_of_input_terminal_index = end_of_production_terminal_index;
	m_production_set.initialize(
		terminal_count,
		nonterminal_count,
		productions);
	m_action_goto_table.initialize(
		terminal_count,
		nonterminal_count,
		action_table,
		goto_table);
}

c_lr_parse_tree c_lr_parser::parse_token_stream(
	f_lr_parser_get_next_token get_next_token,
	void *context,
	std::vector<size_t> &error_tokens_out) const {
	c_lr_parse_tree result_tree;

	struct s_stack_element {
		uint32 state;
		size_t node_index;
	};

	std::stack<s_stack_element> state_stack;
	{
		s_stack_element initial_stack_element;
		initial_stack_element.state = 0;
		initial_stack_element.node_index = c_lr_parse_tree::k_invalid_index;
		state_stack.push(initial_stack_element);
	}

	size_t current_token_index = 0;
	uint16 current_token;
	if (!get_next_token(context, current_token)) {
		current_token = m_end_of_input_terminal_index;
	}

	bool done = false;
	while (!done) {
		wl_assert(valid_index(current_token, m_production_set.get_terminal_count()));

		s_stack_element stack_top = state_stack.top();
		c_lr_action action = m_action_goto_table.get_action(stack_top.state, current_token);

		if (action.get_action_type() == e_lr_action_type::k_invalid) {
			// Error. $TODO Implement error-recovery so we can detect multiple errors, but for now just return early
			error_tokens_out.push_back(current_token_index);
			done = true;
		} else if (action.get_action_type() == e_lr_action_type::k_shift) {
			// Push the new state, read next token
			s_stack_element stack_element;
			stack_element.state = action.get_shift_state_index();
			stack_element.node_index = result_tree.add_terminal_node(
				c_lr_symbol(true, current_token),
				current_token_index);
			state_stack.push(stack_element);

			current_token_index++;
			if (!get_next_token(context, current_token)) {
				current_token = m_end_of_input_terminal_index;
			}
		} else if (action.get_action_type() == e_lr_action_type::k_reduce) {
			uint32 production_index = action.get_reduce_production_index();
			const s_lr_production &production = m_production_set.get_production(production_index);

			// Pop n times and connect the tree
			size_t parent_node_index = result_tree.add_nonterminal_node(production.lhs, production_index);

			if (!production.rhs[0].is_epsilon()) {
				for (size_t rhs_index = 0; rhs_index < production.rhs.get_count(); rhs_index++) {
					s_stack_element stack_element = state_stack.top();
					result_tree.make_first_child_node(parent_node_index, stack_element.node_index);
					state_stack.pop();
				}
			}

			stack_top = state_stack.top();
			s_stack_element stack_element;
			stack_element.state = m_action_goto_table.get_goto(stack_top.state, production.lhs.get_index());
			stack_element.node_index = parent_node_index;
			state_stack.push(stack_element);
		} else if (action.get_action_type() == e_lr_action_type::k_accept) {
			result_tree.set_root_node_index(stack_top.node_index);
			done = true;
		} else {
			// Unknown action type?
			wl_unreachable();
		}
	}

	return result_tree;
}
