#include "compiler/ast.h"

const char *k_voice_entry_point_name = "voice_main";
const char *k_fx_entry_point_name = "fx_main";

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
	wl_assert(child);
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
	m_native_module_index = static_cast<uint32>(-1);
	m_return_type = c_ast_data_type(k_ast_primitive_type_void);
	m_scope = nullptr;
}

c_ast_node_module_declaration::~c_ast_node_module_declaration() {
	// For non-native modules, don't delete arguments, as they are owned by the body scope. For native modules, there is
	// no scope, so we do delete them here.
	if (m_is_native) {
		for (size_t arg = 0; arg < m_arguments.size(); arg++) {
			delete m_arguments[arg];
		}
	}

	delete m_scope;
}

void c_ast_node_module_declaration::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		// Don't iterate the arguments, because they will be iterated inside the body scope

		if (m_is_native) {
			wl_assert(!m_scope);
		} else {
			wl_assert(m_scope);
			m_scope->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_module_declaration::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		// Don't iterate the arguments, because they will be iterated inside the body scope

		if (m_is_native) {
			wl_assert(!m_scope);
		} else {
			wl_assert(m_scope);
			m_scope->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_module_declaration::set_is_native(bool is_native) {
	wl_assert(!is_native || !m_scope);
	m_is_native = is_native;
}

bool c_ast_node_module_declaration::get_is_native() const {
	return m_is_native;
}

void c_ast_node_module_declaration::set_native_module_index(uint32 native_module_index) {
	wl_assert(m_is_native);
	m_native_module_index = native_module_index;
}

uint32 c_ast_node_module_declaration::get_native_module_index() const {
	wl_assert(m_is_native);
	return m_native_module_index;
}

void c_ast_node_module_declaration::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_module_declaration::get_name() const {
	return m_name;
}

void c_ast_node_module_declaration::set_return_type(c_ast_data_type return_type) {
	wl_assert(return_type.get_primitive_type_traits().is_returnable);
	m_return_type = return_type;
}

c_ast_data_type c_ast_node_module_declaration::get_return_type() const {
	return m_return_type;
}

void c_ast_node_module_declaration::add_argument(c_ast_node_named_value_declaration *argument) {
	wl_assert(argument);
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
	wl_assert(!m_scope);
	wl_assert(scope);
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
	m_qualifier = k_ast_qualifier_none;
	m_data_type = c_ast_data_type(k_ast_primitive_type_real);
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

void c_ast_node_named_value_declaration::set_qualifier(e_ast_qualifier qualifier) {
	wl_assert(VALID_INDEX(qualifier, k_ast_qualifier_count));
	m_qualifier = qualifier;
}

e_ast_qualifier c_ast_node_named_value_declaration::get_qualifier() const {
	return m_qualifier;
}

void c_ast_node_named_value_declaration::set_data_type(c_ast_data_type data_type) {
	wl_assert(data_type.get_primitive_type_traits().is_data);
	m_data_type = data_type;
}

c_ast_data_type c_ast_node_named_value_declaration::get_data_type() const {
	return m_data_type;
}

c_ast_node_named_value_assignment::c_ast_node_named_value_assignment()
	: c_ast_node(k_ast_node_type_named_value_assignment) {
	m_is_valid_named_value = true;
	m_expression = nullptr;
	m_array_index_expression = nullptr;
}

c_ast_node_named_value_assignment::~c_ast_node_named_value_assignment() {
	delete m_expression;
	delete m_array_index_expression;
}

void c_ast_node_named_value_assignment::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		if (m_array_index_expression) {
			m_array_index_expression->iterate(visitor);
		}

		wl_assert(m_expression);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_assignment::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		if (m_array_index_expression) {
			m_array_index_expression->iterate(visitor);
		}

		wl_assert(m_expression);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_named_value_assignment::set_is_valid_named_value(bool is_valid_named_value) {
	m_is_valid_named_value = is_valid_named_value;
}

bool c_ast_node_named_value_assignment::get_is_valid_named_value() const {
	return m_is_valid_named_value;
}

void c_ast_node_named_value_assignment::set_named_value(const std::string &named_value) {
	wl_assert(m_is_valid_named_value);
	m_named_value = named_value;
}

const std::string &c_ast_node_named_value_assignment::get_named_value() const {
	return m_named_value;
}

void c_ast_node_named_value_assignment::set_array_index_expression(c_ast_node_expression *array_index_expression) {
	wl_assert(!m_array_index_expression);
	wl_assert(array_index_expression);
	wl_assert(m_is_valid_named_value);
	m_array_index_expression = array_index_expression;
}

c_ast_node_expression *c_ast_node_named_value_assignment::get_array_index_expression() {
	return m_array_index_expression;
}

const c_ast_node_expression *c_ast_node_named_value_assignment::get_array_index_expression() const {
	return m_array_index_expression;
}

void c_ast_node_named_value_assignment::set_expression(c_ast_node_expression *expression) {
	wl_assert(!m_expression);
	wl_assert(expression);
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
		wl_assert(m_expression);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_return_statement::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression);
		m_expression->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_return_statement::set_expression(c_ast_node_expression *expression) {
	wl_assert(!m_expression);
	wl_assert(expression);
	m_expression = expression;
}

c_ast_node_expression *c_ast_node_return_statement::get_expression() {
	return m_expression;
}

const c_ast_node_expression *c_ast_node_return_statement::get_expression() const {
	return m_expression;
}

c_ast_node_repeat_loop::c_ast_node_repeat_loop()
	: c_ast_node(k_ast_node_type_repeat_loop) {
	m_scope = nullptr;
	m_expression = nullptr;
}

c_ast_node_repeat_loop::~c_ast_node_repeat_loop() {
	delete m_scope;
}

void c_ast_node_repeat_loop::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		wl_assert(m_scope);
		m_scope->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_repeat_loop::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_scope);
		m_scope->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_repeat_loop::set_expression(c_ast_node_named_value_assignment *expression) {
	wl_assert(!m_expression);
	wl_assert(expression);
	m_expression = expression;
}

c_ast_node_named_value_assignment *c_ast_node_repeat_loop::get_expression() {
	return m_expression;
}

const c_ast_node_named_value_assignment *c_ast_node_repeat_loop::get_expression() const {
	return m_expression;
}

void c_ast_node_repeat_loop::set_scope(c_ast_node_scope *scope) {
	wl_assert(!m_scope);
	wl_assert(scope);
	m_scope = scope;
}

c_ast_node_scope *c_ast_node_repeat_loop::get_scope() {
	return m_scope;
}

const c_ast_node_scope *c_ast_node_repeat_loop::get_scope() const {
	return m_scope;
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
		wl_assert(m_expression_value);
		m_expression_value->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_expression::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_expression_value);
		m_expression_value->iterate(visitor);

		visitor->end_visit(this);
	}
}

void c_ast_node_expression::set_expression_value(c_ast_node *expression_value) {
	wl_assert(!m_expression_value);
	wl_assert(expression_value);
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
	m_data_type = c_ast_data_type(k_ast_primitive_type_void); // Use void to indicate unassigned/invalid
	m_real_value = 0.0f;
}

c_ast_node_constant::~c_ast_node_constant() {
	for (size_t index = 0; index < m_array_values.size(); index++) {
		delete m_array_values[index];
	}
}

void c_ast_node_constant::iterate(c_ast_node_visitor *visitor) {
	if (visitor->begin_visit(this)) {
		wl_assert(m_array_values.empty() || m_data_type.is_array());

		for (size_t index = 0; index < m_array_values.size(); index++) {
			m_array_values[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

void c_ast_node_constant::iterate(c_ast_node_const_visitor *visitor) const {
	if (visitor->begin_visit(this)) {
		wl_assert(m_array_values.empty() || m_data_type.is_array());

		for (size_t index = 0; index < m_array_values.size(); index++) {
			m_array_values[index]->iterate(visitor);
		}

		visitor->end_visit(this);
	}
}

c_ast_data_type c_ast_node_constant::get_data_type() const {
	return m_data_type;
}

void c_ast_node_constant::set_real_value(real32 value) {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_void);
	m_data_type = c_ast_data_type(k_ast_primitive_type_real);
	m_real_value = value;
	m_string_value.clear();
}

real32 c_ast_node_constant::get_real_value() const {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_real);
	return m_real_value;
}

void c_ast_node_constant::set_bool_value(bool value) {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_void);
	m_data_type = c_ast_data_type(k_ast_primitive_type_bool);
	m_bool_value = value;
	m_string_value.clear();
}

bool c_ast_node_constant::get_bool_value() const {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_bool);
	return m_bool_value;
}

void c_ast_node_constant::set_string_value(const std::string &value) {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_void);
	m_data_type = c_ast_data_type(k_ast_primitive_type_string);
	m_real_value = 0.0f;
	m_string_value = value;
}

const std::string &c_ast_node_constant::get_string_value() const {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_string);
	return m_string_value;
}

void c_ast_node_constant::set_array(c_ast_data_type element_data_type) {
	wl_assert(m_data_type.get_primitive_type() == k_ast_primitive_type_void);
	wl_assert(element_data_type.get_primitive_type_traits().is_data);
	wl_assert(!element_data_type.is_array());
	m_data_type = element_data_type.get_array_type();
}

void c_ast_node_constant::add_array_value(c_ast_node_expression *value) {
	wl_assert(m_data_type.is_array());
	m_array_values.push_back(value);
}

size_t c_ast_node_constant::get_array_count() const {
	wl_assert(m_data_type.is_array());
	return m_array_values.size();
}

c_ast_node_expression *c_ast_node_constant::get_array_value(size_t index) {
	wl_assert(m_data_type.is_array());
	return m_array_values[index];
}

const c_ast_node_expression *c_ast_node_constant::get_array_value(size_t index) const {
	wl_assert(m_data_type.is_array());
	return m_array_values[index];
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
	: c_ast_node(k_ast_node_type_module_call) {
	m_is_invoked_using_operator = false;
}

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

void c_ast_node_module_call::set_is_invoked_using_operator(bool is_invoked_using_operator) {
	m_is_invoked_using_operator = is_invoked_using_operator;
}

bool c_ast_node_module_call::get_is_invoked_using_operator() const {
	return m_is_invoked_using_operator;
}

void c_ast_node_module_call::set_name(const std::string &name) {
	m_name = name;
}

const std::string &c_ast_node_module_call::get_name() const {
	return m_name;
}

void c_ast_node_module_call::add_argument(c_ast_node_expression *argument) {
	wl_assert(argument);
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
