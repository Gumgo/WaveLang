#include "compiler\ast.h"

const char *k_entry_point_name = "main";

c_ast_node::c_ast_node(e_ast_node_type type)
	: m_type(type) {
	m_source_location.clear();
}

c_ast_node::~c_ast_node() {}

e_ast_node_type c_ast_node::get_type() const {
	return m_type;
}

void c_ast_node::set_source_location(const s_compiler_source_location &source_location) {
	m_source_location = source_location;
}

s_compiler_source_location c_ast_node::get_source_location() const {
	return m_source_location;
}

c_ast_node_scope::c_ast_node_scope()
	: c_ast_node(k_ast_node_type_scope) {}

c_ast_node_scope::~c_ast_node_scope() {
	for (size_t index = 0; index < m_children.size(); index++) {
		delete m_children[index];
	}
}

void c_ast_node_scope::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_children.size(); index++) {
			m_children[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_scope::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_children.size(); index++) {
			m_children[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_scope::add_child(c_ast_node *child) {
	wl_assert(child != nullptr);
	m_children.push_back(child);
}

size_t c_ast_node_scope::get_child_count() const {
	return m_children.size();
}

c_ast_node *c_ast_node_scope::get_child(size_t index) {
	return m_children[index];
}

const c_ast_node *c_ast_node_scope::get_child(size_t index) const {
	return m_children[index];
}

c_ast_node_module_declaration::c_ast_node_module_declaration()
	: c_ast_node(k_ast_node_type_module_declaration) {
	m_is_native = false;
	m_has_return_value = false;
	m_scope = nullptr;
}

c_ast_node_module_declaration::~c_ast_node_module_declaration() {
	for (size_t index = 0; index < m_arguments.size(); index++) {
		delete m_arguments[index];
	}

	delete m_scope;
}

void c_ast_node_module_declaration::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_arguments.size(); index++) {
			m_arguments[index]->iterate(visitor);
		}

		if (m_is_native) {
			wl_assert(m_scope == nullptr);
		} else {
			wl_assert(m_scope != nullptr);
			m_scope->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_module_declaration::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_arguments.size(); index++) {
			m_arguments[index]->iterate(visitor);
		}

		wl_assert(m_scope != nullptr);
		m_scope->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_module_declaration::set_is_native(bool is_native) {
	wl_assert(!is_native || m_scope == nullptr);
	m_is_native = is_native;
}

bool c_ast_node_module_declaration::get_is_native() const {
	return m_is_native;
}

void c_ast_node_module_declaration::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_module_declaration::get_name() const {
	return m_name;
}

void c_ast_node_module_declaration::set_has_return_value(bool has_return_value) {
	m_has_return_value = has_return_value;
}

bool c_ast_node_module_declaration::get_has_return_value() const {
	return m_has_return_value;
}

void c_ast_node_module_declaration::add_argument(c_ast_node_named_value_declaration *argument) {
	wl_assert(argument != nullptr);
	m_arguments.push_back(argument);
}

size_t c_ast_node_module_declaration::get_argument_count() const {
	return m_arguments.size();
}

c_ast_node_named_value_declaration *c_ast_node_module_declaration::get_argument(size_t index) {
	return m_arguments[index];
}

const c_ast_node_named_value_declaration *c_ast_node_module_declaration::get_argument(size_t index) const {
	return m_arguments[index];
}

void c_ast_node_module_declaration::set_scope(c_ast_node_scope *scope) {
	wl_assert(!m_is_native);
	wl_assert(m_scope == nullptr);
	wl_assert(scope != nullptr);
	m_scope = scope;
}

c_ast_node_scope *c_ast_node_module_declaration::get_scope() {
	return m_scope;
}

const c_ast_node_scope *c_ast_node_module_declaration::get_scope() const {
	return m_scope;
}

c_ast_node_named_value_declaration::c_ast_node_named_value_declaration()
	: c_ast_node(k_ast_node_type_named_value_declaration) {
	m_qualifier = k_qualifier_none;
}

c_ast_node_named_value_declaration::~c_ast_node_named_value_declaration() {}

void c_ast_node_named_value_declaration::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_declaration::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_declaration::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_named_value_declaration::get_name() const {
	return m_name;
}

void c_ast_node_named_value_declaration::set_qualifier(e_qualifier qualifier) {
	wl_assert(VALID_INDEX(qualifier, k_qualifier_count));
	m_qualifier = qualifier;
}

c_ast_node_named_value_declaration::e_qualifier c_ast_node_named_value_declaration::get_qualifier() const {
	return m_qualifier;
}

c_ast_node_named_value_assignment::c_ast_node_named_value_assignment()
	: c_ast_node(k_ast_node_type_assignment) {
	m_expression = nullptr;
}

c_ast_node_named_value_assignment::~c_ast_node_named_value_assignment() {
	delete m_expression;
}

void c_ast_node_named_value_assignment::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression != nullptr);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_assignment::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression != nullptr);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_assignment::set_named_value(const std::string &named_value) {
	m_named_value = named_value;
}

const std::string &c_ast_node_named_value_assignment::get_named_value() const {
	return m_named_value;
}

void c_ast_node_named_value_assignment::set_expression(c_ast_node_expression *expression) {
	wl_assert(m_expression == nullptr);
	wl_assert(expression != nullptr);
	m_expression = expression;
}

c_ast_node_expression *c_ast_node_named_value_assignment::get_expression() {
	return m_expression;
}

const c_ast_node_expression *c_ast_node_named_value_assignment::get_expression() const {
	return m_expression;
}

c_ast_node_return_statement::c_ast_node_return_statement()
	: c_ast_node(k_ast_node_type_return_statement) {
	m_expression = nullptr;
}

c_ast_node_return_statement::~c_ast_node_return_statement() {
	delete m_expression;
}

void c_ast_node_return_statement::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression != nullptr);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_return_statement::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression != nullptr);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_return_statement::set_expression(c_ast_node_expression *expression) {
	wl_assert(m_expression == nullptr);
	wl_assert(expression != nullptr);
	m_expression = expression;
}

c_ast_node_expression *c_ast_node_return_statement::get_expression() {
	return m_expression;
}

const c_ast_node_expression *c_ast_node_return_statement::get_expression() const {
	return m_expression;
}

c_ast_node_expression::c_ast_node_expression()
	: c_ast_node(k_ast_node_type_expression) {
	m_expression_value = nullptr;
}

c_ast_node_expression::~c_ast_node_expression() {
	delete m_expression_value;
}

void c_ast_node_expression::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression_value != nullptr);
		m_expression_value->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_expression::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression_value != nullptr);
		m_expression_value->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_expression::set_expression_value(c_ast_node *expression_value) {
	wl_assert(m_expression_value == nullptr);
	wl_assert(expression_value != nullptr);
	m_expression_value = expression_value;
}

c_ast_node *c_ast_node_expression::get_expression_value() {
	return m_expression_value;
}

const c_ast_node *c_ast_node_expression::get_expression_value() const {
	return m_expression_value;
}

c_ast_node_constant::c_ast_node_constant()
	: c_ast_node(k_ast_node_type_constant) {
	m_value = 0.0f;
}

c_ast_node_constant::~c_ast_node_constant() {}

void c_ast_node_constant::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_constant::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_constant::set_value(real32 value) {
	m_value = value;
}

real32 c_ast_node_constant::get_value() const {
	return m_value;
}

c_ast_node_named_value::c_ast_node_named_value()
	: c_ast_node(k_ast_node_type_named_value) {}

c_ast_node_named_value::~c_ast_node_named_value() {}

void c_ast_node_named_value::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_named_value::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		visitor->end_visit(this);
	}
}

void c_ast_node_named_value::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_named_value::get_name() const {
	return m_name;
}

c_ast_node_module_call::c_ast_node_module_call()
	: c_ast_node(k_ast_node_type_module_call) {}

c_ast_node_module_call::~c_ast_node_module_call() {
	for (size_t index = 0; index < m_arguments.size(); index++) {
		delete m_arguments[index];
	}
}

void c_ast_node_module_call::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_arguments.size(); index++) {
			m_arguments[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_module_call::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		for (size_t index = 0; index < m_arguments.size(); index++) {
			m_arguments[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_module_call::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_module_call::get_name() const {
	return m_name;
}

void c_ast_node_module_call::add_argument(c_ast_node_expression *argument) {
	wl_assert(argument != nullptr);
	m_arguments.push_back(argument);
}

size_t c_ast_node_module_call::get_argument_count() const {
	return m_arguments.size();
}

c_ast_node_expression *c_ast_node_module_call::get_argument(size_t index) {
	return m_arguments[index];
}

const c_ast_node_expression *c_ast_node_module_call::get_argument(size_t index) const {
	return m_arguments[index];
}
