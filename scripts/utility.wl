module real abs(in real x) {
	return __native_abs(x);
}

module real floor(in real x) {
	return __native_floor(x);
}

module real ceil(in real x) {
	return __native_ceil(x);
}

module real round(in real x) {
	return __native_round(x);
}

module real min(in real a, in real b) {
	return __native_min(a, b);
}

module real max(in real a, in real b) {
	return __native_max(a, b);
}

module real exp(in real x) {
	return __native_exp(x);
}

module real log(in real x) {
	return __native_log(x);
}

module real sqrt(in real x) {
	return __native_sqrt(x);
}

module real pow(in real x, in real y) {
	return __native_pow(x, y);
}

module real select(in bool selector, in real true_value, in real false_value) {
	return __native_select(selector, true_value, false_value);
}

module bool select(in bool selector, in bool true_value, in bool false_value) {
	return __native_select(selector, true_value, false_value);
}

module string select(in bool selector, in string true_value, in string false_value) {
	return __native_select(selector, true_value, false_value);
}

module real pi() {
	return 3.14159265359;
}

module real e() {
	return 2.71828182846;
}

module real count(in real[] array) {
	return __native_array_count(array);
}