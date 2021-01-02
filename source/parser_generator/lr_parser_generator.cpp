#include "parser_generator/lr_parser_generator.h"
#include "parser_generator/visitor_class_generator.h"

#include <fstream>

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

c_lr_symbol_set::c_lr_symbol_set() {}

bool c_lr_symbol_set::add_symbol(const c_lr_symbol &symbol) {
	for (size_t index = 0; index < m_symbols.size(); index++) {
		if (m_symbols[index] == symbol) {
			return false;
		}
	}

	m_symbols.push_back(symbol);
	return true;
}

bool c_lr_symbol_set::union_with_set(const c_lr_symbol_set &symbol_set, bool ignore_epsilon) {
	bool result = false;
	for (size_t index = 0; index < symbol_set.m_symbols.size(); index++) {
		if (!ignore_epsilon || !symbol_set.m_symbols[index].is_epsilon()) {
			result |= add_symbol(symbol_set.m_symbols[index]);
		}
	}

	return result;
}

size_t c_lr_symbol_set::get_symbol_count() const {
	return m_symbols.size();
}

c_lr_symbol c_lr_symbol_set::get_symbol(size_t index) const {
	return m_symbols[index];
}

c_lr_production_set::c_lr_production_set() {
	m_terminal_count = 0;
	m_nonterminal_count = 0;
}

void c_lr_production_set::initialize(uint16 terminal_count, uint16 nonterminal_count) {
	m_terminal_count = terminal_count;
	m_nonterminal_count = nonterminal_count;
	m_productions.clear();
	m_terminal_precedence.resize(terminal_count, 0);
	m_terminal_associativity.resize(terminal_count, e_associativity::k_none);
}

void c_lr_production_set::add_production(const s_lr_production &production) {
	validate_production(production);
	m_productions.push_back(production);
}

void c_lr_production_set::set_terminal_precedence_and_associativity(
	uint16 terminal_index,
	int32 precedence,
	e_associativity associativity) {
	m_terminal_precedence[terminal_index] = precedence;
	m_terminal_associativity[terminal_index] = associativity;
}

size_t c_lr_production_set::get_production_count() const {
	return m_productions.size();
}

const s_lr_production &c_lr_production_set::get_production(size_t index) const {
	return m_productions[index];
}

int32 c_lr_production_set::get_terminal_precedence(uint16 terminal_index) const {
	return m_terminal_precedence[terminal_index];
}

c_lr_production_set::e_associativity c_lr_production_set::get_terminal_associativity(uint16 terminal_index) const {
	return m_terminal_associativity[terminal_index];
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

#if IS_TRUE(ASSERTS_ENABLED)
void c_lr_production_set::validate_symbol(const c_lr_symbol &symbol, bool can_be_epsilon) const {
	wl_assert((can_be_epsilon && symbol.is_epsilon())
		|| (symbol.is_terminal() && symbol.get_index() < m_terminal_count)
		|| (!symbol.is_terminal() && symbol.get_index() < m_nonterminal_count));
}

void c_lr_production_set::validate_production(const s_lr_production &production) const {
	// There should be no epsilon symbols in either the RHS or LHS
	validate_symbol(production.lhs, false);
	for (uint32 index = 0; index < production.rhs.size(); index++) {
		validate_symbol(production.rhs[index], false);
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)

void c_lr_production_set::output_productions(std::ofstream &file) const {
	for (size_t index = 0; index < m_productions.size(); index++) {
		const s_lr_production &production = m_productions[index];

		file << "static const c_lr_symbol k_production_" << index << "_rhs[] = { ";
		for (const c_lr_symbol &symbol : production.rhs) {
			file << "c_lr_symbol(";
			if (!symbol.is_epsilon()) {
				file << (symbol.is_terminal() ? "true" : "false") << ", " << symbol.get_index();
			}
			file << "), ";
		}

		file << "};\n";
	}

	file << "\n";
	file << "static const s_lr_production k_productions[] = {\n";
	for (size_t index = 0; index < m_productions.size(); index++) {
		const s_lr_production &production = m_productions[index];
		wl_assert(!production.lhs.is_epsilon());
		file << "\t{ c_lr_symbol("
			<< (production.lhs.is_terminal() ? "true" : "false") << ", " << production.lhs.get_index() << "), ";
		file << "c_wrapped_array<const c_lr_symbol>::construct(k_production_" << index << "_rhs) }, \n";
	}

	file << "};\n\n";
}

bool s_lr_item::operator==(const s_lr_item &other) const {
	return production_index == other.production_index
		&& pointer_index == other.pointer_index
		&& lookahead == other.lookahead;
}

bool s_lr_item::operator!=(const s_lr_item &other) const {
	return !(*this == other);
}

c_lr_item_set::c_lr_item_set() {}

bool c_lr_item_set::add_item(const s_lr_item &item) {
	for (size_t index = 0; index < m_items.size(); index++) {
		if (item == m_items[index]) {
			return false;
		}
	}

	m_items.push_back(item);
	return true;
}

bool c_lr_item_set::union_with_set(const c_lr_item_set &item_set) {
	bool result = false;
	for (size_t index = 0; index < item_set.m_items.size(); index++) {
		result |= add_item(item_set.m_items[index]);
	}

	return result;
}

size_t c_lr_item_set::get_item_count() const {
	return m_items.size();
}

const s_lr_item &c_lr_item_set::get_item(size_t index) const {
	return m_items[index];
}

bool c_lr_item_set::operator==(const c_lr_item_set &other) const {
	// n^2 compare - improve this if it turns out to be too slow
	if (m_items.size() != other.m_items.size()) {
		return false;
	}

	// Because the sets have no duplicates, then the sets are equal if each item in A is also in B

	for (size_t index_a = 0; index_a < m_items.size(); index_a++) {
		bool found = false;
		for (size_t index_b = 0; !found && index_b < other.m_items.size(); index_b++) {
			if (m_items[index_a] == other.m_items[index_b]) {
				found = true;
			}
		}

		if (!found) {
			return false;
		}
	}

	return true;
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
	m_production_set = nullptr;
}

uint32 c_lr_action_goto_table::get_state_count() const {
	return static_cast<uint32>(m_action_table.size() / m_production_set->get_terminal_count());
}

c_lr_action c_lr_action_goto_table::get_action(uint32 state_index, uint16 terminal_index) const {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(terminal_index, m_production_set->get_terminal_count()));
	size_t index = state_index * m_production_set->get_terminal_count() + terminal_index;
	return m_action_table[index].action;
}

uint32 c_lr_action_goto_table::get_goto(uint32 state_index, uint16 nonterminal_index) const {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(nonterminal_index, m_production_set->get_nonterminal_count()));
	size_t index = state_index * m_production_set->get_nonterminal_count() + nonterminal_index;
	return m_goto_table[index];
}

void c_lr_action_goto_table::initialize(const c_lr_production_set *production_set) {
	m_production_set = production_set;
}

void c_lr_action_goto_table::add_state() {
	m_action_table.resize(m_action_table.size() + m_production_set->get_terminal_count());
	m_goto_table.resize(m_goto_table.size() + m_production_set->get_nonterminal_count(), k_invalid_state_index);
}

s_lr_conflict c_lr_action_goto_table::set_action(uint32 state_index, uint16 terminal_index, c_lr_action action) {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(terminal_index, m_production_set->get_terminal_count()));
	wl_assert(action.get_action_type() != e_lr_action_type::k_invalid);
	size_t index = state_index * m_production_set->get_terminal_count() + terminal_index;

	s_action resolved_action;
	c_lr_action current_action = m_action_table[index].action;
	if (action == current_action) {
		resolved_action.action = action;
		resolved_action.terminal_index = terminal_index;
	} else if (action.get_action_type() == e_lr_action_type::k_shift
		&& current_action.get_action_type() == e_lr_action_type::k_reduce) {
		// Solve shift-reduce conflict
		resolved_action = try_resolve_shift_reduce_conflict(action, terminal_index, current_action);
		if (resolved_action.action.get_action_type() == e_lr_action_type::k_invalid) {
			s_lr_conflict result;
			result.conflict = e_lr_conflict::k_shift_reduce;
			result.terminal_index = terminal_index;
			result.production_index = current_action.get_reduce_production_index();
			return result;
		}
	} else if (action.get_action_type() == e_lr_action_type::k_reduce
		&& current_action.get_action_type() == e_lr_action_type::k_shift) {
		// Solve shift-reduce conflict
		resolved_action =
			try_resolve_shift_reduce_conflict(current_action, m_action_table[index].terminal_index, action);
		if (resolved_action.action.get_action_type() == e_lr_action_type::k_invalid) {
			s_lr_conflict result;
			result.conflict = e_lr_conflict::k_shift_reduce;
			result.terminal_index = m_action_table[index].terminal_index;
			result.production_index = action.get_reduce_production_index();
			return result;
		}
	} else if (action.get_action_type() == e_lr_action_type::k_reduce
		&& current_action.get_action_type() == e_lr_action_type::k_reduce) {
		resolved_action = try_resolve_reduce_reduce_conflict(action, current_action);
		if (resolved_action.action.get_action_type() == e_lr_action_type::k_invalid) {
			s_lr_conflict result;
			result.conflict = e_lr_conflict::k_reduce_reduce;
			result.production_index_a = action.get_reduce_production_index();
			result.production_index_b = current_action.get_reduce_production_index();
			return result;
		}
	} else {
		// Accept should never conflict, and shift-shift should never occur
		wl_assert(current_action.get_action_type() == e_lr_action_type::k_invalid);
		resolved_action.action = action;
		resolved_action.terminal_index = terminal_index;
	}

	m_action_table[index] = resolved_action;
	return { e_lr_conflict::k_none };
}

void c_lr_action_goto_table::set_goto(uint32 state_index, uint16 nonterminal_index, uint32 goto_index) {
	wl_assert(valid_index(state_index, get_state_count()));
	wl_assert(valid_index(nonterminal_index, m_production_set->get_nonterminal_count()));
	size_t index = state_index * m_production_set->get_nonterminal_count() + nonterminal_index;
	m_goto_table[index] = goto_index;
}

void c_lr_action_goto_table::output_action_goto_tables(std::ofstream &file) const {
	file << "static const c_lr_action k_lr_action_table[] = {\n";
	for (size_t index = 0; index < m_action_table.size(); index++) {
		const c_lr_action &action = m_action_table[index].action;

		e_lr_action_type action_type = action.get_action_type();
		const char *action_type_string = nullptr;
		uint32 action_index = 0;

		switch (action_type) {
		case e_lr_action_type::k_invalid:
			action_type_string = "e_lr_action_type::k_invalid";
			break;

		case e_lr_action_type::k_shift:
			action_type_string = "e_lr_action_type::k_shift";
			action_index = action.get_shift_state_index();
			break;

		case e_lr_action_type::k_reduce:
			action_type_string = "e_lr_action_type::k_reduce";
			action_index = action.get_reduce_production_index();
			break;

		case e_lr_action_type::k_accept:
			action_type_string = "e_lr_action_type::k_accept";
			break;

		default:
			wl_unreachable();
		}

		file << "\tc_lr_action(" << action_type_string << ", " << action_index << "),\n";
	}
	file << "};\n\n";

	file << "static const uint32 k_lr_goto_table[] = {\n";
	for (size_t index = 0; index < m_goto_table.size(); index++) {
		uint32 goto_index = m_goto_table[index];
		if (goto_index == k_invalid_state_index) {
			file << "\tc_lr_action_goto_table::k_invalid_state_index,\n";
		} else {
			file << "\t" << goto_index << ",\n";
		}
	}
	file << "};\n\n";
}

c_lr_action_goto_table::s_action c_lr_action_goto_table::try_resolve_shift_reduce_conflict(
	c_lr_action shift_action,
	uint16 shift_terminal_index,
	c_lr_action reduce_action)
{
	c_lr_production_set::e_associativity shift_terminal_associativity =
		m_production_set->get_terminal_associativity(shift_terminal_index);
	int32 shift_terminal_precedence = shift_terminal_associativity == c_lr_production_set::e_associativity::k_none
		? -1
		: m_production_set->get_terminal_precedence(shift_terminal_index);
	int32 reduce_precedence = m_production_set->get_production(reduce_action.get_reduce_production_index()).precedence;

	// If either lacks precedence info, we have a conflict
	if (shift_terminal_precedence >= 0 && reduce_precedence >= 0) {
		if (shift_terminal_precedence > reduce_precedence) {
			// Shift wins
			return s_action(shift_action, shift_terminal_index);
		} else if (shift_terminal_precedence < reduce_precedence) {
			// Reduce wins
			return s_action(reduce_action, 0);
		} else {
			wl_assert(shift_terminal_precedence == reduce_precedence);
			if (shift_terminal_associativity == c_lr_production_set::e_associativity::k_right) {
				// Shift wins
				return s_action(shift_action, shift_terminal_index);
			} else if (shift_terminal_associativity == c_lr_production_set::e_associativity::k_left) {
				// Reduce wins
				return s_action(reduce_action, 0);
			}
		}
	}

	return s_action();
}

c_lr_action_goto_table::s_action c_lr_action_goto_table::try_resolve_reduce_reduce_conflict(
	c_lr_action reduce_action_a,
	c_lr_action reduce_action_b)
{
	// If either production lacks precedence info or if both have the same precedence, it's a conflict. Otherwise,
	// return the action with the highest precedence.
	const s_lr_production &production_a =
		m_production_set->get_production(reduce_action_a.get_reduce_production_index());
	const s_lr_production &production_b =
		m_production_set->get_production(reduce_action_b.get_reduce_production_index());
	if (production_a.has_precedence()
		&& production_b.has_precedence()
		&& production_a.precedence != production_b.precedence) {
		return s_action(production_a.precedence > production_b.precedence ? reduce_action_a : reduce_action_b, 0);
	}

	return s_action();
}

s_lr_conflict c_lr_parser_generator::generate_lr_parser(
	const s_grammar &grammar,
	const char *output_filename_h,
	const char *output_filename_inl) {
	create_production_set(grammar);
	compute_symbols_properties();
	s_lr_conflict conflict = compute_item_sets();
	if (conflict.conflict != e_lr_conflict::k_none) {
		return conflict;
	}

	c_visitor_class_generator visitor_class_generator(grammar);

	{
		std::ofstream file(output_filename_h);
		file << "#pragma once\n\n";

		file << "#include \"common/common.h\"\n\n";

		file << "#include \"compiler/lr_parser.h\"\n\n";

		for (const std::string &include : grammar.includes) {
			file << "#include \"" << include << "\"\n";
		}

		if (!grammar.includes.empty()) {
			file << "\n";
		}

		file << "#include <variant>\n";
		file << "#include <vector>\n\n";

		file << "// THIS FILE WAS AUTOMATICALLY GENERATED - DO NOT EDIT\n\n";

		file << "enum class " << grammar.terminal_type_name << " : uint16 {\n";
		for (const s_grammar::s_terminal &terminal : grammar.terminals) {
			file << "\t" << grammar.terminal_value_prefix << terminal.value << ",\n";
		}
		file << "\n";
		file << "\tk_count\n";
		file << "};\n\n";

		file << "enum class " << grammar.nonterminal_type_name << " : uint16 {\n";
		for (const s_grammar::s_nonterminal &nonterminal : grammar.nonterminals) {
			file << "\t" << grammar.nonterminal_value_prefix << nonterminal.value << ",\n";
		}
		file << "\n";
		file << "\tk_count\n";
		file << "};\n\n";

		visitor_class_generator.generate_class_declaration(file);
		file << "\n";

		file << "const char *get_" << grammar.grammar_name << "_terminal_string("
			<< grammar.terminal_type_name << " terminal);\n";
		file << "void initialize_" << grammar.grammar_name << "_parser(c_lr_parser &parser);\n\n";

		// $TODO could restrict these to non-release builds
		file << "const char *get_" << grammar.grammar_name << "_terminal_reflection_string("
			<< grammar.terminal_type_name << " terminal);\n";
		file << "const char *get_" << grammar.grammar_name << "_nonterminal_reflection_string("
			<< grammar.nonterminal_type_name << " nonterminal);\n";
	}

	{
		std::ofstream file(output_filename_inl);

		file << "// THIS FILE WAS AUTOMATICALLY GENERATED - DO NOT EDIT\n\n";

		m_action_goto_table.output_action_goto_tables(file);
		m_production_set.output_productions(file);

		file << "static constexpr const char *k_terminal_strings[] = {\n";
		for (const s_grammar::s_terminal &terminal : grammar.terminals) {
			if (terminal.string.empty()) {
				file << "\tnullptr,\n";
			} else {
				file << "\t\"" << terminal.string.c_str() << "\",\n";
			}
		}
		file << "};\n\n";

		visitor_class_generator.generate_class_definition(file);
		file << "\n";

		file << "const char *get_" << grammar.grammar_name << "_terminal_string("
			<< grammar.terminal_type_name << " terminal) {\n";
		file << "\twl_assert(valid_enum_index(terminal));\n";
		file << "\treturn k_terminal_strings[enum_index(terminal)];\n";
		file << "}\n\n";

		file << "void initialize_" << grammar.grammar_name << "_parser(c_lr_parser &parser) {\n";
		file << "\tparser.initialize(\n";
		file << "\t\t" << m_production_set.get_terminal_count() << ",\n";
		file << "\t\t" << m_production_set.get_nonterminal_count() << ",\n";
		file << "\t\t" << m_end_of_input_terminal_index << ",\n";
		file << "\t\tc_wrapped_array<const s_lr_production>::construct(k_productions),\n";
		file << "\t\tc_wrapped_array<const c_lr_action>::construct(k_lr_action_table),\n";
		file << "\t\tc_wrapped_array<const uint32>::construct(k_lr_goto_table));\n";
		file << "}\n\n";

		file << "static constexpr const char *k_terminal_reflection_strings[] = {\n";
		for (const s_grammar::s_terminal &terminal : grammar.terminals) {
			std::string value = terminal.value;
			for (size_t i = 0; i < value.size(); i++) {
				if (value[i] == '_') {
					value[i] = '-';
				}
			}
			file << "\t\"" << value << "\",\n";
		}
		file << "};\n\n";

		file << "static constexpr const char *k_nonterminal_reflection_strings[] = {\n";
		for (const s_grammar::s_nonterminal &nonterminal : grammar.nonterminals) {
			std::string value = nonterminal.value;
			for (size_t i = 0; i < value.size(); i++) {
				if (value[i] == '_') {
					value[i] = '-';
				}
			}
			file << "\t\"" << value << "\",\n";
		}
		file << "};\n\n";

		file << "const char *get_" << grammar.grammar_name << "_terminal_reflection_string("
			<< grammar.terminal_type_name << " terminal) {\n";
		file << "\twl_assert(valid_enum_index(terminal));\n";
		file << "\treturn k_terminal_reflection_strings[enum_index(terminal)];\n";
		file << "}\n\n";

		file << "const char *get_" << grammar.grammar_name << "_nonterminal_reflection_string("
			<< grammar.nonterminal_type_name << " nonterminal) {\n";
		file << "\twl_assert(valid_enum_index(nonterminal));\n";
		file << "\treturn k_nonterminal_reflection_strings[enum_index(nonterminal)];\n";
		file << "}\n";
	}

	return { e_lr_conflict::k_none };
}

void c_lr_parser_generator::create_production_set(const s_grammar &grammar) {
	// Build the augmented grammar - add an additional end-of-input terminal, a start nonterminal, and a start rule
	m_end_of_input_terminal_index = cast_integer_verify<uint16>(grammar.terminals.size());
	m_start_nonterminal_index = cast_integer_verify<uint16>(grammar.nonterminals.size());
	m_start_production_index = grammar.rules.size();

	m_production_set.initialize(
		cast_integer_verify<uint16>(grammar.terminals.size() + 1),
		cast_integer_verify<uint16>(grammar.nonterminals.size() + 1));

	for (size_t terminal_index = 0; terminal_index < grammar.terminals.size(); terminal_index++) {
		STATIC_ASSERT(enum_count<s_grammar::e_associativity>() == enum_count<c_lr_production_set::e_associativity>());
		c_lr_production_set::e_associativity associativity =
			static_cast<c_lr_production_set::e_associativity>(grammar.terminals[terminal_index].associativity);
		m_production_set.set_terminal_precedence_and_associativity(
			cast_integer_verify<uint16>(terminal_index),
			grammar.terminals[terminal_index].precedence,
			associativity);
	}

	for (const s_grammar::s_rule &rule : grammar.rules) {
		s_lr_production production;
		production.precedence = -1;
		production.lhs = c_lr_symbol(false, cast_integer_verify<uint16>(rule.nonterminal_index));

		if (!rule.precedence_override_terminal.empty()) {
			production.precedence = grammar.terminals[rule.precedence_override_terminal_index].precedence;
		}

		for (const s_grammar::s_rule_component &component : rule.components) {
			production.rhs.push_back(
				c_lr_symbol(
					component.is_terminal,
					cast_integer_verify<uint16>(component.terminal_or_nonterminal_index)));

			// Precedence of a production is determined by the leftmost terminal that has associativity data
			if (rule.precedence_override_terminal.empty() && !production.has_precedence() && component.is_terminal) {
				const s_grammar::s_terminal &terminal = grammar.terminals[component.terminal_or_nonterminal_index];
				if (terminal.associativity != s_grammar::e_associativity::k_none) {
					production.precedence = terminal.precedence;
				}
			}
		}

		if (production.rhs.empty()) {
			// Add epsilon symbol to indicate that this is an empty rule
			production.rhs.push_back(c_lr_symbol());
		}

		m_production_set.add_production(production);
	}

	// Add the start rule, which produces the first user-defined nonterminal
	s_lr_production start_production;
	start_production.precedence = -1;
	start_production.lhs = c_lr_symbol(false, cast_integer_verify<uint16>(m_start_nonterminal_index));
	start_production.rhs.push_back(c_lr_symbol(false, 0));
	m_production_set.add_production(start_production);
}

void c_lr_parser_generator::compute_symbols_properties() {
	// Calculate properties for all the symbols
	m_symbol_properties_table.resize(m_production_set.get_total_symbol_count());
	compute_symbols_nullable();
	compute_symbols_first_set();
	//compute_symbols_follow_set(); // Unused, we're doing LR(1) and not SLR(1)
}

void c_lr_parser_generator::compute_symbols_nullable() {
	// The nullable symbols are ones which are capable of producing an empty string (epsilon)

	// Initialize all symbols to non-nullable
	for (size_t index = 0; index < m_production_set.get_total_symbol_count(); index++) {
		m_symbol_properties_table[index].is_nullable = false;
	}

	// For each production which yields epsilon, the symbol on the LHS is nullable
	for (size_t index = 0; index < m_production_set.get_production_count(); index++) {
		const s_lr_production &production = m_production_set.get_production(index);
		if (production.rhs[0].is_epsilon()) {
			size_t lhs_index = m_production_set.get_symbol_index(production.lhs);
			m_symbol_properties_table[lhs_index].is_nullable = true;
		}
	}

	// Now, for each production, if all symbols on the RHS are nullable, then the LHS is also nullable
	// Repeat this until nothing changes
	bool found_new_nullable_symbol;
	do {
		found_new_nullable_symbol = false;

		for (size_t index = 0; index < m_production_set.get_production_count(); index++) {
			const s_lr_production &production = m_production_set.get_production(index);

			// Don't bother checking if the production's LHS is already nullable
			size_t lhs_symbol_index = m_production_set.get_symbol_index(production.lhs);

			if (!m_symbol_properties_table[lhs_symbol_index].is_nullable) {
				bool all_nullable = true;
				// Search the RHS to find any non-nullable symbols
				for (size_t rhs_index = 0; all_nullable && rhs_index < production.rhs.size(); rhs_index++) {
					size_t rhs_symbol_index = m_production_set.get_symbol_index(production.rhs[rhs_index]);
					if (!m_symbol_properties_table[rhs_symbol_index].is_nullable) {
						all_nullable = false;
					}
				}

				if (all_nullable) {
					m_symbol_properties_table[lhs_symbol_index].is_nullable = true;
					found_new_nullable_symbol = true;
				}
			}
		}
	} while (found_new_nullable_symbol);
}

void c_lr_parser_generator::compute_symbols_first_set() {
	// The first set for a symbol X is the set of terminals which could be the first to appear from a derivation of X

	// The first set of each terminal is just that terminal
	m_symbol_properties_table[m_production_set.get_epsilon_index()].first_set.add_symbol(c_lr_symbol());
	for (uint16 index = 0; index < m_production_set.get_terminal_count(); index++) {
		c_lr_symbol symbol(true, index);
		m_production_set.validate_symbol(symbol, false);

		size_t symbol_index = m_production_set.get_first_terminal_index() + index;
		m_symbol_properties_table[symbol_index].first_set.add_symbol(symbol);
	}

	// The nonterminal first sets are initially empty

	// Now repeat the following until none of the first sets change:
	// For each production, if the first N symbols on the RHS are nullable, union the LHS symbol's first set with the
	// (N+1)th's RHS symbol's first set. e.g. in the production X = YZ, if Y is capable of deriving epsilon, then it
	// must be the case that the first symbol derived from Z can also be the first symbol derived from X.
	bool first_set_changed;
	do {
		first_set_changed = false;

		for (size_t index = 0; index < m_production_set.get_production_count(); index++) {
			const s_lr_production &production = m_production_set.get_production(index);

			size_t lhs_symbol_index = m_production_set.get_symbol_index(production.lhs);

			for (size_t rhs_index = 0; rhs_index < production.rhs.size(); rhs_index++) {
				// This loop will always iterate on at least the 0th element. If the production yields epsilon, then we
				// will add epsilon to the LHS's first set.

				size_t rhs_symbol_index = m_production_set.get_symbol_index(production.rhs[rhs_index]);
				first_set_changed |= m_symbol_properties_table[lhs_symbol_index].first_set.union_with_set(
					m_symbol_properties_table[rhs_symbol_index].first_set);

				// So far up to this point, all RHS symbols have been nullable. This means the string of symbols (RHS_0,
				// ..., RHS_(i-1)) is nullable. Now check if this RHS symbol is nullable. If not, then for any t >= i,
				// the string (RHS_0, ..., RHS_t) is also not nullable, so we can terminate the loop.
				if (!m_symbol_properties_table[rhs_symbol_index].is_nullable) {
					break;
				}
			}
		}
	} while (first_set_changed);
}

void c_lr_parser_generator::compute_symbols_follow_set() {
	// Initially all follow sets are empty
	// The idea here is for each symbol S, we want to find all the possible terminals that can follow it. e.g. if we
	// have the production X = Yt, then t can follow Y. (This is a trivial example.)
	// We keep repeating the following algorithm until nothing changes: Loop through each production. First, for each
	// nonterminal symbol X in the RHS, we can add the first set of the remaining symbol string in the production. This
	// is because the first set contains all symbols which could first be produced by this string, so therefore those
	// symbols can follow X. Next, for each nonterminal X, check if the remaining symbol string is nullable. If so, this
	// means that whatever can follow the remaining (nullable) string can also follow X, and this set is equal to the
	// follow set of the LHS, so union that into the follow set of X.
	bool follow_set_changed;
	do {
		follow_set_changed = false;

		for (size_t index = 0; index < m_production_set.get_production_count(); index++) {
			const s_lr_production &production = m_production_set.get_production(index);

			size_t lhs_symbol_index = m_production_set.get_symbol_index(production.lhs);

			size_t rhs_count = production.rhs.size();
			for (size_t rhs_index = 0; rhs_index < rhs_count - 1; rhs_index++) {
				if (!production.rhs[rhs_index].is_terminal()) {
					// Union with the first set of the remainder of the RHS. This is because the first symbol in the
					// remaining symbol string must be able to follow this current symbol.
					size_t remaining_index = rhs_index + 1;
					c_lr_symbol_set remaining_first_set = compute_symbol_string_first_set(
						&production.rhs[remaining_index], rhs_count - remaining_index);

					size_t rhs_symbol_index = m_production_set.get_symbol_index(production.rhs[rhs_index]);
					follow_set_changed |= m_symbol_properties_table[rhs_symbol_index].follow_set.union_with_set(
						remaining_first_set, true);
				}
			}

			// Iterate from count-1 to 0
			for (size_t rhs_index_reverse = 0; rhs_index_reverse < rhs_count; rhs_index_reverse++) {
				size_t rhs_index = rhs_count - rhs_index_reverse - 1;
				size_t rhs_symbol_index = m_production_set.get_symbol_index(production.rhs[rhs_index]);

				size_t remaining_index = rhs_index + 1;
				if (remaining_index < production.rhs.size()) {
					size_t remaining_symbol_index = m_production_set.get_symbol_index(production.rhs[remaining_index]);
					if (!m_symbol_properties_table[remaining_symbol_index].is_nullable) {
						// This symbol is not nullable, so from this point on in the loop, the remainder of the symbol
						// string is not nullable.
						break;
					}
				}

				// All remaining symbols up to this point have been nullable, and so is this one, so the remaining
				// symbol string as a whole must be nullable
				if (!production.rhs[rhs_index].is_terminal()) {
					// Union the RHS symbol follow set with the LHS symbol follow set. This is because the remaining
					// symbols are all nullable, so whatever can follow the LHS can also follow this symbol.
					follow_set_changed |= m_symbol_properties_table[rhs_symbol_index].follow_set.union_with_set(
						m_symbol_properties_table[lhs_symbol_index].follow_set);
				}
			}
		}
	} while (follow_set_changed);
}

bool c_lr_parser_generator::is_symbol_string_nullable(const c_lr_symbol *str, size_t count) const {
	for (size_t index = 0; index < count; index++) {
		size_t symbol_index = m_production_set.get_symbol_index(str[index]);
		if (!m_symbol_properties_table[symbol_index].is_nullable) {
			return false;
		}
	}

	return true;
}

c_lr_symbol_set c_lr_parser_generator::compute_symbol_string_first_set(const c_lr_symbol *str, size_t count) const {
	c_lr_symbol_set result;

	for (size_t index = 0; index < count; index++) {
		size_t symbol_index = m_production_set.get_symbol_index(str[index]);
		result.union_with_set(m_symbol_properties_table[symbol_index].first_set);

		if (!m_symbol_properties_table[symbol_index].is_nullable) {
			break;
		}
	}

	return result;
}

s_lr_conflict c_lr_parser_generator::compute_item_sets() {
	m_action_goto_table.initialize(&m_production_set);

	// Create the initial item set
	s_lr_item start_item;
	start_item.production_index = m_start_production_index;
	start_item.pointer_index = 0;
	start_item.lookahead = c_lr_symbol(true, m_end_of_input_terminal_index);
	c_lr_item_set start_item_set;
	start_item_set.add_item(start_item);
	m_item_sets.push_back(compute_closure(start_item_set));
	m_action_goto_table.add_state();
	wl_assert(m_item_sets.size() == m_action_goto_table.get_state_count());

	// Loop over each item set and terminal/nonterminal combination
	// This will continue to loop as long as new items sets keep being added
	for (uint32 item_set_index = 0; item_set_index < m_item_sets.size(); item_set_index++) {
		const c_lr_item_set *item_set = &m_item_sets[item_set_index];

		for (int32 is_terminal_index = 0; is_terminal_index < 2; is_terminal_index++) {
			bool is_terminal = (is_terminal_index == 0);

			uint16 symbol_count = is_terminal
				? m_production_set.get_terminal_count()
				: m_production_set.get_nonterminal_count();

			for (uint16 index = 0; index < symbol_count; index++) {
				c_lr_symbol symbol(is_terminal, index);
				m_production_set.validate_symbol(symbol, false);

				c_lr_item_set goto_item_set = compute_goto(*item_set, symbol);

				uint32 match_index = c_lr_action_goto_table::k_invalid_state_index;
				if (goto_item_set.get_item_count() > 0) {
					// Check if this item set equals any other item set
					for (match_index = 0; match_index < m_item_sets.size(); match_index++) {
						if (m_item_sets[match_index] == goto_item_set) {
							break;
						}
					}

					if (match_index == m_item_sets.size()) {
						// We didn't match any existing item set, so add a new one
						m_item_sets.push_back(goto_item_set);
						// Refresh the pointer
						item_set = &m_item_sets[item_set_index];
						m_action_goto_table.add_state();
						wl_assert(m_item_sets.size() == m_action_goto_table.get_state_count());
					}

					// Assign the action
					if (is_terminal) {
						for (size_t item_index = 0; item_index < item_set->get_item_count(); item_index++) {
							const s_lr_item &item = item_set->get_item(item_index);
							const s_lr_production &item_production =
								m_production_set.get_production(item.production_index);
							c_lr_symbol symbol_after_pointer =
								item_production.get_rhs_symbol_or_epsilon(item.pointer_index);

							if (symbol_after_pointer == symbol) {
								s_lr_conflict conflict = m_action_goto_table.set_action(
									item_set_index,
									symbol.get_index(),
									c_lr_action(e_lr_action_type::k_shift, match_index));

								if (conflict.conflict != e_lr_conflict::k_none) {
									return conflict;
								}
							}
						}
					}
				}

				// Assign the goto
				if (!is_terminal) {
					m_action_goto_table.set_goto(item_set_index, index, match_index);
				}
			}
		}

		// Assign reduce and accept actions
		for (size_t item_index = 0; item_index < item_set->get_item_count(); item_index++) {
			const s_lr_item &item = item_set->get_item(item_index);
			const s_lr_production &item_production = m_production_set.get_production(item.production_index);
			c_lr_symbol symbol_after_pointer = item_production.get_rhs_symbol_or_epsilon(item.pointer_index);

			if (symbol_after_pointer.is_epsilon()) {
				// The production is of the form [A -> B., a]
				// Check if this is the augmented start rule; if so, accept
				if (item_production.lhs == c_lr_symbol(false, m_start_nonterminal_index)) {
					// Verify that the rest of this rule is what we expect
					wl_assert(item.pointer_index == 1);
					wl_assert(item_production.rhs[0] == c_lr_symbol(false, 0)); // Should point to the first "real" rule
					wl_assert(item.lookahead == c_lr_symbol(true, m_end_of_input_terminal_index));

					s_lr_conflict conflict = m_action_goto_table.set_action(
						item_set_index,
						m_end_of_input_terminal_index,
						c_lr_action(e_lr_action_type::k_accept));

					if (conflict.conflict != e_lr_conflict::k_none) {
						return conflict;
					}
				} else {
					// This is a reduce action
					s_lr_conflict conflict = m_action_goto_table.set_action(
						item_set_index,
						item.lookahead.get_index(),
						c_lr_action(e_lr_action_type::k_reduce, cast_integer_verify<uint32>(item.production_index)));

					if (conflict.conflict != e_lr_conflict::k_none) {
						return conflict;
					}
				}
			}
		}
	}

	wl_assert(m_item_sets.size() == m_action_goto_table.get_state_count());
	return { e_lr_conflict::k_none };
}

c_lr_item_set c_lr_parser_generator::compute_closure(const c_lr_item_set &item_set) const {
	c_lr_item_set result = item_set;

	bool item_set_changed;
	do {
		item_set_changed = false;

		// For each item in the set...
		for (size_t item_index = 0; item_index < result.get_item_count(); item_index++) {
			const s_lr_item *item = &result.get_item(item_index);
			const s_lr_production &item_production = m_production_set.get_production(item->production_index);

			bool first_after_pointer_nonterminal_computed = false;
			c_lr_symbol_set first_after_pointer_nonterminal;

			// Does this item's pointer lie to the left of a nonterminal?
			c_lr_symbol pointer_symbol = item_production.get_rhs_symbol_or_epsilon(item->pointer_index);
			if (!pointer_symbol.is_terminal()) {
				// Find each production with an LHS of this nonterminal
				for (size_t production_index = 0;
					production_index < m_production_set.get_production_count();
					production_index++) {
					// Skip if this production's LHS doesn't match
					const s_lr_production &production = m_production_set.get_production(production_index);
					if (production.lhs != pointer_symbol) {
						continue;
					}

					// Lazily compute the first set of the symbols following the nonterminal at the pointer
					if (!first_after_pointer_nonterminal_computed) {
						std::vector<c_lr_symbol> symbols_after_pointer_nonterminal;
						for (size_t i = item->pointer_index + 1; i < item_production.rhs.size(); i++) {
							// We should never hit an epsilon in the remainder of the RHS
							wl_assert(!item_production.rhs[i].is_epsilon());
							symbols_after_pointer_nonterminal.push_back(item_production.rhs[i]);
						}

						// Add the lookahead symbol
						symbols_after_pointer_nonterminal.push_back(item->lookahead);

						first_after_pointer_nonterminal = compute_symbol_string_first_set(
							symbols_after_pointer_nonterminal.data(),
							symbols_after_pointer_nonterminal.size());

						first_after_pointer_nonterminal_computed = true;
					}

					// For each symbol in the first set of the remaining symbols followed by the lookahead...
					for (size_t first_index = 0;
						first_index < first_after_pointer_nonterminal.get_symbol_count();
						first_index++) {
						// Construct an item consisting of the production with the pointer before the first symbol and
						// the lookahead symbol from the first set
						s_lr_item new_item;
						new_item.production_index = production_index;
						new_item.pointer_index = 0;
						new_item.lookahead = first_after_pointer_nonterminal.get_symbol(first_index);
						item_set_changed |= result.add_item(new_item);
						// Refresh the item pointer since it may have changed
						item = &result.get_item(item_index);
					}
				}
			}
		}
	} while (item_set_changed);

	return result;
}

c_lr_item_set c_lr_parser_generator::compute_goto(const c_lr_item_set &item_set, c_lr_symbol symbol) const {
	c_lr_item_set result;

	// goto(s, x) is the state the parser would reach if it recognized x while in state s
	// It doesn't make sense for x to be epsilon
	wl_assert(!symbol.is_epsilon());

	// Find all items in the item set for which the pointer is before an instance of the given symbol
	for (size_t item_index = 0; item_index < item_set.get_item_count(); item_index++) {
		const s_lr_item &item = item_set.get_item(item_index);
		const s_lr_production &item_production = m_production_set.get_production(item.production_index);

		if (item_production.get_rhs_symbol_or_epsilon(item.pointer_index) != symbol) {
			// Symbol after pointer does not match
			continue;
		}

		// "Advance" the pointer
		s_lr_item new_item = item;
		new_item.pointer_index++;
		result.add_item(new_item);
	}

	// Finally, compute the closure
	c_lr_item_set closed_result = compute_closure(result);
	return closed_result;
}
